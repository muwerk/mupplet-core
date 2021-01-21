// led.h
#pragma once

#include "scheduler.h"
#include "mup_util.h"

/* XXX to be clarified:
#include "home_assistant.h"
*/

namespace ustd {
/*! \brief mupplet-core Led class

The Led class allows control of standard Leds (or lights) with
digital and PWM control.

Supported modes are: on/off (mode: Passive), brightness (mode: Passive, with PWM),
wave (soft oszillation, mode: Wave), one-time pulse (mode: Pulse),
and repeating, user-selectable patterns (mode: Pattern).

## Sample Led Integration

\code{cpp}
#define __ESP__ 1   // Platform defines required, see ustd library doc, mainpage.
#include "scheduler.h"

ustd::Scheduler sched;
ust::Led led("myled",13);

void setup() {
    led.begin(&sched);
}
\endcode
 */
class Led {
  public:
    String LED_VERSION = "0.1.0";
    enum Mode {
        Passive, /*!< Led is controlled by API or external events, used for on/off and
                      brightness modes. No automatic state changes */
        Blink,   /*!< Blink the led with a given frequency. The led is automatically
                      controlled by the mupplet until a manual state override is given. */
        Wave,    /*!< This uses PWM to make a soft pulsing wave effect, automatic mode. */
        Pulse,   /*!< One-time pulse for a given length, automatic mode while pulse is active. */
        Pattern  /*!< Plays an (optionally) repeating pattern, e.g. to signal a
                      system state. */
    };

  private:
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t port;
    bool activeLogic = false;
    uint8_t channel;
    double brightlevel;
    bool state;
    uint16_t pwmrange;
    Mode mode;
    unsigned long interval;
    double phase = 0.0;
    unsigned long uPhase = 0;
    unsigned long oPeriod = 0;
    unsigned long startPulse = 0;
    String pattern;
    unsigned int patternPointer = 0;

#ifdef __ESP__
/* XXX: to be clarified
    HomeAssistant *pHA;
*/
#endif
  public:
    Led(String name, uint8_t port, bool activeLogic = false, uint8_t channel = 0)
        : name(name), port(port), activeLogic(activeLogic), channel(channel) {
        /*! Instantiate a Led object at a given hardware port.

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

    ~Led() {
    }

    void begin(Scheduler *_pSched, bool initialState = false) {
        /*! Initialize led hardware and start operation

        @param _pSched Pointer to a muwerk scheduler object, used to create worker
                       tasks and for message pub/sub.
        @param initialState Initial logical state of the led: false=off, true=on.
                            Note that this is independent of the physical output
                            signal on the GPIO port: if a LOW or HIGH signal is
                            required to switch the led on, is defined by the constructor's
                            activeLogic parameter.
        */
        pSched = _pSched;
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

        state = !initialState;  // Make sure set() does something.
        set(initialState);
        interval = 1000;  // Default interval for blink and pulse in ms

        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 50000);

        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/light/#", fnall);
    }

    /* XXX: to be clarified
    #ifdef __ESP__
        void registerHomeAssistant(String homeAssistantFriendlyName, String projectName = "",
                                   String homeAssistantDiscoveryPrefix = "homeassistant") {
            pHA = new HomeAssistant(name, tID, homeAssistantFriendlyName, projectName, LED_VERSION,
                                    homeAssistantDiscoveryPrefix);
            pHA->addLight();
            pHA->begin(pSched);
        }
    #endif
    */

  private:
    void setOn() {
        this->state = true;
        brightlevel = 1.0;
        if (activeLogic) {
#ifdef __ESP32__
            ledcWrite(channel, pwmrange);
#else
            digitalWrite(port, true);
#endif
        } else {
#ifdef __ESP32__
            ledcWrite(channel, 0);
#else
            digitalWrite(port, false);
#endif
        }
    }
    void setOff() {
        brightlevel = 0.0;
        this->state = false;
        if (!activeLogic) {
#ifdef __ESP32__
            ledcWrite(channel, pwmrange);
#else
            digitalWrite(port, true);
#endif
        } else {
#ifdef __ESP32__
            ledcWrite(channel, 0);
#else
            digitalWrite(port, false);
#endif
        }
    }

    void set(bool state, bool _automatic) {
        if (state == this->state)
            return;
        this->state = state;
        if (!_automatic)
            mode = Mode::Passive;
        if (state) {
            setOn();
            if (!_automatic) {
                pSched->publish(name + "/light/unitbrightness", "1.0");
                pSched->publish(name + "/light/state", "on");
            }
        } else {
            setOff();
            if (!_automatic) {
                pSched->publish(name + "/light/unitbrightness", "0.0");
                pSched->publish(name + "/light/state", "off");
            }
        }
    }

  public:
    void set(bool state) {
        /*! Set led to a given logical state.

        @param state true=on, false=off.
        */
        set(state, false);
    }

