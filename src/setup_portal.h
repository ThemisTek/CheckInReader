#pragma once

// Runs WiFi setup until a network has been entered, tested and saved, then returns true.
//
// The reader opens a WPA2 hotspot ("DanceReader-XXXX", with a random password carried in the QR on its
// screen), redirects every name a joining phone looks up to a one-page form, and lets a person pick
// or type the school's WiFi. The network is tested for real before it is saved, and the outcome
// -- including "Wrong password" -- is shown on both the phone and the reader's screen. Blocks the
// caller for the whole session.
//
// allowCancel adds a Cancel button on the reader's screen and lets it return false; otherwise the only
// ways out are success or a hotspot that failed to start (also false). On true the caller should call
// connectWifi() to bring the STA connection up cleanly from the saved list.
bool runWifiSetupPortal(bool allowCancel);
