// light_pca9685.h - muwerk 16 channel light applet using PCA 9685 PWM controller
#pragma once

#include "muwerk.h"
#include "mupplet_core.h"
#include "helper/light_controller.h"
#include <Adafruit_PWMServoDriver.h>

namespace ustd {
// clang-format off
/*! \brief mupplet-core PCA 9685 Light class

The PCA9685 16 channel light class allows control of standard leds (or lights) with digital
and PWM control.

Supported modes are: on/off (mode: Passive), brightness (mode: Passive, with PWM), wave (soft
oszillation, mode: Wave), one-time pulse (mode: Pulse), and repeating, user-selectable patterns
(mode: Pattern).

## Messages

As all light mupplets, the mupplet can be controlled using the standard light messages:

### Light Related Messages sent by Mupplet:

| Topic | Message Body | Description
| ----- | ------------ | -------
| `<mupplet-name>/light/<channel>/unitbrightness` | normalized brightness [0.0-1.0] | `0.34`: Float value encoded as string. Not send on automatic changes (e.g. pulse mode)
| `<mupplet-name>/light/<channel>/state` | `on` or `off` | Current light state (`on` is not sent on pwm intermediate values)

### Light Related Messages received by Mupplet:

| Topic | Message Body | Description
| ----- | ------------ | -------
| `<mupplet-name>/light/<channel>/set` | `on`, `off`, `true`, `false`, `pct 34`, `34%`, `0.34` | Light can be set fully on or off with on/true and off/false. A fractional brightness of 0.34 (within interval [0.0, 1.0]) can be sent as either `pct 34`, or `0.34`, or `34%`.
| `<mupplet-name>/light/<channel>/mode/set` | `passive`, `pulse <duration_ms>`, `blink <intervall_ms>[,<phase-shift>]`, `pattern <pattern>[,<intervall>[,<phase>]]` or `wave <intervall_ms>[,<phase-shift>]` | Mode passive does no automatic light state changes, `pulse` switches the light on for `<duration_ms>` ms, then light goes back to passive mode. `blink` changes the light state very `interval_ms` on/off, `wave` uses pwm to for soft changes between on and off states. Optional comma-speratated phase [0.0, ..., 1.0] can be added as a phase-shift. Two lights, one with `wave 1000` and one with `wave 1000,0.5` blink inverse. Patterns can be specified as string containing `+`,`-`,`0`..`9` or `r`. `+` is light on during `<intervall>` ms, `-` is off, `0`..`9` brightness-level. An `r` at the end of the pattern repeats the pattern. `"pattern +-+-+-+++-+++-+++-+-+-+---r,100"` lets the board signal SOS.

## Sample Integration

\code{cpp}
#define __ESP__ 1   // Platform defines required, see ustd library doc, mainpage.
#include "scheduler.h"
#include "mup_light_pca9685.h"

ustd::Scheduler sched;
ust::LightsPCA9685 pca("mylights");

void setup() {
    pca.begin(&sched);
}
\endcode

More information:
<a href="https://github.com/muwerk/mupplet-core/blob/master/extras/pca9685-notes.md">PCA 9685 Light Application
Notes</a>
*/
// clang-format on
class LightsPCA9685 {
  public:
    static const char *version;  // = "0.1.0";

  private:
    // muwerk task management
    Scheduler *pSched;
    int tID;

    // device configuration
    String name;

    // hardware
    Adafruit_PWMServoDriver *pPwm;
    uint8_t addr;

    // runtime
    LightController light[16];
    bool activeLogic;

  public:
    /*! Instantiate a PCA9685 16 channel light object at a given address.

    No hardware interaction is performed, until \ref begin() is called.

    @param name Name of the light, used to reference it by pub/sub messages
    @param addr I2C device address. Defaults to 0x40
    @param activeLogic Characterizes the pysical logic-level which would turn
                       the light on. Default is 'false', which assumes the light
                       turns on if logic level at the corresponding port is LOW. Change
                       to 'true', if led is turned on by physical logic level HIGH.
    */
    LightsPCA9685(String name, uint8_t addr = 0x40, bool activeLogic = false)
        : name(name), addr(addr), activeLogic(activeLogic) {
    }

