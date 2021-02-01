[![Dev Docs](https://img.shields.io/badge/docs-dev-blue.svg)](https://muwerk.github.io/mupplet-core/docs/index.html)

**Note:** This project is WIP. Functionality from [Research-mupplets](https://github.com/muwerk/Research-mupplets)
is being refactored and ported into this project, which will be published as Arduino- and Platformio-Libraries soon.

mupplet-core
============

**mu**werk a**pplet**s; mupplets: functional units that support specific hardware or reusable
applications.

**mupplets** use muwerks MQTT-style messaging to pass information between each other on the
same device. If connected to an MQTT-server via munet, all functionallity is externally
available through an MQTT server such as Mosquitto.

The `mupplet-core` library consists of the following modules:

<img src="https://github.com/muwerk/mupplet-core/blob/master/extras/led.png" align="right" width="7%" height="7%">

* [`LightGPIO`](https://muwerk.github.io/mupplet-core/docs/classustd_1_1LightGPIO.html) The Light
  GPIO mupplet allows to control a led or light's state, brightness using modes such as
  user-controlled, blink, soft-wave, one-time pulse and automatic pattern playback.
  See [Led application notes](https://github.com/muwerk/mupplet-core/blob/master/extras/led-notes.md)
  for an example and more information. Complete example [muBlink](https://github.com/muwerk/examples/tree/master/muBlink).

<img src="https://github.com/muwerk/mupplet-core/blob/master/extras/switch.png" align="right" width="10%" height="10%">

* [`SwitchGPIO`](https://muwerk.github.io/mupplet-core/docs/classustd_1_1SwitchGPIO.html) The Switch
  GPIO mupplet allows to use hardware buttons and switches connected to a GPIO. Switch supports debouncing,
  flip-flop mode and push-button event. See [Switch application notes]( https://github.com/muwerk/mupplet-core/blob/master/extras/switch-notes.md)
  for an example-code and more information. Complete example [SwitchAndLed](https://github.com/muwerk/examples/tree/master/SwitchAndLed).

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
  partners: the implementation of both mupplets in both cases is simply the same.

### Examples

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
- 0.1.0 (2021-01-XX) [not yet released] Base Framework

References
----------

- [ustd](https://github.com/muwerk/ustd) microWerk standard library
- [muwerk](https://github.com/muwerk/muwerk) microWerk scheduler
- [munet](https://github.com/muwerk/muwerk) microWerk networking
