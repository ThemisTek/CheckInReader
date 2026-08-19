#pragma once

#include <Arduino.h>

// Mirrors the fields of Contracts/CheckIn/ScanContracts.cs -> ScanResultDto
// that the firmware actually renders. Grows once the real HTTP call is
// implemented (Outcome, AttendanceId, etc. per check-in-reader.md).
struct ScanResult {
    String message;
};

// Sends the scanned UID to POST /check-in/scan and returns what to display.
ScanResult scanCard(const String& uid);
