#pragma once

#include <Arduino.h>

// Mirrors the fields of Contracts/CheckIn/ScanContracts.cs -> ScanResultDto
// that the firmware actually renders. Grows further as more fields are needed
// (Outcome, AttendanceId, etc. per check-in-reader.md).
struct ScanResult {
    String message;
    // Echoed raw UID for an UnknownCard outcome, so it can be read off the screen
    // and bound to a student via PUT people/{personId}/card/uid. Empty otherwise.
    String scannedCard;
};

// Sends the scanned UID to POST /check-in/scan and returns what to display.
ScanResult scanCard(const String& uid);
