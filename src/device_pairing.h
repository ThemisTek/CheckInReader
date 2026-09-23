#pragma once

enum class PairingResult { Paired, Cancelled };

// Shows a pairing code and waits for a manager to enter it in the web app, then saves the API key,
// company and URL the API hands back. Codes last 10 minutes; when one runs out a fresh one is
// requested automatically. An unreachable server or a "too many codes" answer is retried, not fatal.
//
// allowCancel adds a Cancel button (used when someone chose "Re-pair" from the setup menu and can
// still back out); a reader that has no key yet has nothing else to do, so it never cancels.
// Blocks until paired or cancelled.
PairingResult runDevicePairing(bool allowCancel);
