// light_controller.h - controller for light related operations

#pragma once

#include "ustd_platform.h"
#include "mupplet_core.h"

namespace ustd {

/*! \brief The Light Controller Class
 *
 * This class is useful to implement mupplets for things that behave like a light. It
 * supports switching the unit on and off and setting the light intensity.
 * Addionally automatic light effects are supported (see \ref Mode and \ref setMode)
 */
class LightController {
  public:
    /*! The light operation mode */
    enum Mode {
        Passive, /*! Light is controlled by API or external events, used for on/off and
                     brightness modes. No automatic state changes */
        Blink,   /*! Blink the light with a given frequency. The light is automatically
                     controlled by the mupplet until a manual state override is given. */
        Wave,    /*! This modulates the light with a soft pulsing wave effect, automatic mode. */
        Pulse,   /*! One-time pulse for a given length, automatic mode while pulse is active. */
        Pattern  /*! Plays an (optionally) repeating pattern, e.g. to signal a system state. */
    };

    /*! \brief Hardware Control Function
     *
     * The hardware control function is called every time when the state of the light shall
     * change or when the state shall be notified.
     *
     * @param state The logical state of the light: `true` if the light is on, `false` if the light
     *              is off.
     * @param level The brightness level of the light. The value ranges between 0.0 and 1.0
     * @param control If `true` the hardware shall be set to the supplied values.
     * @param notify If `true` the current state and brightness level shall be notified to whom
     *               it may concern.
     */
#if defined(__ESP__) || defined(__UNIXOID__)
    typedef std::function<void(bool state, double level, bool control, bool notify)> T_CONTROL;
#elif defined(__ATTINY__)
    typedef void (*T_CONTROL)(bool state, double level, bool control, bool notify);
#else
    typedef ustd::function<void(bool state, double level, bool control, bool notify)> T_CONTROL;
#endif

  private:
    // controller state
    Mode mode;
    bool state;
    double brightlevel;
    // configuration
    T_CONTROL controller;
    unsigned long interval = 1000;
    double phase = 0.0;
    double minWaveBrightness = 0.0;
    double maxWaveBrightness = 1.0;
    String pattern = "";
    // runtime
    unsigned long uPhase = 0;
    unsigned long oPeriod = 0;
    unsigned long startPulse = 0;
    unsigned int patternPointer = 0;

  public:
    LightController() {
        /*! Instantiates a LightController instance
         *
         * No hardware interaction is performed, until \ref begin() is called.
         */
    }

    void begin(T_CONTROL controller, bool initialState = false) {
        /*! Initiate operation
         *
         * @param controller The controller function that controls the hardware and publishes state
         *                   messages.
         * @param initialState Initial logical state of the light.
         */
        this->controller = controller;
        mode = Passive;
        state = !initialState;
        set(initialState);
    }

    void loop() {
        /*! The loop method
         * This function **must** be called in the loop method of the mupplet. In order
         * to get smoth effects, this function should be called every 50ms.
         */
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
            br = br * (maxWaveBrightness - minWaveBrightness) + minWaveBrightness;
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

    bool commandParser(String command, String args) {
        /*! A command parser method for the light

        This function acceps **commands** and their optional arguments. Usually commands in
        mupplets are sent be publishing messages to specific topics containing the command as the
        last part of the topic. The most efficient method for processing commands from message
        subscriptions would be:

        \code{.cpp}
        pSched->subscribe(tID, name + "/light/#", [this](String topic, String msg, String orig) {
            this->light.commandParser(topic.substring(name.length() + 7), msg);
        });
        \endcode

        If the topic `<muppletname>/light/` shoudl support additonal commands, a typical
        implementation would be like:

        \code{.cpp}
        pSched->subscribe(tID, name + "/light/#", [this](String topic, String msg, String orig) {
            topic = topic.substring(name.length() + 7);

            if (this->light.commandParser(topic, msg)) {
                // command was processed in light controller
                return;
            }

            if (topic == "mycommand") {
                // do my stuff...
            } else if (topic == "myothercommand") {
                // do my other stuff
            }
        });
        \endcode

        The following commands are supported:

        * `set` - set the light on/off or to a specific intensity
        * `mode/set` - change the mode - see \ref setMode for details
        * `unitbrightness/get` - notify the current status

        @param command The command to parse
        @param args The arguments to the command
        */
        if (command == "set") {
            double br;
            br = parseUnitLevel(args);
            brightness(br);
            return true;
        }
        if (command == "mode/set") {
            char msgbuf[128];
            memset(msgbuf, 0, 128);
            strncpy(msgbuf, args.c_str(), 127);
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
                    if (p2)
                        t = atoi(p2);
                    if (p3)
                        phs = atof(p3);
                    setMode(Mode::Pattern, t, phs, p);
                }
            }
            return true;
        }
        if (command == "unitbrightness/get") {
            controller(state, brightlevel, false, true);
            return true;
        }
        return false;
    }

