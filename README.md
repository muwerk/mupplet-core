[![PlatformIO CI][image_CI]][badge_CI] [![Dev Docs][image_DOC]][badge_DOC]

**Note:** This project, while being released, is still WIP. Additional functionality from
[Research-mupplets](https://github.com/muwerk/Research-mupplets) is being refactored and
ported into this project.

mupplet-core
============

**mu**werk a**pplet**s; mupplets: functional units that support specific hardware or reusable
applications.

**mupplets** use muwerks MQTT-style messaging to pass information between each other on the
same device. If connected to an MQTT-server via munet, all functionallity is externally
available through an MQTT server such as Mosquitto.

The `mupplet-core` library consists of the following modules:

<img src="https://github.com/muwerk/mupplet-core/blob/master/extras/led.png" align="right" width="7%" height="7%">

- [`Light`][Light_DOC] The Light GPIO mupplet allows to control a led or light's state, brightness
  using modes such as user-controlled, blink, soft-wave, one-time pulse and automatic pattern
  playback. See [Led application notes][Light_NOTES] for an example and more information.
  Complete example [muBlink](https://github.com/muwerk/examples/tree/master/muBlink).

<img src="https://github.com/muwerk/mupplet-core/blob/master/extras/switch.png" align="right" width="10%" height="10%">

- [`Switch`][Switch_DOC] The Switch GPIO mupplet allows to use hardware buttons and switches
  connected to a GPIO. Switch supports debouncing, flip-flop mode and push-button event. See
  [Switch application notes][Switch_NOTES] for an example-code and more information. Complete
  example [SwitchAndLed](https://github.com/muwerk/examples/tree/master/SwitchAndLed).

<img src="https://github.com/muwerk/examples/blob/master/Resources/FrequencyCounter.jpg" align="right" width="8%">

- [`FrequencyCounter`][FrequencyCounter_DOC] The frequency counter mupplet measures impulse frequencies
  on a GPIO using interrupts. An ESP32 can reliably measure up to about 80kHz. See
  [FrequencyCounter application notes][FrequencyCounter_NOTES] for a Geiger counter sample.

- [`LightsPCA9685`][PCA9685_DOC] The PCA 9685 16 Channels Light Mupplet allows to control up to
  16 led or light states, brightness using modes such as user-controlled, blink, soft-wave,
  one-time pulse and automatic pattern playback. See [PCA9685 Application Notes][PCA9685_NOTES]
  for an example and more information.

- [`Home Assistant`][HomeAssistant_DOC]. Almost any mupplet can be integrated easily into home-assistant,
  supported are Home Assistant MQTT Switch, Binary Sensor, Sensor, and Light. The `home_assistant` mupplet
  handles the entire discovery- and synchronization process. For examples see 
  [leds and switches with home-assistant](https://github.com/muwerk/examples/tree/master/switchLedHomeassistant) or 
  [simple binary- and analog sensors](https://github.com/muwerk/examples/tree/master/binaryAndAnalogSimpleSensors) or 
  [multi-sensor environment monitoring](https://github.com/muwerk/examples/tree/master/enviroMaster).

Additional mupplet implementations can be found:

- [`mupplet-sensor`](https://github.com/muwerk/mupplet-sensor): implementations for various sensors, temperature,
  humidity, pressure, illuminance, generic binary sensors and analog sensors.
- [`mupplet-display`](https://github.com/muwerk/mupplet-display): implementations for various displays.

Development and Design considerations
-------------------------------------

Some design recommendations for mupplets:

* mupplets use asynchronous design. Since muwerk uses cooperative scheduling, any
  muwerk task can block the muwerk system. It's therefore imperative to use asychronous
  programming techniques with mupplets. Do not use long-running code with delays. Many
  third-party sensor libraries are not designed for asynchronous operation, and thus
  will block the muwerk system more than necessary.
* If you interact with hardware or software that requires waiting for events, either
  use a state-machine or interrupt handlers, and give back control to the muwerk scheduler
  as soon as possible. Avoid polling for events in a delay-loop.
* As a rule of thumb: avoid blocking operations > 10ms.
* Use muwerks pub/sub mechanism to interface between other mupplets and the outside world.
  Compose using messages and functional interfaces with light coupling that do not impose
  object hierarchies on the programs that use a mupplet.
* If your mupplet exposes an API, make sure that the same functionality is accessible via
  pub/sub messages.
* Mupplets compose with pub/sub messages. If a mupplet uses functionality of another
  Mupplet, they should use pub/sub messages. Example: a CO2 sensor-mupplet requires the current
  temperature for calibration: the CO2 mupplet would subscribe to the messages a temperature
  mupplet would publish. A major advantage of this type of light coupling via messages is
  that the temperature mupplet could run on a different hardware: an MQTT server and munet
  make sure that there is absolutely no difference between remote and local communication
  partners: the implementation of both mupplets in both cases is simply the same. Additionally
  this facilitates a later port to multi-core and preemtive multi-tasking systems.

### Examples ###

* A mupplet can implement a driver for a hardware sensor (e.g. temperature sensor)
* If a hardware is for a very specific purpose, even application-style logic can be
  part of a mupplet. E.g. a 4-digit 7-segment clock display with a colon between
  2nd and 3rd digit is basically suited to display time. In additional to implement
  the basic hardware the mupplet could implement the functions of a basic clock
* The clock mupplet could (optionally) subscribe to the messages a button publishes
  to implement functions like "stop alarm".
* munet's network stacks are basically mupplets themselves.

Structure
---------

An example framework for a mupplet:

```c++
#define __ESP__         // Platform define, see ustd library
#include <ustd_platform.h>   // ustd library
#include <muwerk.h>     // muwerk library

namespace ustd {
class MyMupplet {
  public:
    String name;
    uint8_t addr;  // some hardware address that cannot change
    Scheduler *pSched;  // pointer to muwerk scheduler object, used to start
                        // tasks and to pub/sub messages;
    bool bActive=false; // set to true if conditions for operation are met
                        // (e.g. hardware found and initialized.)
    void MyMupplet(String name): name(name), addr(addr) {
        // Minimal code in constructor, just save invariants like the mupplet's
        // name and immutable hardware parameters.
    }

    void begin(Scheduler *_pSched) {
        pSched=_pSched;  // save muwerk pointer

        // now intialize hard- and software that is used with this mupplet,
        // e.g. initialize a sensor.
        // If things go ok:
        bActive=true;

        // Initialize the worker "thread":
        auto loopfunc = [=]() { this->loop(); };
        int tID = pSched->add(loopfunc, name, 100000);
        // Scheduler will now call loop() every 100ms (100000us).

        // Subscribe to one or more topics to expose the mupplet's
        // functionality
        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/light/#", fnall);
    }

    // loop is called by the scheduler according to the setting in begin()
    void loop() {
        // Implement the main functionality here.
        // Avoid blocking or polling as much as possible
        if (bActive) { // Initialization in setup() was successful
            // calculation... [state-machine]
            // We got a result, lets publish!
            pSched->publish(name+"/state", "Some data here");
            // Other instances that have subscribed to <name>/state will receive
            // "Some data here"
        }
    }

    // subsMsg is called by the scheduler, if a message that has been
    // subscribed is available
    void subsMsg(String topic, String msg, String originator) {
        // A message with topic topic and content msg has been received
        // from muwerk process with name originator.
    }

}; // MyMupplet

} // ustd
```


History
-------
- 0.5.0 (2022-09-22) NOT YET RELEASED
  * New HomeAssistant Device Autodiscovery Helper
  * New 16 channel light applet using PCA 9685 PWM controller
  * Number in string detection functions
  * Support for ASCII part HD44780 displays charset conversions (see: [mupplet-display](https://github.com/muwerk/mupplet-display))
  * count option for switch to count switch events.
  * Astro helper module for sunrise sunset calculations (WIP!)
- 0.4.0 (2021-02-28) Frequency counter
  * Frequency counter mupplet (Frequency measurement up to 80kHz on ESP32)
  * Codepage conversion utility functions (ISO8859-1 <-> UTF8)
  * New parser function `parseLong`
  * Improvements in parser functions
- 0.3.0 (2021-02-19) New helpful parser functions
- 0.2.0 (2021-02-13) Consistent naming, documentation fixes, configurable topic in `DigitalOut`.
- 0.1.1 (2021-02-13) Broken repository-url fixed for `library.json`.
- 0.1.0 (2021-02-13) Mupplets for basic I/O, leds and switches.

More mupplet libraries
----------------------

- [mupplet-sensor][gh_mupsensor] microWerk Sensor mupplets
- [mupplet-display][gh_mupdisplay] microWerk Display mupplets

References
----------

- [ustd][gh_ustd] microWerk standard library
- [muwerk][gh_muwerk] microWerk scheduler
- [munet][gh_munet] microWerk networking

[badge_CI]: https://github.com/muwerk/mupplet-core/actions
[image_CI]: https://github.com/muwerk/mupplet-core/workflows/PlatformIO%20CI/badge.svg
[badge_DOC]: https://muwerk.github.io/mupplet-core/docs/index.html
[image_DOC]: https://img.shields.io/badge/docs-dev-blue.svg

[Light_DOC]: https://muwerk.github.io/mupplet-core/docs/classustd_1_1Light.html
[Light_NOTES]: https://github.com/muwerk/mupplet-core/blob/master/extras/led-notes.md
[Switch_DOC]: https://muwerk.github.io/mupplet-core/docs/classustd_1_1Switch.html
[Switch_NOTES]: https://github.com/muwerk/mupplet-core/blob/master/extras/switch-notes.md
[FrequencyCounter_DOC]: https://muwerk.github.io/mupplet-core/docs/classustd_1_1FrequencyCounter.html
[FrequencyCounter_NOTES]: https://github.com/muwerk/mupplet-core/blob/master/extras/frequency-counter-notes.md
[DigitalOut_DOC]: https://muwerk.github.io/mupplet-core/docs/classustd_1_1DigitalOut.html
[DigitalOut_NOTES]: https://github.com/muwerk/mupplet-core/blob/master/extras/digital-out-notes.md
[PCA9685_DOC]: https://muwerk.github.io/mupplet-core/docs/classustd_1_1LightsPCA9685.html
[PCA9685_NOTES]: https://github.com/muwerk/mupplet-core/blob/master/extras/pca9685-notes.md
[HomeAssistant_DOC]: https://muwerk.github.io/mupplet-core/docs/classustd_1_1HomeAssistant.html

[gh_ustd]: https://github.com/muwerk/ustd
[gh_muwerk]: https://github.com/muwerk/muwerk
[gh_munet]: https://github.com/muwerk/munet
[gh_mufonts]: https://github.com/muwerk/mufonts
[gh_mupcore]: https://github.com/muwerk/mupplet-core
[gh_mupdisplay]: https://github.com/muwerk/mupplet-display
[gh_mupsensor]: https://github.com/muwerk/mupplet-sensor
