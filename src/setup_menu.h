#pragma once

enum class SetupMenuChoice { None, ChangeWifi, RePair };

// How long the idle screen must be held to open the setup menu.
constexpr unsigned long SETUP_HOLD_MS = 5000;

// Call every tick while the reader is idle (and, at boot, while it is waiting for WiFi). True once a
// single finger has been held down for SETUP_HOLD_MS. Keeps its own state; never call it while a
// scan result or Undo button is showing, so it cannot collide with them.
bool setupLongPressDetected();

// Shows "Change WiFi / Re-pair to school / Cancel". A choice is confirmed on a second screen before it
// is returned, because either one stops the reader checking students in until setup is finished.
// Returns None on Cancel, on Back-then-timeout, or after 20 idle seconds. Drains the touch first: the
// finger that just held for five seconds is still down and its release must not press a button.
SetupMenuChoice runSetupMenu();
