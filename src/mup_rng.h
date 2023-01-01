// mup_rng.h
#pragma once

#include "scheduler.h"

namespace ustd {

#if defined(__ESP32__) || defined(__ESP__)
#define G_INT_ATTR IRAM_ATTR
#else
#define G_INT_ATTR
#endif

#define USTD_MAX_FQ_PIRQS (10)

#define USTD_ENTROPY_POOL_SIZE (4096)
uint8_t entropy_pool[USTD_ENTROPY_POOL_SIZE];
int entropy_pool_read_ptr = 0;
int entropy_pool_write_ptr = 0;
uint8_t currentByte = 0;
int currentBitPtr = 0;
uint8_t currentBit = 0;

// volatile unsigned long pFqIrqCounter[USTD_MAX_FQ_PIRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
// volatile unsigned long pFqLastIrqTimer[USTD_MAX_FQ_PIRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile unsigned long pFqBeginIrqTimer[USTD_MAX_FQ_PIRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

void G_INT_ATTR ustd_fq_pirq_master(uint8_t irqno) {
    unsigned long curr = micros();
    noInterrupts();  // TBD: is this safe on ESP32?
    if (pFqBeginIrqTimer[irqno] == 0)
        pFqBeginIrqTimer[irqno] = curr;
    else {
        unsigned long delta = timeDiff(pFqBeginIrqTimer[irqno], curr);
        currentBit = delta % 2;
        // TBD: option for von Neumann entropy extractor, depending on circuit used.
        currentByte = (currentByte << 1) | currentBit;
        currentBitPtr++;
        if (currentBitPtr == 8) {
            entropy_pool[entropy_pool_write_ptr] = currentByte;
            entropy_pool_write_ptr = (entropy_pool_write_ptr + 1) % USTD_ENTROPY_POOL_SIZE;
            currentBitPtr = 0;
            currentByte = 0;
        }
        pFqBeginIrqTimer[irqno] = curr;
    }
    interrupts();
}

void G_INT_ATTR ustd_fq_pirq0() {
    ustd_fq_pirq_master(0);
}
void G_INT_ATTR ustd_fq_pirq1() {
    ustd_fq_pirq_master(1);
}
void G_INT_ATTR ustd_fq_pirq2() {
    ustd_fq_pirq_master(2);
}
void G_INT_ATTR ustd_fq_pirq3() {
    ustd_fq_pirq_master(3);
}
void G_INT_ATTR ustd_fq_pirq4() {
    ustd_fq_pirq_master(4);
}
void G_INT_ATTR ustd_fq_pirq5() {
    ustd_fq_pirq_master(5);
}
void G_INT_ATTR ustd_fq_pirq6() {
    ustd_fq_pirq_master(6);
}
void G_INT_ATTR ustd_fq_pirq7() {
    ustd_fq_pirq_master(7);
}
void G_INT_ATTR ustd_fq_pirq8() {
    ustd_fq_pirq_master(8);
}
void G_INT_ATTR ustd_fq_pirq9() {
    ustd_fq_pirq_master(9);
}

void (*ustd_fq_pirq_table[USTD_MAX_FQ_PIRQS])() = {ustd_fq_pirq0, ustd_fq_pirq1, ustd_fq_pirq2, ustd_fq_pirq3,
                                                   ustd_fq_pirq4, ustd_fq_pirq5, ustd_fq_pirq6, ustd_fq_pirq7,
                                                   ustd_fq_pirq8, ustd_fq_pirq9};

unsigned long getRandomData(uint8_t irqNo, uint8_t *pBuf, unsigned long len) {
    if (len > USTD_ENTROPY_POOL_SIZE)
        len = USTD_ENTROPY_POOL_SIZE;
    noInterrupts();
    unsigned long n = 0;
    for (unsigned long i = 0; i < len; i++) {
        pBuf[i] = entropy_pool[entropy_pool_read_ptr];
        ++n;
        entropy_pool_read_ptr = (entropy_pool_read_ptr + 1) % USTD_ENTROPY_POOL_SIZE;
    }
    interrupts();
    return n;
}

// clang-format off
/*! Interrupt driven random noise to random bytes converter

TBD.

## Messages

### Messages sent by the switch mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/sensor/rng/data` | `TBD` | Some data format

### Message received by the switch mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/sensor/rng/data/get` | `<count>` | Request `count` bytes of random data

*/

class Rng {
  public:
    /*! Interrupt mode that triggers the counter */
    enum InterruptMode { IM_RISING, /*!< trigger on rising signal (LOW->HIGH) */
                         IM_FALLING, /*!< trigger on falling signal */
                         IM_CHANGE  /*!< trigger on both rising and falling signal */
                         };
    String RNG_VERSION = "0.1.0";
  private:
    Scheduler *pSched;
    int tID;

    String name;
    uint8_t pin_input;
    uint8_t irqno_input;
    int8_t interruptIndex_input;
    InterruptMode irqMode;
    uint8_t ipin = 255;
    bool irqsAttached = false;

  public:

    Rng(String name, uint8_t pin_input, int8_t interruptIndex_input,
                     InterruptMode irqMode = InterruptMode::IM_FALLING)
        : name(name), pin_input(pin_input), interruptIndex_input(interruptIndex_input),
          irqMode(irqMode) {
              /*! Create a RNG generator
                @param name Name of mupplet, used in message topics
                @param pin_input GPIO of input signal
                @param interruptIndex_input a system wide unique index for the interrupt to be used [0...9]
                @param irqMode \ref InterruptMode, e.g. IM_RISING.
              */
    }

    ~Rng() {
        if (irqsAttached) {
            detachInterrupt(irqno_input);
        }
    }

    bool begin(Scheduler *_pSched, uint32_t scheduleUs=2000000L) {
        /*! Enable interrupts and start counter
            
        @param _pSched Pointer to scheduler
        @param scheduleUs Measurement schedule in microseconds
        @return true if successful
        */
        pSched = _pSched;

        pinMode(pin_input, INPUT_PULLUP);

        if (interruptIndex_input >= 0 && interruptIndex_input < USTD_MAX_FQ_PIRQS) {
            irqno_input = digitalPinToInterrupt(pin_input);
            switch (irqMode) {
            case IM_FALLING:
                attachInterrupt(irqno_input, ustd_fq_pirq_table[interruptIndex_input], FALLING);
                break;
            case IM_RISING:
                attachInterrupt(irqno_input, ustd_fq_pirq_table[interruptIndex_input], RISING);
                break;
            case IM_CHANGE:
                attachInterrupt(irqno_input, ustd_fq_pirq_table[interruptIndex_input], CHANGE);
                break;
            }
            irqsAttached = true;
        } else {
            return false;
        }

        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, scheduleUs);  // uS schedule

        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/#", fnall);
        return true;
    }

  private:
    void publish_rng_data() {
        // TBD
        char buf[32];
        sprintf(buf, "RNG Data here");
        pSched->publish(name + "/sensor/rng/data", buf);
    }

    void publish() {
        publish_rng_data();
    }

    void loop() {
       // TBD 
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/sensor/rng/state/get") {
            publish();
        } else if (topic == name + "/sensor/rng/data/get") {
            publish_rng_data();
        } 
    };
};  // Rng

}  // namespace ustd
