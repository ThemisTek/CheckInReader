#include "api_client.h"

ScanResult scanCard(const String& uid) {
    // TODO: HTTPClient POST to API_BASE_URL + "/api/companies/{id}/check-in/scan"
    // with header "X-Api-Key: " API_KEY and body {"card": uid}, then parse
    // ScanResultDto.Message out of the JSON response (see check-in-reader.md).
    ScanResult result;
    result.message = "scanCard() not yet implemented (uid=" + uid + ")";
    return result;
}
