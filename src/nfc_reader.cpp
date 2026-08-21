#include "nfc_reader.h"
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedNFC.h>
#include <wiring/m5_unit_unified_wiring.hpp>
#include <vector>

using namespace m5::nfc::a;

namespace {
m5::unit::UnitUnified units;
m5::unit::UnitNFC unit;
m5::nfc::NFCLayerA nfcA{unit};
bool unitReady = false;
}  // namespace

bool initNfcReader() {
    unitReady = m5::unit::wiring::addI2C(units, unit, 0, m5::unit::wiring::NessoPort::PortA) && units.begin();
    if (!unitReady) {
        Serial.println("[nfc_reader] failed to initialize NFC unit");
    }
    return unitReady;
}

String tryReadCard() {
    if (!unitReady) {
        return "";
    }

    units.update();

    // detect() puts any found PICC into HALT; identify() classifies it beyond the
    // provisional SAK-based guess detect() itself makes.
    std::vector<PICC> piccs;
    if (!nfcA.detect(piccs)) {
        return "";
    }

    String uid;
    for (auto&& picc : piccs) {
        if (nfcA.identify(picc)) {
            uid = picc.uidAsString().c_str();
            break;
        }
    }
    nfcA.deactivate();
    return uid;
}
