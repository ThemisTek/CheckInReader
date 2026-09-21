#include "api_client.h"
#include "device_config.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif

namespace {

ScanOutcome mapOutcome(int ordinal) {
    switch (ordinal) {
        case 0: return ScanOutcome::CheckedIn;
        case 1: return ScanOutcome::AlreadyCheckedIn;
        case 2: return ScanOutcome::ChooseSession;
        case 3: return ScanOutcome::NoSession;
        case 4: return ScanOutcome::UnknownCard;
        case 5: return ScanOutcome::PairingBound;
        case 6: return ScanOutcome::PairingAlreadyBound;
        default: return ScanOutcome::Unknown;
    }
}

}  // namespace

ScanResult scanCard(const String& uid) {
    ScanResult result;

    HTTPClient http;
    http.begin(deviceConfig::apiBaseUrl() + "/api/companies/" + deviceConfig::companyId() + "/check-in/scan");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Api-Key", deviceConfig::apiKey());

    JsonDocument requestDoc;
    requestDoc["card"] = uid;
    String body;
    serializeJson(requestDoc, body);

    int status = http.POST(body);
    if (status != HTTP_CODE_OK) {
        result.message = "Scan failed (HTTP " + String(status) + ")";
        http.end();
        return result;
    }

    // getString() undoes chunked transfer-encoding (common for small ASP.NET Core JSON
    // responses); getStream() hands back the raw socket, chunk framing and all, which
    // deserializeJson() cannot parse as JSON.
    String payload = http.getString();
    http.end();

    JsonDocument responseDoc;
    DeserializationError error = deserializeJson(responseDoc, payload);

    if (error) {
        result.message = "Scan failed (bad response)";
        return result;
    }

    result.outcome = mapOutcome(responseDoc["outcome"] | -1);

    const char* message = responseDoc["message"] | "Scan failed (no message)";
    const char* scannedCard = responseDoc["scannedCard"] | "";
    const char* attendanceId = responseDoc["attendanceId"] | "";
    result.message = message;
    result.scannedCard = scannedCard;
    result.attendanceId = attendanceId;

    if (!responseDoc["student"].isNull()) {
        const char* firstName = responseDoc["student"]["firstName"] | "";
        result.studentFirstName = firstName;
    }

    return result;
}

bool deleteAttendance(const String& attendanceId) {
    HTTPClient http;
    http.begin(deviceConfig::apiBaseUrl() + "/api/companies/" + deviceConfig::companyId() +
               "/check-in/attendance/" + attendanceId);
    http.addHeader("X-Api-Key", deviceConfig::apiKey());

    int status = http.sendRequest("DELETE");
    http.end();

    return status == 204 || status == 404;
}

DevicePairingStart startDevicePairing(const String& ssid) {
    DevicePairingStart result;

    JsonDocument requestDoc;
    requestDoc["firmwareVersion"] = FIRMWARE_VERSION;
    requestDoc["ssid"] = ssid;
    String body;
    serializeJson(requestDoc, body);

    HTTPClient http;
    http.begin(deviceConfig::apiBaseUrl() + "/api/device-pairing/start");
    http.addHeader("Content-Type", "application/json");
    result.httpStatus = http.POST(body);
    if (result.httpStatus != HTTP_CODE_OK) {
        http.end();
        return result;
    }
    String payload = http.getString();
    http.end();

    JsonDocument responseDoc;
    if (deserializeJson(responseDoc, payload)) {
        return result;
    }

    const char* code = responseDoc["code"] | "";
    const char* pollToken = responseDoc["pollToken"] | "";
    result.code = code;
    result.pollToken = pollToken;
    result.expiresInSeconds = responseDoc["expiresInSeconds"] | 0;
    result.ok = result.code.length() > 0 && result.pollToken.length() > 0 && result.expiresInSeconds > 0;
    return result;
}

DevicePairingPoll pollDevicePairing(const String& pollToken) {
    DevicePairingPoll result;

    HTTPClient http;
    http.begin(deviceConfig::apiBaseUrl() + "/api/device-pairing/" + pollToken);
    int status = http.GET();
    if (status != HTTP_CODE_OK) {
        http.end();
        return result;  // Error: a network blip, not an answer
    }
    String payload = http.getString();
    http.end();

    JsonDocument responseDoc;
    if (deserializeJson(responseDoc, payload)) {
        return result;
    }

    switch (responseDoc["status"] | -1) {
        case 0:
            result.state = DevicePairingState::Pending;
            break;
        case 2:
            result.state = DevicePairingState::Expired;
            break;
        case 1: {
            const char* companyId = responseDoc["companyId"] | "";
            const char* companyName = responseDoc["companyName"] | "";
            const char* deviceName = responseDoc["deviceName"] | "";
            const char* apiKey = responseDoc["apiKey"] | "";
            result.companyId = companyId;
            result.companyName = companyName;
            result.deviceName = deviceName;
            result.apiKey = apiKey;
            // The server has already forgotten the key, so a Claimed reply without one cannot be retried.
            result.state = (result.apiKey.length() > 0 && result.companyId.length() > 0)
                               ? DevicePairingState::Claimed
                               : DevicePairingState::Error;
            break;
        }
        default:
            break;  // stays Error
    }
    return result;
}