    /*! Initialize PCA9685 hardware and start operation

    @param _pSched Pointer to a muwerk scheduler object, used to create worker
                   tasks and for message pub/sub.
    @param _pWire Optional pointer to a configured 'TwoWire' object. If not specified, the global
                  object `Wire` is used.
    @param initialState Initial logical state of the light: false=off, true=on.
                        Note that this is independent of the physical output
                        signal on the hardware port: if a LOW or HIGH signal is
                        required to switch the light on, is defined by the constructor's
                        'activeLogic' parameter.
    */
    void begin(Scheduler *_pSched, TwoWire *_pWire = nullptr, bool initialState = false) {
        // standard muwerk task initialization
        pSched = _pSched;
        tID = pSched->add([this]() { this->loop(); }, name, 80000L);

        // initialize hardware
        pPwm = new Adafruit_PWMServoDriver(addr, _pWire == nullptr ? Wire : *_pWire);
        pPwm->begin();
        pPwm->setPWMFreq(1000);

        // subscribe to light messages and pass to light controller
        pSched->subscribe(tID, name + "/light/#", [this](String topic, String msg, String orig) {
            topic = topic.substring(name.length() + 7);
            int iPos = topic.indexOf('/');
            if (iPos == -1) {
                // invalid topic
                return;
            }
            long index = ustd::parseLong(topic.substring(0, iPos), -1);
            if (index < 0 || index > 15) {
                // invalid topic
                return;
            }
            this->light[index].commandParser(topic.substring(iPos + 1), msg);
        });

        // start light controller (register hardware implementation)
        for (uint8_t channel = 0; channel < 16; channel++) {
            light[channel].begin(
                [this, channel](bool state, double level, bool control, bool notify) {
                    this->onLightControl(channel, state, level, control, notify);
                },
                initialState);
        }
    }

    /*! Set light to a given logical state.
    @param channel Channel number of light to set (from 0 to 15) or -1 for all channels
    @param state State of the light: true=on, false=off.
    */
    void set(int8_t channel, bool state) {
        if (channel < 0) {
            for (channel = 0; channel < 16; channel++) {
                light[channel].set(state);
            }
        } else if (channel < 16) {
            light[channel].set(state);
        }
    }

    /*! Set light mode to given \ref LightController::Mode
    @param channel Channel number of light to set mode (from 0 to 15) or -1 for all channels
    @param mode Light \ref LightController::Mode
    @param interval_ms Duration of blink in Mode::Blink or pulse duration.
    @param phase_unit Phase difference used to synchronize different lights in Wave
                      or blink mode. A phase_unit of 0 synchronizes the given lights.
                      phase-difference is in [0.0-1.0]. A phase difference of 0.5
                      (180 degrees) between two lights would let lights blink
                      reversed.
    @param pattern Only in Mode::Pattern: a pattern string consisting of the
                   characters '+' (on), '-' (off), '0'-'9' (brightness 0%-100%), or
                   at the end of the string 'r' for endless repeat. Intervall_ms is
                   the time for each pattern step. Example "++-r" with intervall_ms=100
                   lights the led for 200ms on, 100ms off and repeats. "1---------r" makes
                   a faint 100ms flash every second. "0135797531r" simulates a PWM wave.
    */
    void setMode(int8_t channel, LightController::Mode mode, unsigned int interval_ms = 1000,
                 double phase_unit = 0.0, String pattern = "") {
        if (channel < 0) {
            for (channel = 0; channel < 16; channel++) {
                light[channel].setMode(mode, interval_ms, phase_unit, pattern);
            }
        } else if (channel < 16) {
            light[channel].setMode(mode, interval_ms, phase_unit, pattern);
        }
    }

    /*! Set minimum and maximum brightness in wave \ref LightController::Mode

    Useful to compensate, if a light stays at similar brightness for a range of input values.

    @param channel Channel number of light to configure (from 0 to 15) or -1 for all channels
    @param minBrightness Minimum brightness 0-1.0
    @param maxBrightness Maximum brightness 0-1.0
    */
    void setMinMaxWaveBrightness(int8_t channel, double minBrightness, double maxBrightness) {
        if (channel < 0) {
            for (channel = 0; channel < 16; channel++) {
                light[channel].setMinMaxWaveBrightness(minBrightness, maxBrightness);
            }
        } else if (channel < 16) {
            light[channel].setMinMaxWaveBrightness(minBrightness, maxBrightness);
        }
    }

  private:
    void loop() {
        for (int channel = 0; channel < 16; channel++) {
            light[channel].loop();
        }
    }

    void onLightControl(uint8_t channel, bool state, double level, bool control, bool notify) {
        if (control) {
            uint16_t intensity = level * 4096;
            if (intensity == 0 || state == false) {
                // GPIO mode off
                gpioSet(channel, false);
            } else if (intensity == 4096) {
                // GPIO mode on
                gpioSet(channel, true);
            } else {
                // PWM mode
                pwmSet(channel, intensity);
            }
        }
        if (notify) {
            pSched->publish(name + "/light/" + String(channel) + "/unitbrightness",
                            String(level, 3));
            pSched->publish(name + "/light/" + String(channel) + "/state", state ? "on" : "off");
        }
    }

    void gpioSet(uint8_t channel, bool on) {
        if (activeLogic && on) {
            pPwm->setPWM(channel, 4096, 0);
        } else if (activeLogic) {
            pPwm->setPWM(channel, 0, 4096);
        } else if (on) {
            pPwm->setPWM(channel, 0, 4096);
        } else {
            pPwm->setPWM(channel, 4096, 0);
        }
    }

    void pwmSet(uint8_t channel, uint16_t intensity) {
        if (activeLogic) {
            pPwm->setPWM(channel, 0, intensity);
        } else {
            pPwm->setPWM(channel, 0, 4096 - intensity);
        }
    }
};

const char *LightsPCA9685::version = "0.1.0";

}  // namespace ustd