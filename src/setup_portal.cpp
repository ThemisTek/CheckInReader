#include "setup_portal.h"
#include <Arduino.h>
#include <DNSServer.h>
#include <M5Unified.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>
#include <algorithm>
#include <vector>
#include "device_config.h"
#include "setup_screens.h"
#include "wifi_setup.h"

namespace {

const unsigned long JOIN_TIMEOUT_MS = 20000;          // testing the network someone typed
const unsigned long PAGE_DELIVERY_GRACE_MS = 2500;    // let the phone receive the success page
const size_t MAX_LISTED_NETWORKS = 20;

String htmlEscape(const String& in) {
    String out;
    out.reserve(in.length() + 8);
    for (size_t i = 0; i < in.length(); ++i) {
        char c = in[i];
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c;
        }
    }
    return out;
}

String pageShell(const String& body) {
    String html =
        "<!doctype html><html><head><meta charset=utf-8>"
        "<meta name=viewport content='width=device-width,initial-scale=1'>"
        "<title>Reader setup</title><style>"
        "body{font-family:system-ui,sans-serif;margin:0;padding:20px;background:#f4f5fb;color:#1c1e33}"
        "main{max-width:420px;margin:0 auto}h1{font-size:1.3rem}"
        "label{display:block;margin:14px 0 4px;font-weight:600}"
        "input,select,button{width:100%;box-sizing:border-box;padding:12px;font-size:1rem;"
        "border:1px solid #c9cce0;border-radius:8px}"
        "button{background:#4f46e5;color:#fff;border:0;margin-top:18px;font-weight:600}"
        ".err{background:#fde8e8;color:#8a1c1c;padding:10px;border-radius:8px}"
        ".ok{background:#e6f6ec;color:#14532d;padding:10px;border-radius:8px}"
        "</style></head><body><main>";
    html += body;
    html += "</main></body></html>";
    return html;
}

String formPage(const std::vector<String>& networks, const String& error) {
    String body = "<h1>Set up this reader</h1>";
    if (error.length() > 0) {
        body += "<p class=err>" + htmlEscape(error) + "</p>";
    }
    body +=
        "<p>Choose the school's WiFi. If this page stops loading after you tap Connect, look at the "
        "reader's screen &mdash; it shows whether it worked.</p>"
        "<form method=post action=/save>"
        "<label for=ssid>WiFi network</label><select id=ssid name=ssid>";
    for (const String& ssid : networks) {
        body += "<option>" + htmlEscape(ssid) + "</option>";
    }
    body +=
        "</select>"
        "<label for=manual>Or type the network name</label>"
        "<input id=manual name=manual autocomplete=off autocapitalize=off>"
        "<label for=pass>Password</label>"
        "<input id=pass name=pass type=password autocomplete=off>"
        "<button>Connect</button></form>";
    return pageShell(body);
}

String successPage(const String& ssid) {
    return pageShell(
        "<h1>Connected</h1><p class=ok>The reader joined <b>" + htmlEscape(ssid) +
        "</b>. You can close this page &mdash; the reader carries on by itself.</p>");
}

String macSuffix() {
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    return mac.substring(mac.length() - 4);
}

// The hotspot password lives only for this session and only on the reader's screen (in the QR).
String randomPassword(size_t length) {
    static const char alphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";
    String out;
    for (size_t i = 0; i < length; ++i) {
        out += alphabet[esp_random() % (sizeof(alphabet) - 1)];
    }
    return out;
}

std::vector<String> scanVisibleNetworks() {
    std::vector<String> ssids;
    int found = WiFi.scanNetworks();
    for (int i = 0; i < found && ssids.size() < MAX_LISTED_NETWORKS; ++i) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() > 0 && std::find(ssids.begin(), ssids.end(), ssid) == ssids.end()) {
            ssids.push_back(ssid);
        }
    }
    WiFi.scanDelete();
    return ssids;
}

}  // namespace

bool runWifiSetupPortal(bool allowCancel) {
    const String apName = "DanceReader-" + macSuffix();
    const String apPassword = randomPassword(8);
    const String qrPayload = "WIFI:T:WPA;S:" + apName + ";P:" + apPassword + ";;";

    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP_STA);
    showSetupNotice("Looking for WiFi networks...", false);
    const std::vector<String> visible = scanVisibleNetworks();

    if (!WiFi.softAP(apName.c_str(), apPassword.c_str())) {
        Serial.println("[setup_portal] softAP failed");
        showSetupNotice("Could not start the setup hotspot. Retrying...", false);
        delay(2000);
        WiFi.mode(WIFI_STA);
        return false;
    }
    Serial.printf("[setup_portal] hotspot %s up at %s\n", apName.c_str(),
                  WiFi.softAPIP().toString().c_str());

    DNSServer dns;
    dns.start(53, "*", WiFi.softAPIP());  // every name resolves to us: that is what makes it "captive"
    WebServer server(80);

    bool joined = false;
    TouchRect cancel;
    auto redraw = [&](const String& status) {
        cancel = showWifiSetupScreen(qrPayload, apName, status, allowCancel);
    };
    redraw("Waiting for your phone...");

    server.on("/", HTTP_GET, [&]() { server.send(200, "text/html", formPage(visible, "")); });

    server.on("/save", HTTP_POST, [&]() {
        String ssid = server.arg("manual");
        ssid.trim();
        if (ssid.length() == 0) {
            ssid = server.arg("ssid");
        }
        const String password = server.arg("pass");

        if (ssid.length() == 0) {
            server.send(200, "text/html", formPage(visible, "Choose or type a network name."));
            return;
        }

        redraw("Connecting to " + ssid + "...");
        String reason;
        if (joinNetwork(ssid, password, JOIN_TIMEOUT_MS, &reason)) {
            deviceConfig::saveWifiNetwork(ssid, password);
            joined = true;
            redraw("Connected to " + ssid);
            server.send(200, "text/html", successPage(ssid));
        } else {
            redraw(reason + " - try again");
            server.send(200, "text/html",
                        formPage(visible, reason + ". Check the details and try again."));
        }
    });

    // Phones probe fixed URLs (/generate_204, /hotspot-detect.html, /connecttest.txt ...) to decide
    // whether to pop the "sign in to network" sheet; redirecting every unknown path to the form
    // satisfies all of them.
    server.onNotFound([&]() {
        server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
        server.send(302, "text/plain", "");
    });
    server.begin();

    waitForTouchRelease();
    bool cancelled = false;
    unsigned long joinedAt = 0;
    while (!cancelled) {
        M5.update();
        dns.processNextRequest();
        server.handleClient();

        if (joined) {
            if (joinedAt == 0) {
                joinedAt = millis();
            }
            if (millis() - joinedAt > PAGE_DELIVERY_GRACE_MS) {
                break;
            }
        } else if (allowCancel && rectTapped(cancel)) {
            cancelled = true;
        }
        delay(5);
    }

    server.stop();
    dns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    return joined && !cancelled;
}
