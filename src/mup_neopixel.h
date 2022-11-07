#pragma once

#include "ustd_array.h"
#include "scheduler.h"
#include "mupplet_core.h"
#include "helper/mup_astro.h"
#include "Adafruit_NeoPixel.h"

namespace ustd {

uint32_t RGB32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}
void RGB32Parse(uint32_t rgb, uint8_t *r = nullptr, uint8_t *g = nullptr,
                uint8_t *b = nullptr) {
    if (r)
        *r = (uint8_t)((rgb >> 16) & 0xff);
    if (g)
        *g = (uint8_t)((rgb >> 8) & 0xff);
    if (b)
        *b = (uint8_t)(rgb & 0xff);
}

class SpecialEffects {
  public:
    static const int effectCount = 7;
    enum EffectType : uint16_t { Default = 0,
                                 ButterLamp = 1,
                                 Fire = 2,
                                 Waves = 3,
                                 Forest = 4,
                                 Evening = 5,
                                 Concentration = 6
    };
    static constexpr const char *effectName[effectCount] = {"Static", "Butterlamp", "Fire", "Waves", "Forest", "Evening", "Concentration"};
    uint16_t rows, cols;
    SpecialEffects(uint16_t rows, uint16_t cols)
        : rows(rows), cols(cols) {
    }
    ~SpecialEffects() {
    }

    bool setFrame(EffectType type, ustd::array<uint32_t> *pf) {
        switch (type) {
        default:
            return false;
        case EffectType::ButterLamp:
            return butterLampFrame(pf, rows, cols);
        case EffectType::Forest:
            return forestFrame(pf, rows, cols);
        case EffectType::Waves:
            return wavesFrame(pf, rows, cols);
        }
        return false;
    }

    bool bUseModulator = false;
    time_t manualSet = 0;
    bool useAutoTimer = false;
    uint8_t start_hour = 18, start_minute = 0, end_hour = 0, end_minute = 0;
    bool bUnitBrightness = true;
    double unitBrightness = 1.0;
    double amp = 20.0;
    double oldMx = -1.0;

    void configButterLampModulator(bool _bUseModulator, bool _bUseAutoTimer, uint8_t _start_hour, uint8_t _start_minute, uint8_t _end_hour, uint8_t _end_minute) {
        bUseModulator = _bUseModulator;
        useAutoTimer = _bUseAutoTimer;
        start_hour = _start_hour;
        start_minute = _start_minute;
        end_hour = _end_hour;
        end_minute = _end_minute;
    }

    void configButterLampState(bool _bUnitBrightness = true, double _unitBrightness = 1.0) {
        bUnitBrightness = _bUnitBrightness;
        unitBrightness = _unitBrightness;
        manualSet = time(nullptr);
    }

    double butterLampModulator() {
        double m1 = 1.0;
        double m2 = 0.0;
        time_t now = time(nullptr);

        if (!bUseModulator)
            return m1;
        long dt = now - manualSet;
        if (dt < 3600) {
            m2 = (3600.0 - (double)dt) / 3600.0;
        }

        if (useAutoTimer) {
            struct tm *pTm = localtime(&now);

            if (Astro::inHourMinuteInterval(pTm->tm_hour, pTm->tm_min, start_hour, start_minute, end_hour,
                                            end_minute)) {
                int deltaAll = Astro::deltaHourMinuteTime(start_hour, start_minute, end_hour, end_minute);
                int deltaCur =
                    Astro::deltaHourMinuteTime(start_hour, start_minute, pTm->tm_hour, pTm->tm_min);
                if (deltaAll > 0.0)
                    m1 = (deltaAll - deltaCur) / (double)deltaAll;
                else
                    m1 = 0.0;
            } else {
                m1 = 0.0;
            }
        } else {
            m1 = 0.0;
        }

        if (bUnitBrightness) {
            if (m1 > 0.0 || m2 > 0.0) {
                m1 = m1 * unitBrightness;
                m2 = m2 * unitBrightness;
            }
        }
        if (m2 != 0.0) {
            if (m2 > 0.75)
                m1 = m2;
            else
                m1 = (m1 + m2) / 2.0;
        }
        return m1;
    }

