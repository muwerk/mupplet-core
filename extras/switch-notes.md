## Switch

Support switches with automatic debouncing.

<img src="https://github.com/muwerk/mupplet-core/blob/master/extras/switch.png" width="50%">
Hardware: 330Ω resistor, led, switch.

#### Messages sent by switch mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/switch/state` | `on`, `off` or `trigger` | switch state, usually `on` or `off`. In modes `falling` and `rising` only `trigger`
messages are sent on rising or falling signal.
| `<mupplet-name>/switch/debounce` | <time-in-ms> | reply to `<mupplet-name>/switch/debounce/get`, switch debounce time in ms [0..1000]ms.
| `<custom-topic>` |  | `on`, `off` or `trigger` | If a custom-topic is given during switch init, an addtional message is publish on switch state changes with that topic, The message is identical to ../switch/state', usually `on` or `off`. In modes `falling` and `rising` only `trigger`.
| `<mupplet-name>/switch/shortpress` | `trigger` | Switch is in `duration` mode, and button is pressed for less than `<shortpress_ms>` (default 3000ms).
| `<mupplet-name>/switch/longpress` | `trigger` | Switch is in `duration` mode, and button is pressed for less than `<longpress_ms>` (default 30000ms), yet longer than shortpress.
| `<mupplet-name>/switch/verylongtpress` | `trigger` | Switch is in `duration` mode, and button is pressed for longer than `<longpress_ms>` (default 30000ms).
| `<mupplet-name>/switch/duration` | `<ms>` | Switch is in `duration` mode, message contains the duration in ms the switch was pressed.


#### Message received by switch mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/switch/set` | `on`, `off`, `true`, `false`, `toggle` | Override switch setting. When setting the switch state via message, the hardware port remains overridden until the hardware changes state (e.g. button is physically pressed). Sending a `switch/set` message puts the switch in override-mode: e.g. when sending `switch/set` `on`, the state of the button is signalled `on`, even so the physical button might be off. Next time the physical button is pressed (or changes state), override mode is stopped, and the state of the actual physical button is published again.  
| `<mupplet-name>/switch/mode/set` | `default`, `rising`, `falling`, `flipflop`, `timer <time-in-ms>`, `duration [shortpress_ms[,longpress_ms]]` | Mode `default` sends `on` when a button is pushed, `off` on release. `falling` and `rising` send `trigger` on corresponding signal change. `flipflop` changes the state of the logical switch on each change from button on to off. `timer` keeps the switch on for the specified duration (ms). `duration` mode sends messages `switch/shortpress`, if button was pressed for less than `<shortpress_ms>` (default 3000ms), `switch/longpress` if pressed less than `<longpress_ms>`, and `switch/verylongpress` for longer presses.
| `<mupplet-name>/switch/debounce/set` | <time-in-ms> | String encoded switch debounce time in ms, [0..1000]ms. Default is 20ms. This is especially need, when switch is created in interrupt mode (see comment in [example](https://github.com/muwerk/Examples/tree/master/led)).

### Sample code

```cpp
#define __ESP__  // or other ustd library platform define
#include "scheduler.h"
#include "light_gpio.h"
#include "switch_gpio.h"

ustd::Scheduler sched;

uint8_t channel = 0;  // only ESP32, 0..15
ustd::LightGPIO led("myLed", D5, false, channel);
// Led connected to pin D5,
// false: led is on when D5 low
// (inverted logic)
// Each led for ESP32 needs a unique PWM channel 0..15.
// messages are sent/received to myLed/light/...
// channel is ignored for all other platforms.
ustd::SwitchGPIO flipFlop("mySwitch1", D6, ustd::SwitchGPIO::Mode::Flipflop);
// Switch is set to flipflop-mode.

void appLoop() {
    // your code here...
}

void switchMsg(String topic, String message, String originator) {
    // called on switch state change
    if (message == "on") {
        // switch is on!
    } else {
        // off!
    }
}
void setup() {
    led.begin(&sched);
    led.setMode(ustd::LightController::Mode::Wave, 1000);
    // soft pwm pulsing in 1000ms intervals
    // same can be accomplished by publishing
    // topic myLed/light/setmode  msg "wave 1000"
    flipFlop.begin(&sched);

    int tid = sched.add(appLoop, "main",
                        1000000);  // call appLoop every 1sec (1000000us, change as you wish.)
    // Subscribe to switch state changes:
    sched.subscribe(tid, "mySwitch1/switch/state", switchMsg);
}

// Never add code to this loop, use appLoop() instead.
void loop() {
    sched.loop();
}
```

* See [muwerk/examples switchLed](https://github.com/muwerk/examples/tree/master/switchLed) for a complete example.
