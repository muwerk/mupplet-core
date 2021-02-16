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

bool parseBoolean(String arg) {
    /*! Parses a string argument for a boolean value
     *
     * The parser is not case sensistive. The words `on` and
     * `true` are interpreted as `true`, all other words as `false`.
     * If the argument contains a numeric value, 0 is considered
     * as `false`, all other values as `true`

     * @param arg The argument ot parse
     * @return The parsed value
     */
    arg.trim();
    arg.toLowerCase();
    if (arg == "on" || arg == "true") {
        return true;
    } else if (arg == "0") {
        return false;
    } else if (atoi(arg.c_str())) {
        return true;
    } else {
        return false;
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
     * @param tokenList An array of constant zero terminated char string pointers contining the
     *                  tokens. The tokens *must* be lowercase. The last token in the list *must*
     *                  be an empty string.
     * @return The index of the found token, or -1 if no token matches.
     */
    arg.trim();
    arg.toLowerCase();
    for (const char **pToken = tokenList; **pToken; *pToken++) {
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

double parseUnitLevel(String msg) {
    char buff[32];
    int l;
    int len = msg.length();
    double br = 0.0;
    memset(buff, 0, 32);
    if (len > 31)
        l = 31;
    else
        l = len;
    strncpy(buff, (const char *)msg.c_str(), l);

    if ((!strcmp((const char *)buff, "on")) || (!strcmp((const char *)buff, "true"))) {
        br = 1.0;
    } else {
        if ((!strcmp((const char *)buff, "off")) || (!strcmp((const char *)buff, "false"))) {
            br = 0.0;
        } else {
            if ((strlen(buff) > 4) && (!strncmp((const char *)buff, "pct ", 4))) {
                br = atoi((char *)(buff + 4)) / 100.0;
            } else {
                if (strlen(buff) > 1 && buff[strlen(buff) - 1] == '%') {
                    buff[strlen(buff) - 1] = 0;
                    br = atoi((char *)buff) / 100.0;
                } else {
                    if (strchr((char *)buff, '.')) {
                        br = atof((char *)buff);
                    } else {
                        br = atoi((char *)buff) / 100.0;
                    }
                }
            }
        }
    }
    if (br < 0.0)
        br = 0.0;
    if (br > 1.0)
        br = 1.0;
    return br;
}

}  // namespace ustd
