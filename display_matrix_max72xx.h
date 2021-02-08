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
        Left,
        Center,
        Right,
        SlideIn,
    };

  private:
    // font helper
    typedef struct {
        uint8_t baseLine;
        uint8_t xAdvance;
        uint8_t yAdvance;
        uint8_t dummy;
    } FontSize;

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
    ustd::array<FontSize> sizes;
    Mode mode;                 // current display mode
    bool loopEffect;           // loop the display effect
    ustd::timeout waitEffect;  // time to wait between loops
    uint8_t speed;             // speed of effects
    String content;            // current display contant
    uint8_t delayCtr;          // effect delay counter
    uint16_t charPos;          // index of char to slide
    uint8_t lastPos;           // target position of sliding char
    uint8_t slidePos;          // position of sliding char
    uint8_t charX;             // width of current char
    uint8_t charY;             // height of current char

  public:
    DisplayMatrixMAX72XX(String name, uint8_t csPin = D8, uint8_t hDisplays = 1,
                         uint8_t vDisplays = 1, uint8_t rotation = 0)
        : name(name), max(csPin, hDisplays, vDisplays, rotation), fonts(4, ARRAY_MAX_SIZE, 4) {
        FontSize default_size = {0, 6, 8, 0};
        fonts.add(default_font);
        sizes.add(default_size);
        current_font = 0;
    }

    void begin(Scheduler *_pSched, bool initialState = false) {
        pSched = _pSched;
        tID = pSched->add([this]() { this->loop(); }, name, 10000L);

        pSched->subscribe(tID, name + "/display/#", [this](String topic, String msg, String orig) {
            this->commandParser(topic.substring(name.length() + 9), msg);
        });
        pSched->subscribe(tID, name + "/light/#", [this](String topic, String msg, String orig) {
            this->light.commandParser(topic.substring(name.length() + 7), msg);
        });

        mode = Left;
        loopEffect = true;
        waitEffect = 2000;
        speed = 16;

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
        // determine sizes and positions of specified font
        FontSize size = {0, 0, 0, 0};
        uint8_t first = pgm_read_byte(&font->first);
        uint8_t last = pgm_read_byte(&font->last);
        uint16_t aOffset = (uint16_t)'A' > first ? 'A' - first : 0;
        uint16_t glyphs = last - first + 1;

        for (uint16_t i = 0; i < glyphs; i++) {
            GFXglyph *glyph = pgm_read_glyph_ptr(font, i);
            uint8_t xAdvance = (uint8_t)pgm_read_byte(&glyph->xAdvance);

            if (i == aOffset) {
                Serial.println((int8_t)pgm_read_byte(&glyph->yOffset));
                size.baseLine = (int8_t)pgm_read_byte(&glyph->yOffset) * -1;
            }
            if (xAdvance > size.xAdvance) {
                size.xAdvance = xAdvance;
            }
        }
        size.yAdvance = (uint8_t)pgm_read_byte(&font->yAdvance);
        // add to array
        fonts.add(font);
        sizes.add(size);
    }

    void clear(bool flush = true) {
        max.fillScreen(0);
        max.setCursor(0, sizes[current_font].baseLine);
        if (flush) {
            max.write();
        }
    }

    void font(String args) {
        int value = atoi(args.c_str());
        if (value >= 0 && value < (int)fonts.length()) {
            max.setFont(fonts[value]);
            current_font = value;
        }
    }

    void setmode(String args) {
    }

    void print(String msg) {
        content = msg;
        display();
    }

    void left(String args) {
        mode = Left;
        content = args;
        displayLeft();
    }

    void center(String args) {
        mode = Center;
        content = args;
        displayCenter();
    }

    void right(String args) {
        mode = Right;
        content = args;
        displayRight();
    }

    void slideIn(String msg) {
        mode = SlideIn;
        content = msg;
        charPos = 0;
        lastPos = 0;
        delayCtr = 17 - speed;
        slidePos = max.width();
        if (initNextCharDimensions()) {
            clear();
        } else {
            mode = Left;
            displayLeft();
        }
    }

    void setSpeed(String args) {
        int i = atoi(args.c_str());
        if (i > 0 && i < 17) {
            speed = i;
        }
    }

  private:
    void loop() {
        light.loop();
        if (mode == SlideIn) {
            if (charPos == content.length()) {
                // we are looping and waiting to restart
                if (waitEffect.test()) {
                    endOfEffect();
                    return;
                }
                return;
            }
            if (--delayCtr) {
                return;
            }
            delayCtr = 17 - speed;
            // clear char at previous position
            max.fillRect(slidePos, 0, slidePos + charX, charY, 0);
            // draw char at new position
            // slidePos = (slidePos - speed) < lastPos ? lastPos : slidePos - speed;
            slidePos--;
            max.drawChar(slidePos, sizes[current_font].baseLine, content[charPos], 1, 0, 1);
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

    void display() {
        switch (mode) {
        case Left:
            displayLeft();
            break;
        case Center:
            displayCenter();
            break;
        case Right:
            displayRight();
            break;
        default:
            break;
        }
    }

    void displayLeft() {
        clear(false);
        max.print(content);
        max.write();
    }

    void displayCenter() {
        int16_t width = getTextWidth(content);
        clear(false);
        max.setCursor((max.width() - width) / 2, max.getCursorY());
        max.print(content);
        max.write();
    }

    void displayRight() {
        int16_t width = getTextWidth(content);
        clear(false);
        max.setCursor(-width, max.getCursorY());
        max.print(content);
        max.write();
    }

    int16_t getTextWidth(const String &text) {
        int16_t x, y;
        uint16_t w, h;
        max.getTextBounds(text, 0, max.getCursorY(), &x, &y, &w, &h);
        if (w < max.width()) {
            return w;
        } else {
            GFXcanvas1 tmp(text.length() * sizes[current_font].xAdvance,
                           sizes[current_font].yAdvance);
            if (current_font != 0) {
                tmp.setFont(fonts[current_font]);
            }
            tmp.print(text);
            return tmp.getCursorX();
        }
    }

    bool initNextCharDimensions() {
        while (charPos < content.length()) {
            int16_t minx = 0x7FFF, miny = 0x7FFF, maxx = -1, maxy = -1;
            int16_t x = 0, y = sizes[current_font].baseLine;
            max.getCharBounds(content[charPos], &x, &y, &minx, &miny, &maxx, &maxy);
            if (maxx >= minx) {
                charX = x;
                charY = sizes[current_font].yAdvance;
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
            mode = Left;
            return;
        }
        switch (mode) {
        case Left:
        case Center:
        case Right:
            break;
        case SlideIn:
            clear(false);
            charPos = 0;
            lastPos = 0;
            slidePos = max.width();
            if (!initNextCharDimensions()) {
                mode = Left;
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
        } else if (command == "left") {
            left(args);
        } else if (command == "center") {
            center(args);
        } else if (command == "right") {
            right(args);
        } else if (command == "slidein") {
            slideIn(args);

        } else if (command == "speed") {
            setSpeed(args);

        } else if (command == "on") {
            light.set(true);
        } else if (command == "off") {
            light.set(false);
        } else if (command == "mode") {
        }
    }

    void onLightControl(bool state, double level, bool control, bool notify) {
        uint8_t intensity = (level * 15000) / 1000;
        if (control) {
            max.setIntensity(intensity);
        }
        if (control) {
            max.setPowerSave(!state);
        }
        if (notify) {
            //     char buf[32];
            //     sprintf(buf, "%5.3f", level);
            //     pSched->publish(name + "/light/unitbrightness", buf);
            pSched->publish(name + "/light/unitbrightness", String(level, 3));
            pSched->publish(name + "/light/state", state ? "on" : "off");
        }
    }

    static GFXglyph *pgm_read_glyph_ptr(const GFXfont *gfxFont, uint8_t c) {
#ifdef __AVR__
        return &(((GFXglyph *)pgm_read_pointer(&gfxFont->glyph))[c]);
#else
        // expression in __AVR__ section may generate "dereferencing type-punned
        // pointer will break strict-aliasing rules" warning In fact, on other
        // platforms (such as STM32) there is no need to do this pointer magic as
        // program memory may be read in a usual way So expression may be simplified
        return gfxFont->glyph + c;
#endif  //__AVR__
    }
};

const GFXfont *DisplayMatrixMAX72XX::default_font = nullptr;
}  // namespace ustd