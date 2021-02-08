// max72xx.h - fork of markruys/arduino-Max72xxPanel with some improvements

#pragma once

#include <SPI.h>
#include <Adafruit_GFX.h>

namespace ustd {

class max72xx : public Adafruit_GFX {
  private:
    // hardware configuration
    uint8_t csPin;

    // runtime - pixel and module logic
    uint8_t hDisplays;
    uint8_t *bitmap;
    uint8_t bitmapSize;
    uint8_t *matrixPosition;
    uint8_t *matrixRotation;

    // runtime - max79xx operation type
    enum OP {
        noop = 0,
        digit0 = 1,
        digit1 = 2,
        digit2 = 3,
        digit3 = 4,
        digit4 = 5,
        digit5 = 6,
        digit6 = 7,
        digit7 = 8,
        decodemode = 9,
        intensity = 10,
        scanlimit = 11,
        shutdown = 12,
        displaytest = 15
    };

  public:
    max72xx(uint8_t csPin = D8, uint8_t hDisplays = 1, uint8_t vDisplays = 1, uint8_t rotation = 0)
        : Adafruit_GFX(hDisplays << 3, vDisplays << 3), csPin(csPin), hDisplays(hDisplays) {
        uint8_t displays = hDisplays * vDisplays;
        bitmapSize = displays * 8;
        bitmap = (uint8_t *)malloc(bitmapSize + (2 * displays));
        matrixPosition = bitmap + bitmapSize;
        matrixRotation = matrixPosition + displays;

        for (uint8_t display = 0; display < displays; display++) {
            matrixPosition[display] = display;
            matrixRotation[display] = rotation;
        }
    }

    virtual ~max72xx() {
        free(bitmap);
    }

    /*! Start the display matrix
     */
    void begin() {
        if (bitmap != nullptr) {
            // multiple init management is done inside the SPI library
            SPI.begin();
            // SPI.setBitOrder(MSBFIRST);
            // SPI.setDataMode(SPI_MODE0);
            pinMode(csPin, OUTPUT);

            // Clear the screen
            fillScreen(0);

            // Make sure we are not in test mode
            spiTransfer(OP::displaytest, 0);

            // We need the multiplexer to scan all segments
            spiTransfer(OP::scanlimit, 7);

            // We don't want the multiplexer to decode segments for us
            spiTransfer(OP::decodemode, 0);

            // write to screen
            write();
        }
    }

    /*! Set the power saving mode for the device
     * @param status If true the device goes into power-down mode. Set to false for normal
     * operation.
     */
    void setPowerSave(bool status) {
        if (bitmap != nullptr) {
            spiTransfer(OP::shutdown, status ? 0 : 1);
        }
    }

    /*! Set the brightness of the display.
     * @param intensity the brightness of the display. (0..15)
     */
    void setIntensity(uint8_t intensity) {
        if (bitmap != nullptr) {
            spiTransfer(OP::intensity, intensity > 15 ? 15 : intensity);
        }
    }

    /*! After you're done filling the bitmap buffer with your picture,
     * send it to the display(s).
     */
    void write() {
        if (bitmap != nullptr) {
            for (uint8_t row = OP::digit7; row >= OP::digit0; row--) {
                spiTransfer((OP)row);
            }
        }
    }

    /*! Define how the displays are ordered.
     * @param display Display index (0 is the first)
     * @param x horizontal index
     * @param y vertical index
     */
    void setPosition(uint8_t display, uint8_t x, uint8_t y) {
        if (bitmap != nullptr) {
            matrixPosition[x + hDisplays * y] = display;
        }
    }

    /*! Define if and how the displays are rotated.
     * @param display Display index (0 is the first)
     * @param rotation Rotation of the display:
     *   0: no rotation
     *   1: 90 degrees clockwise
     *   2: 180 degrees
     *   3: 90 degrees counter clockwise
     */
    void setRotation(uint8_t display, uint8_t rotation) {
        if (bitmap != nullptr) {
            matrixRotation[display] = rotation;
        }
    }

    void getCharBounds(unsigned char c, int16_t *x, int16_t *y, int16_t *minx, int16_t *miny,
                       int16_t *maxx, int16_t *maxy) {
        charBounds(c, x, y, minx, miny, maxx, maxy);
    }

  public:
    // overrides of virtual functions
    virtual void fillScreen(uint16_t color) {
        /*
         * As we can do this much faster then setting all the pixels one by
         * one, we have a dedicated function to clear the screen.
         * The color can be 0 (blank) or non-zero (pixel on).
         */
        if (bitmap != nullptr) {
            memset(bitmap, color ? 0xff : 0, bitmapSize);
        }
    }

    void drawPixel(int16_t xx, int16_t yy, uint16_t color) {
        /*
         * Draw a pixel on your canvas. Note that for performance reasons,
         * the pixels are not actually send to the displays. Only the internal
         * bitmap buffer is modified.
         */
        if (bitmap == nullptr) {
            return;
        }

        // Operating in bytes is faster and takes less code to run. We don't
        // need values above 200, so switch from 16 bit ints to 8 bit unsigned
        // ints (bytes).
        // Keep xx as int16_t so fix 16 panel limit
        int16_t x = xx;
        byte y = yy;
        byte tmp;

        if (rotation) {
            // Implement Adafruit's rotation.
            if (rotation >= 2) {
                // rotation == 2 || rotation == 3
                x = _width - 1 - x;
            }

            if (rotation == 1 || rotation == 2) {
                // rotation == 1 || rotation == 2
                y = _height - 1 - y;
            }

            if (rotation & 1) {
                // rotation == 1 || rotation == 3
                tmp = x;
                x = y;
                y = tmp;
            }
        }

        if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
            // Ignore pixels outside the canvas.
            return;
        }

        // Translate the x, y coordinate according to the layout of the
        // displays. They can be ordered and rotated (0, 90, 180, 270).

        byte display = matrixPosition[(x >> 3) + hDisplays * (y >> 3)];
        x &= 0b111;
        y &= 0b111;

        byte r = matrixRotation[display];
        if (r >= 2) {
            // 180 or 270 degrees
            x = 7 - x;
        }
        if (r == 1 || r == 2) {
            // 90 or 180 degrees
            y = 7 - y;
        }
        if (r & 1) {
            // 90 or 270 degrees
            tmp = x;
            x = y;
            y = tmp;
        }

        byte d = display / hDisplays;
        x += (display - d * hDisplays) << 3;  // x += (display % hDisplays) * 8
        y += d << 3;                          // y += (display / hDisplays) * 8

        // Update the color bit in our bitmap buffer.

        byte *ptr = bitmap + x + WIDTH * (y >> 3);
        byte val = 1 << (y & 0b111);

        if (color) {
            *ptr |= val;
        } else {
            *ptr &= ~val;
        }
    }

  private:
    void spiTransfer(OP opcode, uint8_t data = 0) {
        // If opcode > OP::digit7, send the opcode and data to all displays.
        // If opcode <= OP::digit7, display the column with data in our buffer for all displays.
        // We do not support (nor need) to use the OP_NOOP opcode.

        // Enable the line
        digitalWrite(csPin, LOW);

        // Now shift out the data, two bytes per display. The first byte is the opcode,
        // the second byte the data.
        uint16_t end = opcode - OP::digit0;
        uint16_t start = bitmapSize + end;
        do {
            start -= 8;
            SPI.transfer(opcode);
            SPI.transfer(opcode <= OP::digit7 ? bitmap[start] : data);
        } while (start > end);

        // Latch the data onto the display(s)
        digitalWrite(csPin, HIGH);
    }
};
}  // namespace ustd