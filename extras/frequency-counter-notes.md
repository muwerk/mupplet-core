Frequency counter
=================

## Application notes

On an ESP32, measurement of frequencies at a GPIO are possible in ranges of 0-250kHz.
There are six measurement modes:

* `LOWFREQUENCY_FAST` (good for measurements <50Hz, not much filtering)
* `LOWFREQUENCY_MEDIUM` (good for sub-Hertz enabled measurements, e.g. Geiger-counters)
* `LOWFREQUENCY_LONGTERM` (good for <50Hz, little deviation, high precision)

The HIGHFREQUENCY modes automatically reset the filter, if the input signal drops to
0Hz.

* `HIGHFREQUENCY_FAST` (no filtering)
* `HIGHFREQUENCY_MEDIUM` (some filtering)
* `HIGHFREQUENCY_LONGTERM` (strong filtering, good for precision measurement of signals with little deviation)

Precision <80kHz is better than 0.0005%. Interrupt load > 80kHz impacts the performance of the ESP32 severely and error goes up to 2-5%.

## Messages

### Messages sent by the frequency counter mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/sensor/frequency` | `53.001` | Frequency in Hz encoded as String
| `<mupplet-name>/sensor/mode` | `[LOW,HIGH]FREQUENCY_[FAST,MEDIUM,LONGTERM]` | Sample mode, e.g. `LOWFREQUENCY_MEDIUM` as string

### Message received by the frequency counter mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/mode/set` | `[LOW,HIGH]FREQUENCY_[FAST,MEDIUM,LONGTERM]` | Set the sample mode, e.g. `LOWFREQUENCY_FAST`, string encoded
| `<mupplet-name>/state/get` |  | Causes current frequency to be sent with topic `<mupplet-name>/sensor/frequency`
| `<mupplet-name>/frequency/get` |  | Causes current frequency to be sent with topic `<mupplet-name>/sensor/frequency`
| `<mupplet-name>/mode/get` |  | Causes current sample mode to be sent with topic `<mupplet-name>/sensor/mode`

## Documentation

[FrequencyCounter documentation](https://muwerk.github.io/mupplet-core/docs/classustd_1_1FrequencyCounter.html)

## Geiger counter example

Using the frequency counter mupplet, a seven segment display mupplet and
a geiger counter.

<img src="https://github.com/muwerk/examples/blob/master/Resources/FrequencyCounter.jpg" width="50%">
Hardware: 2x 4.7kΩ resistor, 1nF capacitor, N-Channel mosfet (e.g. N7000),
geiger counter, 8 digit 7 segment display, Wemos D1 mini.

See [geigerCounter](https://github.com/muwerk/examples/tree/master/geigerCounter) for a geiger counter project with the frequency counter.
