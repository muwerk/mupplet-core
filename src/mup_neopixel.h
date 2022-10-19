#pragma once

#include "ustd_array.h"
#include "scheduler.h"
#include "mupplet_core.h"
#include "Adafruit_NeoPixel.h"

namespace ustd {
class NeoPixel {
  public:
    String NEOPIXEL_VERSION = "0.1.0";
    Scheduler *pSched;
    int tID;
    String name;
    bool bStarted = false;
    uint8_t pin;
    uint16_t numPixels;
    uint8_t options;
    Adafruit_NeoPixel *pPixels;
    ustd::array<uint32_t> hwBuf;
    ustd::array<uint32_t> hwFrameBuf;
    bool state;

    NeoPixel(String name, uint8_t pin, uint16_t numPixels = 1,
             uint8_t options = NEO_GRB + NEO_KHZ800)
        : name(name), pin(pin), numPixels(numPixels), options(options) {
    }

    ~NeoPixel() {
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;

        pPixels = new Adafruit_NeoPixel(numPixels, pin, options);
        hwBuf = ustd::array<uint32_t>(numPixels, numPixels);
        hwFrameBuf = ustd::array<uint32_t>(numPixels, numPixels);
        for (uint16_t i = 0; i < numPixels; i++) {
            pixel(i, 0, 0, 0);
        }
        pPixels->begin();
        pixelsUpdate();
        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 100000);
        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/light/#", fnall);
        publishState();
        bStarted = true;
    }

    uint32_t RGB32(uint8_t r, uint8_t g, uint8_t b) {
        return (r << 16) + (g << 8) + b;
    }

    void RGB32Parse(uint32_t rgb, uint8_t *r = nullptr, uint8_t *g = nullptr,
                    uint8_t *b = nullptr) {
        if (r)
            *r = (rgb >> 16) & 0xff;
        if (g)
            *g = (rgb >> 8) & 0xff;
        if (b)
            *b = rgb & 0xff;
    }

    void set(uint16_t i, bool state) {
        if (i < numPixels) {
            if (state) {
                pixel(i, 0xff, 0xff, 0xff);
            } else {
                pixel(i, 0x0, 0x0, 0x0);
            }
        }
    }

    void pixel(uint16_t i, uint8_t r, uint8_t g, uint8_t b, bool updateHardware = true) {
        if (i >= numPixels)
            return;
        pPixels->setPixelColor(i, pPixels->Color(r, g, b));
        hwFrameBuf[i] = RGB32(r, g, b);
        if (updateHardware)
            pixelsUpdate();
    }

    void pixelsUpdate() {
        pPixels->show();
        uint32_t st = 0;
        for (uint16_t i = 0; i < numPixels; i++) {
            hwBuf[i] = hwFrameBuf[i];
            st |= hwBuf[i];
        }
        if (st)
            state = true;
        else
            state = false;
    }

    void publishState() {
        if (state) {
            pSched->publish(name + "/light/state", "on");
            this->state = true;
        } else {
            pSched->publish(name + "/light/state", "off");
            this->state = false;
        }
    }

    void loop() {
        if (bStarted)
            return;
    }

    void subsMsg(String topic, String msg, String originator) {
        String leader = name + "/light/";
        if (topic.startsWith(leader)) {
            String sub = topic.substring(leader.length());
            int pi = sub.indexOf('/');
            if (pi != -1) {
                int index = (int)strtol(sub.substring(0, pi).c_str(), 0, 10);
                if (index >= 0 && index < numPixels) {
                    String cmd = sub.substring(pi + 1);
                    if (cmd == "set") {
                        if (msg.startsWith("#") || msg.startsWith("0x")) {
                            uint8_t r, g, b;
                            if (parseColor(msg, &r, &g, &b)) {
                                pixel(index, r, g, b);
                            }
                        } else {
                            bool newState = parseBoolean(msg);
                            set(index, newState);
                        }
                    }
                }
            }
        }
    }
};  // NeoPixel
}  // namespace ustd
