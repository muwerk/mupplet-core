// mup_rng.h
#pragma once

#include "scheduler.h"

namespace ustd {

#if defined(__ESP32__) || defined(__ESP__)
#define G_INT_ATTR IRAM_ATTR
#else
#define G_INT_ATTR
#endif

#define USTD_MAX_RNG_PIRQS (10)

#define USTD_ENTROPY_POOL_SIZE (512)
// TBD: is is almost always a huge waste, since mostly only one instance is used, but we allocate for all (10).
volatile uint8_t entropy_pool[USTD_MAX_RNG_PIRQS][USTD_ENTROPY_POOL_SIZE];
volatile int entropy_pool_read_ptr[USTD_MAX_RNG_PIRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile int entropy_pool_write_ptr[USTD_MAX_RNG_PIRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile int entropy_pool_size[USTD_MAX_RNG_PIRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile uint8_t currentByte[USTD_MAX_RNG_PIRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile int currentBitPtr[USTD_MAX_RNG_PIRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile uint8_t bit_cnt[USTD_MAX_RNG_PIRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile uint8_t last_bit[USTD_MAX_RNG_PIRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile uint16_t crc[USTD_MAX_RNG_PIRQS] = {0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff};

volatile unsigned long irq_count_total = 0;

volatile unsigned long d_hist[256];
volatile const int maxBits = 3;

// This interrupt is triggered by the random device connected to the MCU at random time intervals.
void G_INT_ATTR ustd_rng_pirq_master(uint8_t irqno) {
    unsigned long curr = micros();  // the value of micros() is determined by the random device
                                    // that generates the interrupt.
    irq_count_total++;
    uint8_t i;
    uint8_t delta;
    if (entropy_pool_size[irqno] < USTD_ENTROPY_POOL_SIZE) {
        // CRC16 CCITT bit shuffle, input is 16 bits of micros() timer,
        // current time is determined by the chaotic behavior of the random
        // device that generates the interrupt.
        uint16_t dw = ((uint16_t)(curr & 0xffff));
        uint8_t *data_p = (uint8_t *)&dw;
        uint8_t length = 2;
        uint16_t data;
        do {
            for (i = 0, data = (unsigned int)0xff & *data_p++; i < 8; i++, data >>= 1) {
                if ((crc[irqno] & 0x0001) ^ (data & 0x0001)) {
                    crc[irqno] = (crc[irqno] >> 1) ^ 0x8408;  // 0x8408: CRC16 CCITT polynomial;
                } else {
                    crc[irqno] >>= 1;
                }
            }
        } while (--length);

        crc[irqno] = ~crc[irqno];
        data = crc[irqno];
        crc[irqno] = (crc[irqno] << 8) | (data >> 8 & 0xff);

        delta = crc[irqno] & 0xff;
        // check for delta distribution via histogram
        d_hist[delta]++;
        // only use maxBits bits of delta, the value of maxBits is determined by the
        // by the 'speed' of the random device that generates the interrupt and the
        // mcu speed. Slower devices should use less bits, faster devices can use more.
        for (int i = 0; i < maxBits; i++) {
            uint8_t curBit = (delta >> i) & 0x01;
            // von Neumann extractor
            if (bit_cnt[irqno] == 0) {
                last_bit[irqno] = curBit;
                bit_cnt[irqno]++;
            } else {
                if (last_bit[irqno] != (curBit)) {
                    currentByte[irqno] = (currentByte[irqno] << 1) | last_bit[irqno];
                    currentBitPtr[irqno]++;
                    if (currentBitPtr[irqno] == 8) {
                        entropy_pool[irqno][entropy_pool_write_ptr[irqno]] = currentByte[irqno];
                        entropy_pool_write_ptr[irqno] = (entropy_pool_write_ptr[irqno] + 1) % USTD_ENTROPY_POOL_SIZE;
                        entropy_pool_size[irqno] = (entropy_pool_size[irqno] + 1);
                        if (entropy_pool_size[irqno] > USTD_ENTROPY_POOL_SIZE)
                            entropy_pool_size[irqno] = USTD_ENTROPY_POOL_SIZE;
                        currentBitPtr[irqno] = 0;
                        currentByte[irqno] = 0;
                    }
                }
                bit_cnt[irqno] = 0;
            }
        }
    }
}

void G_INT_ATTR ustd_rng_pirq0() {
    ustd_rng_pirq_master(0);
}
void G_INT_ATTR ustd_rng_pirq1() {
    ustd_rng_pirq_master(1);
}
void G_INT_ATTR ustd_rng_pirq2() {
    ustd_rng_pirq_master(2);
}
void G_INT_ATTR ustd_rng_pirq3() {
    ustd_rng_pirq_master(3);
}
void G_INT_ATTR ustd_rng_pirq4() {
    ustd_rng_pirq_master(4);
}
void G_INT_ATTR ustd_rng_pirq5() {
    ustd_rng_pirq_master(5);
}
void G_INT_ATTR ustd_rng_pirq6() {
    ustd_rng_pirq_master(6);
}
void G_INT_ATTR ustd_rng_pirq7() {
    ustd_rng_pirq_master(7);
}
void G_INT_ATTR ustd_rng_pirq8() {
    ustd_rng_pirq_master(8);
}
void G_INT_ATTR ustd_rng_pirq9() {
    ustd_rng_pirq_master(9);
}

void (*ustd_rng_pirq_table[USTD_MAX_RNG_PIRQS])() = {ustd_rng_pirq0, ustd_rng_pirq1, ustd_rng_pirq2, ustd_rng_pirq3,
                                                     ustd_rng_pirq4, ustd_rng_pirq5, ustd_rng_pirq6, ustd_rng_pirq7,
                                                     ustd_rng_pirq8, ustd_rng_pirq9};

unsigned long getRandomData(uint8_t irqNo, uint8_t *pBuf, unsigned long len) {
    if (len > USTD_ENTROPY_POOL_SIZE)
        len = USTD_ENTROPY_POOL_SIZE;
    noInterrupts();
    if (len > entropy_pool_size[irqNo])
        len = entropy_pool_size[irqNo];
    unsigned long n = 0;
    for (unsigned long i = 0; i < len; i++) {
        pBuf[i] = entropy_pool[irqNo][entropy_pool_read_ptr[irqNo]];
        ++n;
        entropy_pool_read_ptr[irqNo] = (entropy_pool_read_ptr[irqNo] + 1) % USTD_ENTROPY_POOL_SIZE;
        entropy_pool_size[irqNo]--;
    }
    interrupts();
    return n;
}

void get_d_hist(unsigned long *pHistBuf) {
    noInterrupts();
    for (int i = 0; i < 256; i++) {
        pHistBuf[i] = d_hist[i];
        d_hist[i] = 0;
    }
    interrupts();
}

unsigned long getTotalIrqCount() {
    noInterrupts();
    unsigned long irqs = irq_count_total;
    interrupts();
    return irqs;
}

// clang-format off
/*! Interrupt driven random noise to random bytes converter

TBD.

## Messages

### Messages sent by the switch mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/rng/data` | up to 128 bytes encoded as hex (256 chars) of random data | If no random data is available, no message is sent. It not enough data is available, the message is sent with the available data. |
| `<mupplet-name>/rng/state` | `none`, `self-test`, `ok`, `failed` | State of the RNG. `none` means rng is not started, `self-test` means the RNG is in self-test mode, `ok` means the RNG is operational, `failed` means the RNG failed the self-test, indicating a hardware problem with the RNG. |
### Message received by the switch mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/rng/data/get` |  | Request a block of hex random data |
| `<mupplet-name>/rng/state/get` |  | Request the state of the RNG |
*/

class Rng {
  public:
    /*! Interrupt mode that triggers the counter */
    enum InterruptMode { IM_RISING, /*!< trigger on rising signal (LOW->HIGH) */
                         IM_FALLING, /*!< trigger on falling signal */
                         IM_CHANGE  /*!< trigger on both rising and falling signal */
                         };
    String RNG_VERSION = "0.1.1";
  private:
    Scheduler *pSched;
    int tID;

    enum RngSampleMode {RSM_NONE, RSM_SELF_TEST, RSM_OK, RSM_FAILED};
    RngSampleMode rngSampleMode = RSM_NONE;

    String name;
    uint8_t pin_input;
    uint8_t irqno_input;
    int8_t interruptIndex_input;
    InterruptMode irqMode;
    uint8_t ipin = 255;
    bool irqsAttached = false;
    unsigned long selfTestSampleSize;
    bool isFirstFailure = true;
    int8_t rngStateLedPin;
    bool rngStateLedActiveHigh;
    unsigned long rngStateBlinkTimer = 0;
    bool rngStateLedCurrentState = false;
    unsigned long lastOkMillis = 0;
    unsigned long lastIrqCount = 0;
    bool publishViaSerial = false;
  public:

    Rng(String name, uint8_t pin_input, int8_t interruptIndex_input,
                     InterruptMode irqMode = InterruptMode::IM_RISING, int8_t rngStateLedPin=-1, bool rngStateLedActiveHigh=false, unsigned long selfTestSampleSize = 25600)
        : name(name), pin_input(pin_input), interruptIndex_input(interruptIndex_input), rngStateLedPin(rngStateLedPin), rngStateLedActiveHigh(rngStateLedActiveHigh), selfTestSampleSize(selfTestSampleSize),
          irqMode(irqMode) {
              /*! Create a RNG generator
                @param name Name of mupplet, used in message topics
                @param pin_input GPIO of input signal
                @param interruptIndex_input a system wide unique index for the interrupt to be used [0...9]
                @param irqMode \ref InterruptMode, e.g. IM_RISING.
                @param rngStateLedPin GPIO of LED to indicate RNG state, -1 if no LED is used. Fast blink: self-test running, slow blink: self-test failed, on: self-test OK
                @param rngStateLedActiveHigh true if LED is active high, default is active low
                @param selfTestSampleSize Number of samples to be used for self test
              */
        if (rngStateLedPin >= 0) {
            pinMode(rngStateLedPin, OUTPUT);
            digitalWrite(rngStateLedPin, !rngStateLedActiveHigh);
        }
    }

    ~Rng() {
        if (irqsAttached) {
            detachInterrupt(irqno_input);
        }
    }

    bool begin(Scheduler *_pSched, bool _publishViaSerial, uint32_t scheduleUs=1000L) {
        /*! Start random number generation

        Random numbers can optionally be published via Serial (USB) in blocks of 40 bytes, hex encoded, with a linefeed after each 40 bytes (80 characters).
        The start of a data stream is marked by "\n===RNG-START===\n", the end (due to malfunction) by "\n===RNG-STOP===\n", and a restart again by "\n===RNG-START===\n".

        @param _pSched Pointer to scheduler
        @param _publishViaSerial true if data should be published via Serial. Start of data stream is marked by "\n===RNG-START===\n", end (due to malfunction) by "\n===RNG-STOP===\n", restart again by "\n===RNG-START===\n"
        @param scheduleUs Measurement schedule in microseconds
        @return true if successful
        */
        pSched = _pSched;
        publishViaSerial = _publishViaSerial;
        if (rngStateLedPin >= 0) {
            rngStateBlinkTimer = millis();
        }
        pinMode(pin_input, INPUT_PULLUP);

        if (interruptIndex_input >= 0 && interruptIndex_input < USTD_MAX_RNG_PIRQS) {
            irqno_input = digitalPinToInterrupt(pin_input);
            switch (irqMode) {
            case IM_FALLING:
                attachInterrupt(irqno_input, ustd_rng_pirq_table[interruptIndex_input], FALLING);
                break;
            case IM_RISING:
                attachInterrupt(irqno_input, ustd_rng_pirq_table[interruptIndex_input], RISING);
                break;
            case IM_CHANGE:
                attachInterrupt(irqno_input, ustd_rng_pirq_table[interruptIndex_input], CHANGE);
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
        pSched->subscribe(tID, name + "/rng/#", fnall);
        startSelfTest();
        return true;
    }

    unsigned long getIrqCount() {
        return getTotalIrqCount();
    }
    unsigned long getSampleCount() {
        return samples;
    }

  private:
    void byteToHex(uint8_t byte, char *buf) {
        uint8_t hc = byte >> 4;
        uint8_t lc = byte & 0x0f;
        if (hc < 10) {
            buf[0] = '0' + hc;
        } else {
            buf[0] = 'A' + hc - 10;
        }
        if (lc < 10) {
            buf[1] = '0' + lc;
        } else {
            buf[1] = 'A' + lc - 10;
        }
        buf[2] = 0;
    }

    const static int publishMax = 128;
    char buf[publishMax+1];
    char publishBuf[publishMax];
    int publishBufPtr = 0;

    void publish_rng_data() {
        if (publishBufPtr > 0) {
            for (int i = 0; i < publishBufPtr; i++) {
                byteToHex(publishBuf[i], buf + i * 2);
            }
            publishBufPtr = 0;
            pSched->publish(name + "/rng/data", buf);

        }
    }

    void publish() {
        switch (rngSampleMode) {
        case RSM_NONE:
            pSched->publish(name + "/rng/state", "none");
            break;
        case RSM_SELF_TEST:
            pSched->publish(name + "/rng/state", "self-test");
            break;
        case RSM_OK:
            pSched->publish(name + "/rng/state", "ok");
            break;
        case RSM_FAILED:
            pSched->publish(name + "/rng/state", "failed");
            break;
        }
    }

    void startSelfTest() {
        rngSampleMode = RSM_SELF_TEST;
        rngSelfTestState = RST_INIT;
    }

    enum RngSelfTestState {RST_NONE, RST_INIT, RST_RUNNING, RST_SAMPLE_DONE, RST_FAILED, RST_OK};
    unsigned long rngHistogram[256];
    const static unsigned long rngBufSize = 512;
    uint8_t rngBuf[rngBufSize];
    unsigned long dBuf[256];
    RngSelfTestState rngSelfTestState = RST_NONE;
    time_t testTimer;
    unsigned long samples;

    bool evalRngSelfTest() {
        unsigned long exp = selfTestSampleSize / 256;
        float fugde = 2.0;
        unsigned long min = (unsigned long)((float)selfTestSampleSize / 256.0 / fugde);
        unsigned long max = (unsigned long)((float)selfTestSampleSize / 256.0 * fugde);
        bool ok = true;
        #ifdef __USE_SERIAL_DBG__
        Serial.println();
        #endif
        for (int i = 0; i < 256; i++) {
            if (rngHistogram[i] < min || rngHistogram[i] > max) {
                ok = false;
                #ifdef __USE_SERIAL_DBG__
                Serial.print(i);
                Serial.print(": ");
                Serial.print(rngHistogram[i]);
                Serial.print(" FAIL! ");
                #endif
            } else {
                #ifdef __USE_SERIAL_DBG__
                Serial.print(i);
                Serial.print(": ");
                Serial.print(rngHistogram[i]);
                Serial.print(" OK! ");
                #endif
            }
            #ifdef __USE_SERIAL_DBG__
            if (((i+1) % 8) == 0) {
                Serial.println();
            }
            #endif
        }
        #ifdef __USE_SERIAL_DBG__
        Serial.println();
        get_d_hist(dBuf);
        for (int i=0; i<256; i++) {
            Serial.print(i);
            Serial.print(": ");
            Serial.print(dBuf[i]);
            Serial.print(", ");
            if ((i+1) %8==0) {
                Serial.println();
            }
        }
        Serial.println();
        #endif
        return ok;
    }

    void rngStateLedUpdate() {
        if (rngStateLedPin >= 0) {
            switch (rngSampleMode) {
            case RSM_SELF_TEST:
                if (millis() - rngStateBlinkTimer > 100) {
                    rngStateBlinkTimer = millis();
                    rngStateLedCurrentState = !rngStateLedCurrentState;
                    digitalWrite(rngStateLedPin, rngStateLedCurrentState);
                }
                break;
            case RSM_FAILED:
                if (millis() - rngStateBlinkTimer > 1000) {
                    rngStateBlinkTimer = millis();
                    rngStateLedCurrentState = !rngStateLedCurrentState;
                    digitalWrite(rngStateLedPin, rngStateLedCurrentState);
                }
                break;
            
            case RSM_NONE:
                digitalWrite(rngStateLedPin, !rngStateLedActiveHigh);
                break;
            case RSM_OK:
                digitalWrite(rngStateLedPin, rngStateLedActiveHigh);
                break;
            }
        }
    }

    void rngSelfTest() {
        switch (rngSelfTestState) {
        case RST_INIT:
            testTimer = time(nullptr);
            for (int i = 0; i < 256; i++) {
                rngHistogram[i] = 0;
            }
            samples = 0;
            rngSelfTestState = RST_RUNNING;
            break;
        case RST_RUNNING:
            if (samples < selfTestSampleSize) {
                unsigned long byteCount = getRandomData(interruptIndex_input, rngBuf, rngBufSize);
                for (int i = 0; i < byteCount; i++) {
                    rngHistogram[rngBuf[i]]++;
                    samples++;
                }
                if (byteCount==0) {
                    if (time(nullptr) - testTimer > 10) {
                        if (isFirstFailure) {
                            Serial.print("RNG Self Test failed, no data received, check hardware connection to pin ");
                            Serial.print(pin_input);
                            Serial.print(", total interrupts on pin: ");
                            Serial.println(irq_count_total);
                            isFirstFailure = false;
                        }
                        rngSelfTestState = RST_FAILED;
                    }
                } else {
                    testTimer = time(nullptr);
                }
            } else {
                rngSelfTestState = RST_SAMPLE_DONE;
            }
            break;
        case RST_SAMPLE_DONE:
            if (evalRngSelfTest()) {
                rngSelfTestState = RST_OK;
            } else {
                rngSelfTestState = RST_FAILED;
            }
            break;
        }
    }

    unsigned int print_cnt=0;
    bool sampleRandomAndDistribute() {
        uint8_t byte;
        unsigned long byteCount = getRandomData(interruptIndex_input, rngBuf, rngBufSize);
        if (byteCount == 0) {
            return false;
        }
        if (publishBufPtr < publishMax) {
            for (int i = 0; i < byteCount; i++) {
                publishBuf[publishBufPtr] = rngBuf[i];
                publishBufPtr++;
                if (publishBufPtr >= publishMax) {
                    break;
                }
            }
        }
        if (publishViaSerial) {
            for (int i = 0; i < byteCount; i++) {
                byte = rngBuf[i];
                if (byte < 16) {
                    Serial.print("0");
                }
                Serial.print(byte, HEX);

                print_cnt++;
                if (print_cnt > 40) {
                    Serial.println();
                    print_cnt = 0;
                }
            }
        }
        return true;
    }

    void loop() {
        rngStateLedUpdate();
        switch (rngSampleMode) {
        case RSM_SELF_TEST:
            rngSelfTest();
            switch (rngSelfTestState) {
                case RST_OK:
                    #ifdef __USE_SERIAL_DBG__
                    Serial.println("RNG Self Test OK");
                    #endif
                    if (publishViaSerial) {
                        Serial.println();
                        Serial.println("===RNG-START===");
                    }
                    rngSampleMode = RSM_OK;
                    break;
                case RST_FAILED:
                    #ifdef __USE_SERIAL_DBG__
                    Serial.println("RNG Self Test failed");
                    #endif
                    rngSampleMode = RSM_FAILED;
                break;
            }
            break;
        case RSM_OK:
            lastOkMillis = millis();
            if (!sampleRandomAndDistribute()) {
                rngSampleMode = RSM_FAILED;
                if (publishViaSerial) {
                    Serial.println();
                    Serial.println("===RNG-STOP===");
                }
            }
            // TBD
            break;
        case RSM_FAILED:
            if (getIrqCount() - lastIrqCount > 4) {
                startSelfTest();
            }
            break;
        }
       lastIrqCount = getIrqCount();
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/rng/state/get") {
            publish();
        } else if (topic == name + "/rng/data/get") {
            publish_rng_data();
        } 
    };
};  // Rng

}  // namespace ustd
