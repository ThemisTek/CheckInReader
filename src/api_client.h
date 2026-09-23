#pragma once

#include <Arduino.h>

// Mirrors Contracts/CheckIn/ScanContracts.cs -> ScanOutcome. The API has no
// JsonStringEnumConverter registered, so `outcome` arrives as its declaration-order ordinal;
// scanCard() maps that int to this enum. Unknown = an ordinal we don't recognise (e.g. the
// server added a new outcome) -- rendered as neutral grey with no icon and no Undo, never a crash.
enum class ScanOutcome {
    CheckedIn,
    AlreadyCheckedIn,
    ChooseSession,
    NoSession,
    UnknownCard,
    PairingBound,
    PairingAlreadyBound,
    Unknown,
};

// Mirrors the fields of Contracts/CheckIn/ScanContracts.cs -> ScanResultDto that the firmware
// actually renders.
struct ScanResult {
    ScanOutcome outcome = ScanOutcome::Unknown;
    String message;
    // Echoed raw UID for an UnknownCard outcome, so it can be read off the screen
    // and bound to a student via PUT people/{personId}/card/uid. Empty otherwise.
    String scannedCard;
    // The attendance row this scan created or found. Empty when the outcome has none
    // (UnknownCard, NoSession, ChooseSession, either pairing outcome).
    String attendanceId;
    // Empty when Student is null (UnknownCard) or the field was absent.
    String studentFirstName;
};

// Sends the scanned UID to POST /check-in/scan and returns what to display.
ScanResult scanCard(const String& uid);

// Deletes an attendance row by id (Undo). Returns true on 204 (idempotent: deleted or already gone)
// or defensively on 404 (company mismatch, should not occur in normal operation).
bool deleteAttendance(const String& attendanceId);

// ---- device pairing (API ADR-036) ----------------------------------------------------------
// Both calls are anonymous: a reader being paired has no key yet.

struct DevicePairingStart {
    bool ok = false;
    int httpStatus = 0;  // 429 = too many codes are open from this network; <= 0 = could not connect
    String code;
    String pollToken;
    int expiresInSeconds = 0;
};

// Asks the API for a pairing code. `ssid` is only a hint the manager sees to recognise this reader.
DevicePairingStart startDevicePairing(const String& ssid);

// Mirrors Contracts/DevicePairing DevicePairingStatus, which travels as its ordinal
// (0 Pending, 1 Claimed, 2 Expired). Error = the poll itself failed; try again.
enum class DevicePairingState { Pending, Claimed, Expired, Error };

struct DevicePairingPoll {
    DevicePairingState state = DevicePairingState::Error;
    // Set only when state == Claimed. The API delivers this exactly once.
    String companyId;
    String companyName;
    String deviceName;
    String apiKey;
};

DevicePairingPoll pollDevicePairing(const String& pollToken);
