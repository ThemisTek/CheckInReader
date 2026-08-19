#pragma once

#include <Arduino.h>

// Polls the NFC Universal Unit for a card. Returns the tag UID as an
// upper-case hex string with no separators, or "" if no card is present.
String tryReadCard();
