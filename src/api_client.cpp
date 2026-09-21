#include "api_client.h"
#include "device_config.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

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
