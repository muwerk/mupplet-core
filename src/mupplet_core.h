// mupplet_core.h - muwerk mupplet core library

#pragma once

/*! \mainpage mupplet-core is a collection of hardware applets for the muwerk scheduler

\section Introduction

mupplet-core implements the following classes based on the cooperative scheduler muwerk:

* * \ref ustd::Light
* * \ref ustd::Switch
* * \ref ustd::DigitalOut

Additionally there are implementation for the following helper classes:

* * \ref ustd::LightController

For an overview, see:
<a href="https://github.com/muwerk/mupplet-core/blob/master/README.md">mupplet-core readme</a>

Libraries are header-only and should work with any c++11 compiler and
and support platforms esp8266 and esp32.

This library requires the libraries ustd, muwerk and requires a
<a href="https://github.com/muwerk/ustd/blob/master/README.md">platform
define</a>.

\section Reference
* * <a href="https://github.com/muwerk/mupplet-core">mupplet-core github repository</a>

depends on:
* * <a href="https://github.com/muwerk/ustd">ustd github repository</a>
* * <a href="https://github.com/muwerk/muwerk">muwerk github repository</a>

*/

#include "scheduler.h"

//! \brief The muwerk namespace
namespace ustd {

int8_t parseBoolean(String arg) {
    /*! Parses a string argument for a boolean value
     *
     * The parser is not case sensistive. The words `on` and
     * `true` are interpreted as `true`, `off` and `false`as `false`.
     * If the argument contains a numeric value, 0 is considered
     * as `false`, all other values as `true`
     * If the argument contains anything different, -1 is returned.

     * @param arg The argument to parse
     * @return `0` if `false`, `1` if `true`, `-1` if invalid
     */
    arg.trim();
    arg.toLowerCase();
    if (arg == "on" || arg == "true") {
        return 1;
    } else if (arg == "0" || arg == "off" || arg == "false") {
        return 0;
    } else if (atoi(arg.c_str())) {
        return 1;
    } else {
        return -1;
    }
}

int16_t parseToken(String arg, const char **tokenList) {
    /*! Parses a string argument against a token list
     *
     * The parser is not case sensistive and returns
     * the index of the detected token. If no token matches
     * the parser returns -1
     *
     * @param arg The argument to parse
     * @param tokenList An array of constant zero terminated char string pointers containing the
     *                  tokens. The tokens *must* be lowercase. The last token in the list *must*
     *                  be null pointer.
     * @return The index of the found token, or -1 if no token matches.
     */
    arg.trim();
    arg.toLowerCase();
    for (const char **pToken = tokenList; *pToken; pToken++) {
        if (!strcmp(arg.c_str(), *pToken)) {
            return pToken - tokenList;
        }
    }
    return -1;
}

long parseRangedLong(String arg, long minVal, long maxVal, long minDefaultVal, long maxDefaultVal) {
    /*! Parses a string argument for a valid integer value
     *
     * The parser checks if the parsed value stays between the defined boundaries and returns either
     * the entered value or one the defaults depending upon the entered value beeing lower than the
     * minimum value or higher than the maximum value.
     *
     * @param arg The argument to parse
     * @param minVal The minimum acceptable value
     * @param maxVal The maximum acceptable value
     * @param minDefaultVal The default value to return in case the enetered value is lower than
     *                      `minVal`
     * @param maxDefaultVal The default value to return in case the enetered value is higher than
     *                      `maxVal`
     * @return The parsed value
     */
    arg.trim();
    long val = atol(arg.c_str());
    if (val < minVal) {
        return minDefaultVal;
    } else if (val > maxVal) {
        return maxDefaultVal;
    } else {
        return val;
    }
}

double parseUnitLevel(String arg) {
    /*! Parses a string argument for a valid unit level
     *
     * A unit level (like a light) can be set fully on or off with `on` or `true` and `off` or
     * `false`. A fractional brightness of `0.34` (within interval [0.0, 1.0]) can be sent as either
     * as `pct 34`, or `0.34`, or `34%`.
     *
     * @param arg The argument to parse
     * @return The parsed value (double between 0.00 and 1.00)
     */
    double val = 0.0;
    arg.trim();
    arg.toLowerCase();

    if (arg == "on" || arg == "true") {
        return 1.0;
    } else if (arg == "off" || arg == "false") {
        return 0.0;
    } else if (arg == "pct") {
        arg = shift(arg);
        val = atoi(arg.c_str()) / 100.0;
    } else if (arg.endsWith("%")) {
        arg.remove(arg.length() - 1);
        val = atoi(arg.c_str()) / 100.0;
    } else if (arg.indexOf('.') == -1) {
        val = atoi(arg.c_str()) / 100.0;
    } else {
        val = atof(arg.c_str());
    }
    return val < 0.0 ? 0.0 : val > 1.0 ? 1.0 : val;
}

}  // namespace ustd
