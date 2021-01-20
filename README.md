mupplet-core
============

**mu**werk a**pplet**s; mupplets: functional units that support specific hardware or reusable applications.

**mupplets** use muwerks MQTT-style messaging to pass information between each other on the same device. If connected to an MQTT-server via munet, all functionallity is externally available through an MQTT server such as Mosquitto.

Some design recommendations for mupplets:

* mupplets use asynchronous design. Since muwerk uses cooperative scheduling, any
muwerk task can block the muwerk system. It's therefore imperative to use asychronous
programming techniques with mupplets. Do not use long-running code with delays. Many
third-party sensor libraries are not designed for asynchronous operation, and thus
will block the muwerk system more than necessary.
* If you interact with hardware or software that requires waiting for events, either
use a state-machine or interrupt handlers, and give back control to the muwerk scheduler as soon as possible. Avoid polling for events in a delay-loop.
* Use muwerks pub/sub mechanism to interface between other mupplets and the outside
world. Compose using messages and functional interfaces with light coupling that do not impose object hierarchies on the programs that use a mupplet.
* If your mupplet exposes an API, make sure that the same functionality is accessible via pub/sub messages.

Structure
---------

An example framework for a mupplet:

```c++
namespace ustd {
class MyMupplet {
  public:
    String name;
    uint8_t addr;  // some hardware address that cannot change
    Scheduler *pSched;  // pointer to muwerk scheduler object, used to start tasks
                        // and to pub/sub messages;
    bool bActive=false; // set to true if conditions for operation are met (e.g.
                        // hardware found and initialized.)
    void MyMupplet(String name): name(name), addr(addr) {
        // Minimal code in constructor, just save invariants like the mupplet's name
        // and immutable hardware parameters.
    }

    void begin(Scheduler *_pSched) {
        pSched=_pSched;  // save muwerk pointer

        // now intialize hard- and software that is used with this mupplet,
        // e.g. initialize a sensor. 
        // If things go ok:
        bActive=true;

        // Initialize the worker "thread":
        auto loopfunc = [=]() { this->loop(); };
        tID = pSched->add(loopfunc, name, 100000);  // Scheduler will now call
                                                    // loop() every 100ms. (100000us)

        // Subscribe to one or more topics to expose the mupplet's functionality
        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/light/#", fnall);
    }

    // loop is called by the scheduler according to the setting in begin()
    void loop() {
        // Implement the main functionality here.
        // Avoid blocking or polling as much as possible.
    }

    // subsMsg is called by the scheduler, if a message that has been subscribed
    // is available
    void subsMsg(String topic, String msg, String originator) {
        // A message with topic topic and content msg has been received from
        // muwerk process with name originator.
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