    void setMode(Mode _mode, unsigned int interval_ms = 1000, double phase_unit = 0.0,
                 String pattern) {
        /*! Set led mode to given \ref Mode

        @param _mode Led \ref Mode
        @param interval_ms Duration of blink in Mode::Blink or pulse duration.
        @param phase_unit Phase difference used to synchronize different leds in Wave
                          or blink mode. A phase_unit of 0 synchronizes thes given leds.
                          phase-difference is in [0.0-1.0]. A phase difference of 0.5
                          (180 degrees) between two leds would let leds blink
                          reversed.
        @param pattern Only in Mode::Pattern: a pattern string consisting of the
                       characters '+' (on), '-' (off), '0'-'9' (brightness 0%-100%), or
                       at the end of the string 'r' for endless repeat. Intervall_ms is
                       the time for each pattern step. Example "++-r" with intervall_ms=100
                       lights the led for 200ms on, 100ms off and repeats. "1---------r" makes
                       a faint 100ms flash every second. "0135797531r" simulates a PWM wave.
        */
        mode = _mode;
        if (mode == Mode::Passive)
            return;
        phase = phase_unit;
        if (phase < 0.0)
            phase = 0.0;
        if (phase > 1.0)
            phase = 1.0;
        interval = interval_ms;
        if (interval < 100)
            interval = 100;
        if (interval > 100000)
            interval = 100000;
        startPulse = millis();
        uPhase = (unsigned long)(2.0 * (double)interval * phase);
        oPeriod = (millis() + uPhase) % interval;
        if (mode == Mode::Pattern) {
            Pattern = pattern;
            patternPointer = 0;
        }
    }

  private:
    void publishState() {
        if (brightlevel > 0.0) {
            pSched->publish(name + "/light/state", "on");
            this->state = true;
        } else {
            pSched->publish(name + "/light/state", "off");
            this->state = false;
        }
        char buf[32];
        sprintf(buf, "%5.3f", brightlevel);
        pSched->publish(name + "/light/unitbrightness", buf);
    }

    void brightness(double bright, bool _automatic = false) {
        uint16_t bri;

        if (!_automatic)
            mode = Mode::Passive;
        if (bright == 1.0) {
            set(true, _automatic);
            return;
        }
        if (bright == 0.0) {
            set(false, _automatic);
            return;
        }

        if (bright < 0.0)
            bright = 0.0;
        if (bright > 1.0)
            bright = 1.0;
        brightlevel = bright;
        bri = (uint16_t)(bright * (double)pwmrange);
        if (!bri)
            state = false;
        else
            state = true;
        if (!activeLogic)
            bri = pwmrange - bri;
#if defined(__ESP32__)
        ledcWrite(channel, bri);
#else
        analogWrite(port, bri);
#endif
        if (!_automatic) {
            publishState();
        }
    }

  public:
    void brightness(double bright) {
        brightness(bright, false);
    }

  private:
    void loop() {
        if (mode == Mode::Passive)
            return;
        unsigned long period = (millis() + uPhase) % (2 * interval);
        if (mode == Mode::Pulse) {
            if (millis() - startPulse < interval) {
                set(true, true);
            } else {
                set(false, true);
                setMode(Mode::Passive);
            }
        }
        if (mode == Mode::Blink) {
            if (period < oPeriod) {
                set(false, true);
            } else {
                if (period > interval && oPeriod < interval) {
                    set(true, true);
                }
            }
        }
        if (mode == Mode::Wave) {
            unsigned long period = (millis() + uPhase) % (2 * interval);
            double br = 0.0;
            if (period < interval) {
                br = (double)period / (double)interval;
            } else {
                br = (double)(2 * interval - period) / (double)interval;
            }
            brightness(br, true);
        }
        if (mode == Mode::Pattern) {
            if (period < oPeriod) {
                if (patternPointer < pattern.length()) {
                    char c = pattern[patternPointer];
                    if (c == 'r') {
                        patternPointer = 0;
                        c = pattern[patternPointer];
                    }
                    if (c == '+') {
                        set(true, true);
                    }
                    if (c == '-') {
                        set(false, true);
                    }
                    if (c >= '0' && c <= '9') {
                        double br = (double)(c - '0') * 0.1111;
                        brightness(br, true);
                    }
                    ++patternPointer;
                } else {
                    patternPointer = 0;
                    set(false, true);
                    setMode(Mode::Passive);
                }
            }
        }
        oPeriod = period;
    }

    void subsMsg(String topic, String msg, String originator) {
        char msgbuf[128];
        memset(msgbuf, 0, 128);
        strncpy(msgbuf, msg.c_str(), 127);
        if (topic == name + "/light/set") {
            double br;
            br = parseUnitLevel(msg);
            brightness(br);
        }
        if (topic == name + "/light/mode/set") {
            char *p = strchr(msgbuf, ' ');
            char *p2 = nullptr;
            char *p3 = nullptr;
            if (p) {
                *p = 0;
                ++p;
                p2 = strchr(p, ',');
                if (p2) {
                    *p2 = 0;
                    ++p2;
                    p3 = strchr(p2, ',');
                    if (p3) {
                        *p3 = 0;
                        ++p3;
                    }
                }
            }
            int t = 1000;
            double phs = 0.0;
            if (!strcmp(msgbuf, "passive")) {
                setMode(Mode::Passive);
            } else if (!strcmp(msgbuf, "pulse")) {
                if (p)
                    t = atoi(p);
                setMode(Mode::Pulse, t);
            } else if (!strcmp(msgbuf, "blink")) {
                if (p)
                    t = atoi(p);
                if (p2)
                    phs = atof(p2);
                setMode(Mode::Blink, t, phs);
            } else if (!strcmp(msgbuf, "wave")) {
                if (p)
                    t = atoi(p);
                if (p2)
                    phs = atof(p2);
                setMode(Mode::Wave, t, phs);
            } else if (!strcmp(msgbuf, "pattern")) {
                if (p && strlen(p) > 0) {
                    pattern = String(p);
                    if (p2)
                        t = atoi(p2);
                    if (p3)
                        phs = atof(p3);
                    setMode(Mode::Pattern, t, phs, pattern);
                }
            }
        }
        if (topic == name + "/light/unitbrightness/get") {
            publishState();
        }
    };
};  // Led

}  // namespace ustd
