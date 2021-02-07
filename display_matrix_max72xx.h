// display_matrix_max72xx.h - mupplet for Matrix Display using MAX72xx

#pragma once

#include "max72xx.h"
#include "ustd_array.h"
#include "timeout.h"
#include "helper/light_controller.h"

namespace ustd {

class DisplayMatrixMAX72XX {
  public:
    const char *version = "0.1.0";
    enum Mode {
        Normal,
        SlideIn,
    };

  private:
    // muwerk task management
    Scheduler *pSched;
    int tID;

    // device configuration
    String name;

    // hardware configuration
    max72xx max;

    // runtime
    LightController light;
    static const GFXfont *default_font;
    uint8_t current_font;
    ustd::array<const GFXfont *> fonts;
    Mode mode;                 // current display mode
    bool loopEffect;           // loop the display effect
    ustd::timeout waitEffect;  // time to wait between loops
    uint8_t speed;             // speed of effects
    String content;            // current display contant
    uint16_t charPos;          // index of char to slide
    uint8_t lastPos;           // target position of sliding char
    uint8_t slidePos;          // position of sliding char
    uint8_t charX;             // width of current char
    uint8_t charY;             // height of current char

  public:
    DisplayMatrixMAX72XX(String name, uint8_t csPin = D8, uint8_t hDisplays = 1,
                         uint8_t vDisplays = 1, uint8_t rotation = 0)
        : name(name), max(csPin, hDisplays, vDisplays, rotation), fonts(4, ARRAY_MAX_SIZE, 4) {
        fonts.add(default_font);
        current_font = 0;
    }

    void begin(Scheduler *_pSched, bool initialState = false) {
        pSched = _pSched;
        tID = pSched->add([this]() { this->loop(); }, name, 50000L);

        pSched->subscribe(tID, name + "/display/#", [this](String topic, String msg, String orig) {
            this->commandParser(topic.substring(name.length() + 9), msg);
        });
        pSched->subscribe(tID, name + "/light/#", [this](String topic, String msg, String orig) {
            this->light.commandParser(topic.substring(name.length() + 7), msg);
        });

        mode = Normal;
        loopEffect = true;
        waitEffect = 2000;
        speed = 2;

        // prepare hardware
        max.begin();
        max.fillScreen(0);
        max.write();
        max.setPowerSave(false);
        max.setIntensity(8);

        // start light controller
        light.begin([this](bool state, double level, bool control,
                           bool notify) { this->onLightControl(state, level, control, notify); },
                    initialState);
    }

    void addfont(const GFXfont *font) {
        fonts.add(font);
    }

    void clear(bool flush = true) {
        max.fillScreen(0);
        if (current_font != 0) {
            max.setFont();
            max.setCursor(0, 0);
            max.setFont(fonts[current_font]);
        } else {
            max.setCursor(0, 0);
        }
        if (flush) {
            max.write();
        }
    }

    void font(String arg) {
        int value = atoi(arg.c_str());
        if (value >= 0 && value < (int)fonts.length()) {
            max.setFont(fonts[value]);
            current_font = value;
        }
    }

    void print(String msg) {
        content = msg;
        clear(false);
        max.fillScreen(0);
        max.print(content);
        max.write();
    }

    void slideIn(String msg) {
        mode = SlideIn;
        content = msg;
        charPos = 0;
        lastPos = 0;
        slidePos = max.width();
        if (initNextCharDimensions()) {
            clear();
        } else {
            mode = Normal;
        }
    }

  private:
    void loop() {
        light.loop();
        if (mode == SlideIn) {
            if (charPos == content.length()) {
                // we are looping and waiting to restart
                if (waitEffect.test()) {
                    Serial.println("endOfEffect");
                    endOfEffect();
                    return;
                }
                Serial.print(".");
                return;
            }
            // clear char at previous position
            max.fillRect(slidePos, 0, slidePos + charX, charY, 0);
            // draw char at new position
            slidePos = (slidePos - speed) < lastPos ? lastPos : slidePos - speed;
            max.drawChar(slidePos, 0, content[charPos], 1, 0, 1);
            DBG3("Writing " + String(content[charPos]) + " at position " + String(slidePos));
            // flush to display
            max.write();
            // prepare for next iterations:
            if (slidePos <= lastPos) {
                // char has arrived
                lastPos += charX;
                slidePos = max.width();
                if (lastPos >= slidePos) {
                    // display full
                    endOfSlideIn();
                    return;
                }
                ++charPos;
                if (!initNextCharDimensions()) {
                    // end of string
                    endOfSlideIn();
                    return;
                }
            }
        }
    }

    bool initNextCharDimensions() {
        while (charPos < content.length()) {
            int16_t minx = 0x7FFF, miny = 0x7FFF, maxx = -1, maxy = -1;
            int16_t x = 0, y = 0;
            max.getCharBounds(content[charPos], &x, &y, &minx, &miny, &maxx, &maxy);
            if (maxx >= minx) {
                charX = maxx + 1;
                charY = maxy;
                DBG3("Character " + String(content[charPos]) + " has dimensions " + String(charX) +
                     "," + String(charY));
                if (content[charPos] == ' ') {
                    lastPos += charX;
                } else {
                    return true;
                }
            }
            // char is not printable
            ++charPos;
        }
        // end of string
        return false;
    }

    void endOfSlideIn() {
        if (loopEffect) {
            charPos = content.length();
            waitEffect.reset();
        } else {
            endOfEffect();
        }
    }

    void endOfEffect() {
        if (!loopEffect) {
            mode = Normal;
            return;
        }
        switch (mode) {
        case Normal:
            break;
        case SlideIn:
            clear(false);
            charPos = 0;
            lastPos = 0;
            slidePos = max.width();
            if (!initNextCharDimensions()) {
                mode = Normal;
            }
            break;
        }
    }

    void commandParser(String command, String args) {
        if (command == "clear") {
            clear();
        } else if (command == "font") {
            font(args);
        } else if (command == "print") {
            print(args);
        } else if (command == "slidein") {
            slideIn(args);
        } else if (command == "on") {
            light.set(true);
        } else if (command == "off") {
            light.set(false);
        } else if (command == "set") {
            light.commandParser("set", args);
        } else if (command == "mode") {
            light.commandParser("mode/set", args);
        }
    }

    void onLightControl(bool state, double level, bool control, bool notify) {
        uint8_t intensity = (level * 15000) / 1000;
        if (control) {
            max.setIntensity(intensity);
            // Serial.print("BRI:");
            // Serial.print(level);
            // Serial.print(":");
            // Serial.println(intensity);
        }
        if (control) {
            max.setPowerSave(!state);
            // Serial.println(state ? "ON" : "OFF");
        }
        if (notify) {
            //     char buf[32];
            //     sprintf(buf, "%5.3f", level);
            //     pSched->publish(name + "/light/unitbrightness", buf);
            pSched->publish(name + "/light/unitbrightness", String(level, 3));
            pSched->publish(name + "/light/state", state ? "on" : "off");
        }
    }
};

const GFXfont *DisplayMatrixMAX72XX::default_font = nullptr;
}  // namespace ustd