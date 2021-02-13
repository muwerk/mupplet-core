LightGPIO Mupplet
=================

Allows to control LEDs or generic lights via digital logic or PWM brightness.

<img src="https://github.com/muwerk/mupplet-core/blob/master/extras/led.png" width="30%" height="30%">

Hardware: 330Ω resistor, led.

#### Messages sent by LightGPIO mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/light/unitbrightness` | normalized brightness [0.0-1.0] | `0.34`: Float value encoded as string. Not send on automatic changes (e.g. pulse mode)
| `<mupplet-name>/light/state` | `on` or `off` | current led state (`on` is not sent on pwm intermediate values)

#### Message received by LightGPIO mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/light/set` | `on`, `off`, `true`, `false`, `pct 34`, `34%`, `0.34` | Led can be set fully on or off with on/true and off/false. A fractional brightness of 0.34 (within interval [0.0, 1.0]) can be sent as either `pct 34`, or `0.34`, or `34%`.
| `<mupplet-name>/light/mode/set` | `passive`, `pulse <duration_ms>`, `blink <intervall_ms>[,<phase-shift>]`, `pattern <pattern>[,<intervall>[,<phase>]]` or `wave <intervall_ms>[,<phase-shift>]` | Mode passive does no automatic led state changes, `pulse` switches the led on for `<duration_ms>` ms, then led goes back to passive mode. `blink` changes the led state very `interval_ms` on/off, `wave` uses pwm to for soft changes between on and off states. Optional comma-speratated phase [0.0, ..., 1.0] can be added as a phase-shift. Two leds, one with `wave 1000` and one with `wave 1000,0.5` blink inverse. Patterns can be specified as string containing `+`,`-`,`0`..`9` or `r`. `+` is led on during `<intervall>` ms, `-` is off, `0`..`9` brightness-level. An `r` at the end of the pattern repeats the pattern. `"pattern +-+-+-+++-+++-+++-+-+-+---r,100"` lets the board signal SOS.

Example: sending an MQTT message with topic `<led-name>/mode/set` and message `wave 1000` causes the led to softly pulse between on and off every 1000ms.

Multiple leds are time and phase synchronized.

### Sample code

Please update the [ustd library platform define](https://github.com/muwerk/ustd#platform-defines) for your hardware.

```cpp
#define __ESP__   // or other ustd library platform define
#include "muwerk.h"
#include "light_gpio.h"

uint8_t channel=0; // only ESP32, 0..15
ustd::LightGPIO led("myLed", 13, false, channel);
            // Led connected to pin D5,
            // false: led is on when D5 low
            // (inverted logic)
            // Each led for ESP32 needs a unique PWM channel 0..15.
            // messages are sent/received to myLed/light/...
            // channel is ignored for all other platforms.

void setup() {
    led.begin(&sched);
    led.setmode(LightController::Mode::Wave, 1000);
            // soft pwm pulsing in 1000ms intervals
            // same can be accomplished by publishing
            // topic myLed/light/setmode  msg "wave 1000"
}
```
