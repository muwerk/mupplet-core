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
    uint8_t gr, gg, gb;
    double gbr;
    Adafruit_NeoPixel *pPixels;
    ustd::array<uint32_t> *phwBuf;
    ustd::array<uint32_t> *phwFrameBuf;
    bool state;
    unsigned long ticker = 0;
    unsigned long lastTicker = 0;

    NeoPixel(String name, uint8_t pin, uint16_t numPixels = 1,
             uint8_t options = NEO_RGB + NEO_KHZ800)
        : name(name), pin(pin), numPixels(numPixels), options(options) {
    }

    ~NeoPixel() {
        // XXX Framebuffers cleanup
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;

        pPixels = new Adafruit_NeoPixel(numPixels, pin, options);
        phwBuf = new ustd::array<uint32_t>(numPixels, numPixels);
        phwFrameBuf = new ustd::array<uint32_t>(numPixels, numPixels);
        for (uint16_t i = 0; i < numPixels; i++) {
            pixel(i, 0, 0, 0);
        }
        pPixels->begin();
        pixelsUpdate();
        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 50000);
        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/light/#", fnall);
        publishState();
        publishColor();
        publishBrightness();
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
        (*phwFrameBuf)[i] = RGB32(r, g, b);
        if (updateHardware)
            pixelsUpdate();
    }

    bool setFrame(ustd::array<uint32_t> *pFr) {
        if (!pFr || pFr->length() != (*phwFrameBuf).length()) return false;
        for (uint16_t i = 0; i < numPixels; i++) {
            (*phwBuf)[i] = (*phwFrameBuf)[i];
        }
        pixelsUpdate();
    }

    void pixelsUpdate(bool notify = true) {
        pPixels->show();
        uint32_t st = 0;
        uint32_t br = 0;
        uint8_t dr = 0, dg = 0, db = 0, r, g, b;
        for (uint16_t i = 0; i < numPixels; i++) {
            (*phwBuf)[i] = (*phwFrameBuf)[i];
            st |= (*phwBuf)[i];
            RGB32Parse((*phwBuf)[i], &r, &g, &b);
            pPixels->setPixelColor(i, pPixels->Color(r, g, b));
            br += (r + g + b) / 3;
            dr += r;
            dg += g;
            db += b;
        }
        gbr = (br / numPixels) / 255.0;
        gr = dr / numPixels;
        gg = dg / numPixels;
        gb = db / numPixels;
        pPixels->show();
        if (st)
            state = true;
        else
            state = false;
        if (notify) {
            publishState();
            publishBrightness();
            publishColor();
        }
    }

    void brightness(double unitBrightness) {
        if (unitBrightness < 0.0) unitBrightness = 0.0;
        if (unitBrightness > 1.0) unitBrightness = 1.0;
        uint8_t br = (uint8_t)(unitBrightness * 255.0);
        for (uint16_t i = 0; i < numPixels; i++) {
            pixel(i, br, br, br, false);
        }
        pixelsUpdate();
    }

    void color(uint8_t r, uint8_t g, uint8_t b) {
        for (uint16_t i = 0; i < numPixels; i++) {
            pixel(i, r, g, b, false);
        }
        pixelsUpdate();
    }

    void publishBrightness(int16_t index = -1) {
        char buf[32];
        if (index == -1) {
            sprintf(buf, "%5.3f", gbr);
        } else {
            uint8_t r, g, b;
            RGB32Parse((*phwBuf)[index], &r, &g, &b);
            double br = ((uint16_t)r + (uint16_t)g + (uint16_t)b) / 3.0 / 255.0;
            sprintf(buf, "%5.3f", br);
        }
        pSched->publish(name + "/light/unitbrightness", buf);
    }

    void publishColor(int16_t index = -1) {
        char buf[64];
        if (index == -1) {
            sprintf(buf, "%d,%d,%d", gr, gg, gb);
        } else {
            uint8_t r, g, b;
            RGB32Parse((*phwBuf)[index], &r, &g, &b);
            sprintf(buf, "%d,%d,%d", r, g, b);
        }
        pSched->publish(name + "/light/color", buf);
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
            ++ticker;
    }

    void subsMsg(String topic, String msg, String originator) {
        uint8_t r, g, b;
        String leader = name + "/light/";
        if (topic == name + "/light/state/get") {
            publishState();
        } else if (topic == name + "/light/unitbrightness/get") {
            publishBrightness(-1);
        } else if (topic == name + "/light/color/get") {
            publishColor(-1);
        } else if (topic == name + "/light/set" || topic == name + "/light/state/set" || topic == name + "/light/unitbrightness/set") {
            if (ticker - lastTicker < 2) return;  // ignore anything that follows too "fast" after color-sets.
            double br = parseUnitLevel(msg);
            brightness(br);
            lastTicker = ticker;
        } else if (topic == name + "/light/color/set") {
            parseColor(msg, &r, &g, &b);
            color(r, g, b);
            lastTicker = ticker;
        } else if (topic.startsWith(leader)) {
            String sub = topic.substring(leader.length());
            int pi = sub.indexOf('/');
            if (pi != -1) {
                int index = (int)strtol(sub.substring(0, pi).c_str(), 0, 10);
                if (index >= 0 && index < numPixels) {
                    String cmd = sub.substring(pi + 1);
                    if (cmd == "set") {
                        if (msg.startsWith("#") || msg.startsWith("0x") || msg.indexOf(',') != -1) {
                            if (parseColor(msg, &r, &g, &b)) {
                                pixel(index, r, g, b);
                            }
                        } else {
                            bool newState = parseBoolean(msg);
                            set(index, newState);
                        }
                    }
                    if (cmd == "color/set") {
                        if (parseColor(msg, &r, &g, &b)) {
                            pixel(index, r, g, b);
                        }
                    }
                    if (cmd == "color/get") {
                        publishColor(index);
                    }
                    if (cmd == "brightness/get") {
                        publishBrightness(index);
                    }
                }
            }
        }
    }
};  // NeoPixel
}  // namespace ustd
