#define __ESP__  // or other ustd library platform define
#include "scheduler.h"
#include "light_gpio.h"

ustd::Scheduler sched;

uint8_t channel = 0;  // only ESP32, 0..15
ustd::LightGPIO led("myLed", 13, false, channel);
// Led connected to pin D5,
// false: led is on when D5 low
// (inverted logic)
// Each led for ESP32 needs a unique PWM channel 0..15.
// messages are sent/received to myLed/light/...
// channel is ignored for all other platforms.

void appLoop() {
    // your code here...
}

void setup() {
    led.begin(&sched);
    led.setMode(ustd::LightController::Mode::Wave, 1000);
    // soft pwm pulsing in 1000ms intervals
    // same can be accomplished by publishing
    // topic myLed/light/setmode  msg "wave 1000"
    sched.add(appLoop, "main",
              1000000);  // call appLoop every 1sec (1000000us, change as you wish.)
}

// Never add code to this loop, use appLoop() instead.
void loop() {
    sched.loop();
}
