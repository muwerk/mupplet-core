// mup_digital_out.h - muwerk digital out applet
#pragma once

#include "scheduler.h"
#include "mupplet_core.h"

namespace ustd {

// clang-format off
/*! \brief mupplet-core DigitalOut class

The DigitalOut class allows to integrate external relays or similar hardware
that can be switched on or off.

## Messages

### Messages sent by the switch mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/<topic>/state` | `on`, `off` | switch state, `on` or `off`.

### Message received by the switch mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/<topic>/set` | `on`, `off` | Switches the GPIO output accordingly.

## Sample digital out Integration

\code{cpp}
#define __ESP__ 1   // Platform defines required, see ustd library doc, mainpage.
#include "scheduler.h"
#include "mup_digital_out.h"

ustd::Scheduler sched;
ust::DigitalOut relay("myRelay",13);

void setup() {
    relay.begin(&sched);
}
\endcode

More information:
<a href="https://github.com/muwerk/mupplet-core/blob/master/extras/digital_out-notes.md">Digital out application
notes</a>
*/
// clang-format on
class DigitalOut {
  public:
#if USTD_FEATURE_MEMORY > USTD_FEATURE_MEM_512B
    static const char *version;  // = "0.2.0";
#endif

  private:
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t port;
    bool activeLogic = false;
    const char *topic;
    bool state;

  public:
    DigitalOut(String name, uint8_t port, bool activeLogic = false, const char *topic = "relay")
        : name(name), port(port), activeLogic(activeLogic), topic(topic) {
        /*! Instantiate a DigitalOut object

        @param name Unique name of this mupplet, appears in pub/sub messages
        @param port GPIO port number
        @param activeLogic true: calling \ref set with true generate HIGH level (active high),
                           false: calling \ref set with true generates LOW level (active low)
        @param topic Topic name of the device, default value is "relay"
         */
    }

    ~DigitalOut() {
    }

    void begin(Scheduler *_pSched) {
        /*! Initialize GPIO and start operation

        @param _pSched Pointer to Scheduler object, used for internal task and pub/sub.
        */
        pSched = _pSched;
        pinMode(port, OUTPUT);

        setOff();
#if USTD_FEATURE_MEMORY > USTD_FEATURE_MEM_512B
        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 50000);
        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/" + topic + "/#", fnall);
#endif
    }

    void set(bool state) {
        /*! set assciated GPIO according to activeLogic defined in \ref begin

        @param state logical state (is inverse to actual GPIO level, if activeLogic=false)
        */
        if (this->state == state)
            return;
        if (state)
            setOn();
        else
            setOff();
#if USTD_FEATURE_MEMORY > USTD_FEATURE_MEM_512B
        publishState();
#endif
    }

  private:
    void setOn() {
        state = true;
        if (activeLogic) {
            digitalWrite(port, true);
        } else {
            digitalWrite(port, false);
        }
    }
    void setOff() {
        state = false;
        if (activeLogic) {
            digitalWrite(port, false);
        } else {
            digitalWrite(port, true);
        }
    }

#if USTD_FEATURE_MEMORY > USTD_FEATURE_MEM_512B
    void publishState() {
        if (state) {
            pSched->publish(name + "/" + topic + "/state", "on");
        } else {
            pSched->publish(name + "/" + topic + "/state", "off");
        }
    }
#endif

#if USTD_FEATURE_MEMORY > USTD_FEATURE_MEM_512B
    void loop() {
    }
#endif

#if USTD_FEATURE_MEMORY > USTD_FEATURE_MEM_512B
    void subsMsg(String topic, String msg, String originator) {
        char msgbuf[128];
        memset(msgbuf, 0, 128);
        strncpy(msgbuf, msg.c_str(), 127);
        msg.toLowerCase();
        if (topic == name + "/" + topic + "/set") {
            set((msg == "on" || msg == "1"));
        }
    }
#endif
};  // DigitalOut

// version
#if USTD_FEATURE_MEMORY > USTD_FEATURE_MEM_512B
const char *DigitalOut::version = "0.2.0";
#endif

}  // namespace ustd
