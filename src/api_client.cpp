#include "api_client.h"
#include "config.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

ScanResult scanCard(const String& uid) {
    ScanResult result;

    HTTPClient http;
    http.begin(String(API_BASE_URL) + "/api/companies/" + COMPANY_ID + "/check-in/scan");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Api-Key", API_KEY);

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

    const char* message = responseDoc["message"] | "Scan failed (no message)";
    const char* scannedCard = responseDoc["scannedCard"] | "";
    result.message = message;
    result.scannedCard = scannedCard;
    return result;
}
