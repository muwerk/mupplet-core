// mupplet_core.h - muwerk mupplet core library

#pragma once

/*! \mainpage mupplet-core is a collection of hardware applets for the muwerk scheduler

\section Introduction

mupplet-core implements the following classes based on the cooperative scheduler muwerk:

* * \ref ustd::Led
* * \ref ustd::Switch
* * \ref ustd::DigitalOut

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
