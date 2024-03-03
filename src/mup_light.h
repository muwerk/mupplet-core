// mup_light.h - muwerk light applet

#pragma once

#include "helper/light_controller.h"
#include "scheduler.h"

namespace ustd {
// clang-format off
/*! \brief mupplet-core GPIO Light class

The GPIO Light class allows control of standard Leds (or lights) with digital and PWM control.

Supported modes are: on/off (mode: Passive), brightness (mode: Passive, with PWM), wave (soft
oszillation, mode: Wave), one-time pulse (mode: Pulse), and repeating, user-selectable patterns
(mode: Pattern).

## Messages

As all light mupplets, the mupplet can be controlled using the standard light messages:

### Light Related Messages sent by Mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/light/unitbrightness` | normalized brightness [0.0-1.0] | `0.34`: Float value encoded as string. Not send on automatic changes (e.g. pulse mode)
| `<mupplet-name>/light/state` | `on` or `off` | current led state (`on` is not sent on pwm intermediate values)

### Light Related Messages received by Mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/light/set` | `on`, `off`, `true`, `false`, `pct 34`, `34%`, `0.34` | Led can be set fully on or off with on/true and off/false. A fractional brightness of 0.34 (within interval [0.0, 1.0]) can be sent as either `pct 34`, or `0.34`, or `34%`.
| `<mupplet-name>/light/mode/set` | `passive`, `pulse <duration_ms>`, `blink <intervall_ms>[,<phase-shift>]`, `pattern <pattern>[,<intervall>[,<phase>]]` or `wave <intervall_ms>[,<phase-shift>]` | Mode passive does no automatic led state changes, `pulse` switches the led on for `<duration_ms>` ms, then led goes back to passive mode. `blink` changes the led state very `interval_ms` on/off, `wave` uses pwm to for soft changes between on and off states. Optional comma-speratated phase [0.0, ..., 1.0] can be added as a phase-shift. Two leds, one with `wave 1000` and one with `wave 1000,0.5` blink inverse. Patterns can be specified as string containing `+`,`-`,`0`..`9` or `r`. `+` is led on during `<intervall>` ms, `-` is off, `0`..`9` brightness-level. An `r` at the end of the pattern repeats the pattern. `"pattern +-+-+-+++-+++-+++-+-+-+---r,100"` lets the board signal SOS.

## Sample Led Integration

\code{cpp}
#define __ESP__ 1   // Platform defines required, see ustd library doc, mainpage.
#include "scheduler.h"
#include "mup_light.h"

ustd::Scheduler sched;
ust::Light led("myled",13);

void setup() {
    led.begin(&sched);
}
\endcode

More information:
<a href="https://github.com/muwerk/mupplet-core/blob/master/extras/led-notes.md">Led application
notes</a>
*/
// clang-format on

class Light {
  public:
    static const char *version;  // = "0.1.0";

  private:
    // muwerk task management
    Scheduler *pSched;
    int tID;

    // device configuration
    String name;

    // hardware configuration
    uint8_t port;
    bool activeLogic = false;
    uint16_t pwmrange;
    uint8_t channel;

  public:
    LightController light;
    
    Light(String name, uint8_t port, bool activeLogic = false, uint8_t channel = 0)
        : name(name), port(port), activeLogic(activeLogic), channel(channel) {
        /*! Instantiate a GPIO Led object at a given hardware port.

        No hardware interaction is performed, until \ref begin() is called.

        @param name Name of the led, used to reference it by pub/sub messages
        @param port GPIO port number, needs to be a PWM enabled pin, if brightness,
                    Pulse-mode or Pattern-mode with brightness levels are used.
        @param activeLogic Characterizes the pysical logicl-level which would turn
                           the led on. Default is 'false', which assumes the led
                           turns on if logic level at the GPIO port is LOW. Change
                           to 'true', if led is turned on by physical logic level HIGH.
        @param channel currently ESP32 only, can be ignored for all other platforms.
                       ESP32 requires assignment of a system-wide unique channel number
                       (0..15) for each led in the system. So for ESP32 both the GPIO port
                       number and a unique channel id are required.
        */
    }

