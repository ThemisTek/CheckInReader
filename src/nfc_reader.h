#pragma once

#include <Arduino.h>

// Initializes the NFC Universal Unit (ST25R3916) over I2C. Call once from setup().
// Returns false if the unit did not respond.
bool initNfcReader();

// Polls the NFC Universal Unit for a card. Returns the tag UID as an
// upper-case hex string with no separators, or "" if no card is present.
String tryReadCard();
