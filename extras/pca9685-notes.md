PCA 9685 16 Channels Light Mupplet
==================================

Allows to control up to 16 separate LEDs or generic lights via a PCA 9685 based board.

<img src="https://github.com/muwerk/mupplet-core/blob/master/extras/light-pca9685.png" width="30%" height="30%">

Hardware: PCA9685 based I2C 16 channel board, 100Ω resistors, leds.

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

Example: sending an MQTT message with topic `<mupplet-name>/light/12/mode/set` and message
`wave 1000` causes the light connected to channel 12 to softly pulse between on and off every
1000ms.

Multiple lights are time and phase synchronized.

### Sample code

Please update the [ustd library platform define](https://github.com/muwerk/ustd#platform-defines) for your hardware.

```cpp
#define __ESP__   // or other ustd library platform define
#include "scheduler.h"
#include "mup_light_pca9685.h"

ust::LightsPCA9685 pca("mylights", 0x41, true);
            // PCA 9685 board with I2C address 0x41,
            // true: lights are on when output pin is HIGH
            // (active logic)

void setup() {
    pca.begin(&sched);
    pca.setMode(12, ustd::LightController::Mode::Wave, 1000);
            // soft pwm pulsing in 1000ms intervals for light on channel 12
            // same can be accomplished by publishing
            // topic mylights/light/12/mode/set  msg "wave 1000"
}
```

For a complete example, see [multiLight](https://github.com/muwerk/mupplet-core/blob/master/examples/multiLight/multiLight.ino)
