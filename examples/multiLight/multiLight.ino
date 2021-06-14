#define __ESP__  // or other ustd library platform define
#include "scheduler.h"
#include "light_pca9685.h"
#include "mup_switch.h"

ustd::Scheduler sched;

ust::LightsPCA9685 pca("mylights", 0x40, true);
// PCA 9685 board with I2C address 0x40,
// true: lights are on when output pin is HIGH
// (active logic)
ustd::Switch flipFlop1("mySwitch1", D5, ustd::Switch::Mode::Flipflop);
ustd::Switch flipFlop2("mySwitch2", D6, ustd::Switch::Mode::Flipflop);
// Switches are set to flipflop-mode.

void appLoop() {
    // your code here...
}

void switchMsg1(String topic, String message, String originator) {
    // called on switch state change
    if (message == "on") {
        // switch is on! set pulsing wave on light channel 0
        pca.setMode(0, ustd::LightController::Mode::Wave, 1000);
        // soft pwm pulsing in 1000ms intervals
        // same can be accomplished by publishing
        // topic pca/light/0/setmode  msg "wave 1000"
    } else {
        // off!
        pca.set(0, false);
        // switch light channel 0 off
        // same can be accomplished by publishing
        // topic pca/light/0/set  msg "off"
    }
}

void switchMsg2(String topic, String message, String originator) {
    // called on switch state change
    pca.set(1, message == "on");
}

void setup() {
    pca.begin(&sched);
    flipFlop1.begin(&sched);
    flipFlop2.begin(&sched);

    // call appLoop every 1sec (1000000us, change as you wish.)
    int tid = sched.add(appLoop, "main", 1000000);

    // Subscribe to switch 1 state changes:
    sched.subscribe(tid, "mySwitch1/switch/state", switchMsg1);
    // Subscribe to switch 2 state changes:
    sched.subscribe(tid, "mySwitch2/switch/state", switchMsg2);
}

// Never add code to this loop, use appLoop() instead.
void loop() {
    sched.loop();
}