    int f1 = 0, f2 = 0, max_b = 20;
    double wind = 50;
    bool butterLampFrame(ustd::array<uint32_t> *pf, uint16_t rows, uint16_t cols) {
        /*! simulate an arbitrary large array of butterlamps burning in the wind until they are slowly extinguished.

        Note: uses realistic simulation of butterlamp flickering and weakening of the flame over time (requires start/end times)
        */
        int ce, cr, cg, cb, mf;
        int flic[] = {4, 7, 8, 9, 10, 12, 16, 20, 32, 30, 32, 20, 24, 16, 8, 6};
        uint16_t x, y, index, cx, cy;
        if (pf->length() != rows * cols) {
            for (uint16_t i = 0; i < pf->length(); i++)
                (*pf)[i] = RGB32(255, 0, 0);
            return false;
        }

        for (y = 0; y < rows; y++) {
            for (x = 0; x < cols; x++) {
                index = y * cols + x;
                cx = x % 4;
                cy = y % 4;
                if ((cx == 1 || cx == 2 || cols < 4) && (cy == 1 || cy == 2 || rows < 4)) {
                    ce = 1;  // centre
                } else {
                    ce = 0;
                }
                if (ce == 1) {  // center of one lamp
                    cr = 40;
                    cg = 15;
                    cb = 0;
                    mf = flic[f1];
                    f1 += rand() % 3 - 1;
                    if (f1 < 0)
                        f1 = 15;
                    if (f1 > 15)
                        f1 = 0;
                    mf = 32 - ((32 - mf) * wind) / 100;
                } else {  // border
                    cr = 20;
                    cg = 4;
                    cb = 0;
                    mf = flic[f2];
                    f2 += rand() % 3 - 1;
                    if (f2 < 0)
                        f2 = 15;
                    if (f2 > 15)
                        f2 = 0;
                    mf = 32 - ((32 - mf) * wind) / 100;
                }

                cr = cr + rand() % 2;
                cg = cg + rand() % 2;
                cb = cb + rand() % 1;

                if (cr > max_b)
                    max_b = cr;
                if (cg > max_b)
                    max_b = cg;
                if (cb > max_b)
                    max_b = cb;

                cr = (cr * amp * 4 * mf) / (max_b * 50);
                cg = (cg * amp * 4 * mf) / (max_b * 50);
                cb = (cb * amp * 4 * mf) / (max_b * 50);

                if (cr > 255)
                    cr = 255;
                if (cr < 0)
                    cr = 0;
                if (cg > 255)
                    cg = 255;
                if (cg < 0)
                    cg = 0;
                if (cb > 255)
                    cb = 255;
                if (cb < 0)
                    cb = 0;

                if (bUseModulator) {
                    double mx = butterLampModulator();
                    double dx = fabs(oldMx - mx);
                    if (dx > 0.05) {
                        oldMx = mx;
                    }
                    cr = ((double)cr * mx);
                    cg = ((double)cg * mx);
                    cb = ((double)cb * mx);
                }
                (*pf)[index] = RGB32(cr, cg, cb);
            }
        }
        return true;
    }

    uint8_t varyByte(uint8_t b, uint8_t var, uint8_t min = 0, uint8_t max = 255) {
        int d = random(2 * var + 1) - var;
        if ((int)b + d < (int)min) return min;
        if ((int)b + d > (int)max) return max;
        return (uint8_t)((int)b + d);
    }

    bool wavesFrame(ustd::array<uint32_t> *pf, uint16_t rows, uint16_t cols) {
        uint16_t num, ind;
        uint8_t r, g, b;
        uint32_t color;
        num = rows * cols;
        for (int i = 0; i < 20; i++) {
            ind = random(num);
            color = (*pf)[ind];
            RGB32Parse(color, &r, &g, &b);
            b = varyByte(b, 20, 20, 170);
            g = varyByte(g, 10, 0, 50);
            r = varyByte(r, 10, 0, 20);
            color = RGB32(r, g, b);
            (*pf)[ind] = color;
        }
        return true;
    }
    bool forestFrame(ustd::array<uint32_t> *pf, uint16_t rows, uint16_t cols) {
        uint16_t num, ind;
        uint8_t r, g, b;
        uint32_t color;
        num = rows * cols;
        for (int i = 0; i < 20; i++) {
            ind = random(num);
            color = (*pf)[ind];
            RGB32Parse(color, &r, &g, &b);
            b = varyByte(b, 10, 0, 70);
            g = varyByte(g, 20, 20, 200);
            r = varyByte(r, 10, 0, 30);
            color = RGB32(r, g, b);
            (*pf)[ind] = color;
        }
        return true;
    }
};  // SpecialEffects