    void set(bool state) {
        // clang-format off
        /*! Set light to a given logical state.
        A light mupplet usually sends the following messages on state change:
        | topic                     | message body                    | comment
        | ------------------------- | ------------------------------- | -------
        | `<prefix>/unitbrightness` | normalized brightness [0.0-1.0] | `0.34`: Float value encoded as string. Not send on automatic changes (e.g. pulse mode)
        | `<prefix>/state`          | `on` or `off`                   | Current light state (`on` is not sent on brightness intermediate values)
        @param state true=on, false=off.
        */
        // clang-format on
        set(state, false);
    }

    void brightness(double level) {
        /*! Set light brighness level
        @param level Brightness level [0.0 (off) - 1.0 (on)]
        */
        brightness(level, false);
    }

    void setMode(Mode mode, unsigned int interval_ms = 1000, double phase_unit = 0.0,
                 String pattern = "") {
        /*! Set light mode to given \ref Mode
        @param mode Light \ref Mode
        @param interval_ms Duration of blink in Mode::Blink or pulse duration.
        @param phase_unit Phase difference used to synchronize different lights in Wave
                          or blink mode. A phase_unit of 0 synchronizes the given lights.
                          phase-difference is in [0.0-1.0]. A phase difference of 0.5
                          (180 degrees) between two leds would let leds blink
                          reversed.
        @param pattern Only in Mode::Pattern: a pattern string consisting of the
                       characters '+' (on), '-' (off), '0'-'9' (brightness 0%-100%), or
                       at the end of the string 'r' for endless repeat. Intervall_ms is
                       the time for each pattern step. Example "++-r" with intervall_ms=100
                       lights the led for 200ms on, 100ms off and repeats. "1---------r" makes
                       a faint 100ms flash every second. "0135797531r" simulates a wave.
        */
        this->mode = mode;
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
            this->pattern = pattern;
            patternPointer = 0;
        }
    }

    void setMinMaxWaveBrightness(double minBrightness, double maxBrightness) {
        /*! Set minimum and maximum brightness in wave \ref Mode
        Useful to compensate, if a light stays at similar brightness for a range of input values.
        @param minBrightness Minimum brightness 0-1.0
        @param maxBrightness Maximum brightness 0-1.0
        */
        if (minBrightness < 0.0 || minBrightness > 1.0)
            minBrightness = 0.0;
        if (maxBrightness < 0.0 || maxBrightness > 1.0)
            maxBrightness = 1.0;
        if (minBrightness >= maxBrightness) {
            minBrightness = 0.0;
            maxBrightness = 1.0;
        }
        minWaveBrightness = minBrightness;
        maxWaveBrightness = maxBrightness;
    }

    void forceState(bool state, double brightlevel) {
        /*! Force the internal state and brightness to a specific value.
         Useful to fix the internal state in case the hardware is not able to display the requested
         state. This can usually happen if the brightness is so low that the hardware switches off.
         e.g.: if a led is moduled via a PWM with only 256 steps, it may happen that a very low
         brightness will result in the PWM value 0 -> the led is switched off. The implementation
         will than force the state to `false, 0.0`:

        \code{.cpp}
            ...
            } else if (state && level > 0.0) {
                // led is dimmed
                uint16_t bri = (uint16_t)(level * (double)pwmrange);
                if (bri) {
                    if (!activeLogic)
                        bri = pwmrange - bri;
                    ledcWrite(channel, bri);
                } else {
                    light.forceState(false, 0.0);
                    onLightControl(false, 0.0, control, notify);
                }
            } else {
            ...
        \endcode

         @param state Logical state of the light: true=on, false=off.
         @param brightlevel Brightness level of the light [0.0 (off) - 1.0 (on)]
         */
        this->state = state;
        this->brightlevel = brightlevel < 0.0 ? 0.0 : brightlevel > 1.0 ? 1.0 : brightlevel;
    }

  private:
    void set(bool _state, bool _automatic) {
        if (_state == state)
            return;
        if (!_automatic)
            mode = Mode::Passive;
        // if we want to preserve the brightness level, we need to change this
        brightlevel = _state ? 1.0 : 0.0;
        state = _state;
        controller(state, brightlevel, true, !_automatic);
    }

    void brightness(double _brightlevel, bool _automatic) {
        if (_brightlevel < 0.0)
            _brightlevel = 0.0;
        if (_brightlevel > 1.0)
            _brightlevel = 1.0;
        if (brightlevel == _brightlevel)
            return;
        if (!_automatic)
            mode = Mode::Passive;
        brightlevel = _brightlevel;
        state = _brightlevel > 0.0;
        controller(state, brightlevel, true, !_automatic);
    }
};

}  // namespace ustd