    /** Initialize GPIO hardware and start operation
     *
     * @param _pSched Pointer to a muwerk scheduler object, used to create worker tasks and for
     *                message pub/sub.
     * @param initialState Initial logical state of the light: false=off, true=on.
     *                     Note that this is independent of the physical output
     *                     signal on the GPIO port: if a LOW or HIGH signal is
     *                     required to switch the light on, is defined by the constructor's
     *                     `activeLogic` parameter.
     */
    void begin(Scheduler *_pSched, bool initialState = false) {
        pSched = _pSched;
        tID = pSched->add([this]() { this->light.loop(); }, name, 50000L);

        pSched->subscribe(tID, name + "/light/#", [this](String topic, String msg, String orig) {
            this->light.commandParser(topic.substring(name.length() + 7), msg);
        });

        // prepare hardware
#if defined(__ESP32__)
        pinMode(port, OUTPUT);
// use first channel of 16 channels (started from zero)
#define LEDC_TIMER_BITS 10
// use 5000 Hz as a LEDC base frequency
#define LEDC_BASE_FREQ 5000
        ledcSetup(channel, LEDC_BASE_FREQ, LEDC_TIMER_BITS);
        ledcAttachPin(port, channel);
#else
        pinMode(port, OUTPUT);
#endif
#ifdef __ESP__
        pwmrange = 1023;
#else
        pwmrange = 255;
#endif

        // start light controller
        light.begin([this](bool state, double level, bool control,
                           bool notify) { this->onLightControl(state, level, control, notify); },
                    initialState);
    }

    /** Set light to a given logical state.
     * @param state State of the light: true=on, false=off.
     */
    void set(bool state) {
        light.set(state);
    }

    /** Set light mode to given \ref LightController::Mode
     * @param mode Light \ref LightController::Mode
     * @param interval_ms Duration of blink in Mode::Blink or pulse duration.
     * @param phase_unit Phase difference used to synchronize different lights in Wave
     *                   or blink mode. A phase_unit of 0 synchronizes the given lights.
     *                   phase-difference is in [0.0-1.0]. A phase difference of 0.5
     *                   (180 degrees) between two lights would let lights blink
     *                   reversed.
     * @param pattern Only in Mode::Pattern: a pattern string consisting of the
     *                characters '+' (on), '-' (off), '0'-'9' (brightness 0%-100%), or
     *                at the end of the string 'r' for endless repeat. Intervall_ms is
     *                the time for each pattern step. Example "++-r" with intervall_ms=100
     *                lights the led for 200ms on, 100ms off and repeats. "1---------r" makes
     *                a faint 100ms flash every second. "0135797531r" simulates a PWM wave.
     */
    void setMode(LightController::Mode mode, unsigned int interval_ms = 1000,
                 double phase_unit = 0.0, String pattern = "") {
        light.setMode(mode, interval_ms, phase_unit, pattern);
    }

    /** Set minimum and maximum brightness in wave \ref LightController::Mode
     *
     * Useful to compensate, if a light stays at similar brightness for a range of input values.
     *
     * @param minBrightness Minimum brightness 0-1.0
     * @param maxBrightness Maximum brightness 0-1.0
     */
    void setMinMaxWaveBrightness(double minBrightness, double maxBrightness) {
        light.setMinMaxWaveBrightness(minBrightness, maxBrightness);
    }

#ifdef USTD_FEATURE_HOMEASSISTANT
    /** Adds an entity to HomeAsisstant for a specific channel
     * @param pHass Pointer to the HomeAssistant Device Autodiscovery Helper
     * @param human Optional human readable name for HomeAssistant entity
     * @param icon Optional alternative icon
     * @param attribs Optional alternative attribute group (by default all entities reference the
     * @param dimmable Optional dimmable mode (default: `true`)
     * "device" attributes group)
     */
    void registerHomeAssistant(HomeAssistant *pHass, String human = "", String icon = "",
                               String attribs = "", bool dimmable = true) {
        pHass->addLight(name, human, dimmable ? HomeAssistant::LightDim : HomeAssistant::Light,
                        icon, attribs);
    }
#endif
  private:
    void onLightControl(bool state, double level, bool control, bool notify) {
        if (control) {
            if (state && level == 1.0) {
                // led is on at maximum brightness
#ifdef __ESP32__
                ledcWrite(channel, activeLogic ? pwmrange : 0);
#else
                digitalWrite(port, activeLogic ? HIGH : LOW);
#endif
            } else if (state && level > 0.0) {
                // led is dimmed
                uint16_t bri = (uint16_t)(level * (double)pwmrange);
                if (bri) {
                    if (!activeLogic)
                        bri = pwmrange - bri;
#if defined(__ESP32__)
                    ledcWrite(channel, bri);
#else
                    analogWrite(port, bri);
#endif
                } else {
                    light.forceState(false, 0.0);
                    onLightControl(false, 0.0, control, notify);
                }
            } else {
                // led is off
#ifdef __ESP32__
                ledcWrite(channel, activeLogic ? 0 : pwmrange);
#else
                digitalWrite(port, activeLogic ? LOW : HIGH);
#endif
            }
        }
        if (notify) {
#ifdef __ARDUINO__
            pSched->publish(name + "/light/unitbrightness", String(level, 3));
#else
            char buf[32];
            sprintf(buf, "%5.3f", level);
            pSched->publish(name + "/light/unitbrightness", buf);
#endif
            pSched->publish(name + "/light/state", state ? "on" : "off");
        }
    }
};

const char *Light::version = "0.1.0";

}  // namespace ustd