class NeoPixel {
  public:
    String NEOPIXEL_VERSION = "0.1.0";
    Scheduler *pSched;
    int tID;
    String name;
    bool bStarted = false;
    uint8_t pin;
    uint16_t numRows, numCols, numPixels;
    uint8_t options;
    uint8_t gr, gg, gb;
    double gbr;
    double unitBrightness;
    Adafruit_NeoPixel *pPixels;
    ustd::array<uint32_t> *phwBuf;
    ustd::array<uint32_t> *phwFrameBuf;
    bool state;
    unsigned long ticker = 0;
    unsigned long lastTicker = 0;
    double zeroBrightnessUpperBound = 0.02;
    SpecialEffects::EffectType effectType;
    SpecialEffects *pEffects = nullptr;
    bool isFirstLoop = true;
    bool scheduled = false;
    int startHour, endHour, startMin, endMin;

    NeoPixel(String name, uint8_t pin, uint16_t numRows = 1, uint16_t numCols = 1,
             uint16_t options = NEO_RGB + NEO_KHZ800)
        : name(name), pin(pin), numRows(numRows), numCols(numCols), options(options) {
        if (numRows < 1) numRows = 1;
        if (numCols < 1) numCols = 1;
        numPixels = numRows * numCols;
    }

    ~NeoPixel() {
        // XXX Framebuffers cleanup
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;

        pPixels = new Adafruit_NeoPixel(numPixels, pin, options);
        phwBuf = new ustd::array<uint32_t>(numPixels, numPixels);
        phwFrameBuf = new ustd::array<uint32_t>(numPixels, numPixels);
        pEffects = new SpecialEffects(numRows, numCols);
        for (uint16_t i = 0; i < numPixels; i++) {
            pixel(i, 0, 0, 0, false);
        }
        pPixels->begin();
        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 50000);
        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/light/#", fnall);
        pSched->subscribe(tID, "mqtt/state", fnall);
        setEffect(SpecialEffects::EffectType::Default, true);
        publishState();
        publishColor();
        bStarted = true;
    }

    void pixel(uint16_t i, uint8_t r, uint8_t g, uint8_t b, bool update = true) {
        if (i >= numPixels)
            return;
        (*phwFrameBuf)[i] = RGB32(r, g, b);
        // setEffect(SpecialEffects::EffectType::Default);
        if (update)
            pixelsUpdate();
    }

    bool setFrame(ustd::array<uint32_t> *pFr) {
        if (!pFr || pFr->length() != (*phwFrameBuf).length()) return false;
        for (uint16_t i = 0; i < numPixels; i++) {
            (*phwBuf)[i] = (*phwFrameBuf)[i];
        }
        pixelsUpdate();
    }

    void setEffect(SpecialEffects::EffectType _type, bool force = false) {
        if (_type != effectType || force) {
            effectType = _type;
            isFirstLoop = true;
            publishEffect();
        }
    }

    String getEffectList() {
        /*! Generate a single string that contains a comma-separated list of available special effects, as used for HA addLight() */
        String eff;
        eff = "";
        bool first = true;
        for (int i = 0; i < SpecialEffects::effectCount; i++) {
            if (!first) eff += ", ";
            first = false;
            eff += SpecialEffects::effectName[i];
        }
        return eff;
    }

    bool setSchedule(String startTime, String endTime) {
        int sh, sm, eh, em;
        if (pEffects == nullptr) return false;
        if (!ustd::Astro::parseHourMinuteString(startTime, &sh, &sm))
            return false;
        if (!ustd::Astro::parseHourMinuteString(endTime, &eh, &em))
            return false;
        startHour = sh;
        endHour = eh;
        startMin = sm;
        endMin = em;
        if (effectType == SpecialEffects::EffectType::ButterLamp) {
            pEffects->configButterLampModulator(true, true, sh, sm, eh, em);
        }
        return true;
    }
    void pixelsUpdate(bool notify = true) {
        pPixels->show();
        uint32_t st = 0;
        double br = 0.0;
        uint32_t dr = 0, dg = 0, db = 0;
        uint8_t r, g, b, rs, gs, bs;
        for (uint16_t i = 0; i < numPixels; i++) {
            (*phwBuf)[i] = (*phwFrameBuf)[i];
            st |= (*phwBuf)[i];
            RGB32Parse((*phwBuf)[i], &r, &g, &b);
            br += ((double)r + (double)g + (double)b) / 3.0;
            dr += r;
            dg += g;
            db += b;
            if (unitBrightness != 1.0) {
                rs = (uint8_t)(r * unitBrightness);
                gs = (uint8_t)(g * unitBrightness);
                bs = (uint8_t)(b * unitBrightness);
            } else {
                rs = r;
                gs = g;
                bs = b;
            }
            pPixels->setPixelColor(i, pPixels->Color(rs, gs, bs));
        }
        gbr = (br / (double)numPixels) / 255.0;
        gr = dr / numPixels;
        gg = dg / numPixels;
        gb = db / numPixels;
        pPixels->show();
        if (st && unitBrightness > zeroBrightnessUpperBound)
            state = true;
        else
            state = false;
        if (notify) {
            publishState();
            publishColor();
        }
    }

    void brightness(double _unitBrightness, bool update = true, bool resetEffect = true) {
        if (_unitBrightness < 0.0) _unitBrightness = 0.0;
        if (_unitBrightness > 1.0) _unitBrightness = 1.0;
        // if (_unitBrightness == 1.0 && gbr < zeroBrightnessUpperBound) color(0xff, 0xff, 0xff, false);
        if (_unitBrightness < zeroBrightnessUpperBound) _unitBrightness = 0.0;
        unitBrightness = _unitBrightness;
        if (resetEffect)
            setEffect(SpecialEffects::EffectType::Default);
        if (update) pixelsUpdate();
    }

    void color(uint8_t r, uint8_t g, uint8_t b, bool update = true, bool resetEffect = true) {
        for (uint16_t i = 0; i < numPixels; i++) {
            pixel(i, r, g, b, false);
        }
        if (resetEffect)
            setEffect(SpecialEffects::EffectType::Default);
        if (update) {
            pixelsUpdate();
        }
    }

    void publishBrightness() {
        char buf[32];
        sprintf(buf, "%5.3f", unitBrightness);
        pSched->publish(name + "/light/unitbrightness", buf);
    }

    void publishColor(int16_t index = -1) {
        char buf[64];
        if (index == -1) {
            sprintf(buf, "%d,%d,%d", gr, gg, gb);
            pSched->publish(name + "/light/color", buf);
        } else {
            uint8_t r, g, b;
            RGB32Parse((*phwBuf)[index], &r, &g, &b);
            sprintf(buf, "%d,%d,%d", r, g, b);
            pSched->publish(name + "/light/" + String(index) + "/color", buf);
        }
    }

    void publishEffect() {
        String nameE = SpecialEffects::effectName[(int)effectType];
        pSched->publish(name + "/light/effect", nameE);
    }

    void publishState() {
        if (state) {
            pSched->publish(name + "/light/state", "on");
            this->state = true;
        } else {
            pSched->publish(name + "/light/state", "off");
            this->state = false;
        }
        publishBrightness();
        publishEffect();
    }

    void loop() {
        if (bStarted) {
            ++ticker;
            switch (effectType) {
            case SpecialEffects::EffectType::Default:
                if (isFirstLoop) {
                    color(128, 128, 128, false, false);
                    brightness(0.2, true, false);
                    isFirstLoop = false;
                }
                break;
            case SpecialEffects::EffectType::ButterLamp:
                if (ticker % 3 != 0) return;
                if (isFirstLoop) {
                    brightness(1.0, false, false);
                    pEffects->setFrame(effectType, phwFrameBuf);
                    pixelsUpdate(true);
                    isFirstLoop = false;
                } else {
                    pEffects->setFrame(effectType, phwFrameBuf);
                    pixelsUpdate(false);
                }
                break;
            case SpecialEffects::EffectType::Fire:
                if (isFirstLoop) {
                    brightness(1.0, false, false);
                    pEffects->setFrame(SpecialEffects::EffectType::ButterLamp, phwFrameBuf);  // Fire is 'fast' butterlamp
                    pixelsUpdate(true);
                    isFirstLoop = false;
                } else {
                    pEffects->setFrame(SpecialEffects::EffectType::ButterLamp, phwFrameBuf);  // Fire is 'fast' butterlamp
                    pixelsUpdate(false);
                }
                break;
            case SpecialEffects::EffectType::Waves:
                if (ticker % 5 != 0) return;
                if (isFirstLoop) {
                    color(20, 50, 192, false, false);
                    brightness(0.1, false, false);
                    pEffects->setFrame(SpecialEffects::EffectType::Waves, phwFrameBuf);  // Fire is 'fast' butterlamp
                    pixelsUpdate(true);
                    isFirstLoop = false;
                } else {
                    pEffects->setFrame(SpecialEffects::EffectType::Waves, phwFrameBuf);  // Fire is 'fast' butterlamp
                    pixelsUpdate(false);
                }
                break;
            case SpecialEffects::EffectType::Forest:
                if (ticker % 10 != 0) return;
                if (isFirstLoop) {
                    color(0, 128, 0, false, false);
                    brightness(0.2, false, false);
                    pEffects->setFrame(SpecialEffects::EffectType::Forest, phwFrameBuf);  // Fire is 'fast' butterlamp
                    pixelsUpdate(true);
                    isFirstLoop = false;
                } else {
                    pEffects->setFrame(SpecialEffects::EffectType::Forest, phwFrameBuf);  // Fire is 'fast' butterlamp
                    pixelsUpdate(false);
                }
                break;
            case SpecialEffects::EffectType::Evening:
                if (isFirstLoop) {
                    isFirstLoop = false;
                    color(255, 128, 0, false, false);
                    brightness(0.1, true, false);
                }
                break;
            case SpecialEffects::EffectType::Concentration:
                if (isFirstLoop) {
                    isFirstLoop = false;
                    color(128, 128, 255, false, false);
                    brightness(0.8, true, false);
                }
                break;
            default:
                break;
            }
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        uint8_t r, g, b;
        String leader = name + "/light/";
        if (topic == name + "/light/state/get") {
            publishState();
        } else if (topic == name + "/light/unitbrightness/get") {
            publishBrightness();
        } else if (topic == name + "/light/color/get") {
            publishColor(-1);
        } else if (topic == name + "/light/set" || topic == name + "/light/state/set" || topic == name + "/light/unitbrightness/set") {
            // if (ticker - lastTicker < 6) return;  // ignore anything that follows too "fast" after color-sets.
            bool ab;
            msg.toLowerCase();
            if (msg == "on" || msg == "true")
                ab = true;
            else
                ab = false;
            double br = parseUnitLevel(msg);
            if (ab && unitBrightness > zeroBrightnessUpperBound) br = unitBrightness;
            brightness(br);
            lastTicker = ticker;
        } else if (topic == name + "/light/color/set") {
            parseColor(msg, &r, &g, &b);
            color(r, g, b);
            lastTicker = ticker;
        } else if (topic == name + "/light/effect/set") {
            for (int eff = 0; eff < SpecialEffects::effectCount; eff++) {
                String thisName = SpecialEffects::effectName[eff];
                if (thisName == msg) {
                    setEffect((SpecialEffects::EffectType)eff);
                }
            }
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
                            if (newState) {
                                pixel(index, 0xff, 0xff, 0xff);
                            } else {
                                pixel(index, 0xff, 0xff, 0xff);
                            }
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
                }
            }
        } else if (topic == "mqtt/state" && msg == "connected") {
            publishState();
            publishColor();
        }
    }
};  // NeoPixel

}  // namespace ustd
