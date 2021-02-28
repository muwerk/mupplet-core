// mupplet_core.h - muwerk mupplet core library

#pragma once

/*! \mainpage mupplet-core is a collection of hardware applets for the muwerk scheduler

\section Introduction

mupplet-core implements the following classes based on the cooperative scheduler muwerk:

* * \ref ustd::Light
* * \ref ustd::Switch
* * \ref ustd::DigitalOut
* * \ref ustd::FrequencyCounter

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

// clang-format off
/*! \brief mupplet-core string encoding utilities

Encoding of UTF-8 strings:

| Number of bytes | First code point | Last code point | Byte 1   | Byte 2   | Byte 3   | Byte 4   |
| --------------- | ---------------- | --------------- | -------- | -------- | -------- | -------- |
| 1               | U+0000           | U+007F          | 0xxxxxxx |          |          |
| 2               | U+0080           | U+07FF          | 110xxxxx | 10xxxxxx |          |
| 3               | U+0800           | U+FFFF          | 1110xxxx | 10xxxxxx | 10xxxxxx |
| 4               | U+10000          | U+10FFFF        | 11110xxx | 10xxxxxx | 10xxxxxx | 10xxxxxx |

Mapping between UTF-8 and latin1 ISO 8859-1

* <https://www.utf8-chartable.de/>

*/
// clang-format on

bool isAscii(String utf8string) {
    /*! Check, if an arbitrary UTF-8 string only consists of ASCII characters

    @return true, if ASCII compliant
    */
   for (unsigned int i=0; i<utf8string.length(); i++) {
       unsigned char c=utf8string[i];
       if ((c & 0x80) != 0) return false;
   }
   return true;
}

String utf8ToLatin(String utf8string, char invalid_char='_') {
    /*! Convert an arbitrary UTF-8 string into latin1 (ISO 8859-1)

    Note: This converts arbitrary multibyte utf-8 strings to latin1 on best-effort
    basis. Characters that are not in the target code-page are replaced by invalid_char.
    The conversion is aborted, if an invalid UTF8-encoding is encountered.
    @param utf8string utf8-encoded string
    @param invalid_char character that is used for to replace characters that are not in latin1
    @return latin1 (ISO 8859-1) encoded string
    */
    String latin="";
    unsigned char c,nc;
    for (unsigned int i=0; i<utf8string.length(); i++) {
        c=utf8string[i];
        if ((c & 0x80)==0) { // 1 byte utf8
            latin+=(char)c;
            continue;
        } else {
            if ((c & 0b11100000)==0b11000000) { // 2 byte utf8
                if (i<utf8string.length()-1) {
                    nc=utf8string[i+1];
                    switch (c) {
                        case 0xc2:
                            latin+=(char)nc;
                        break;
                        case 0xc3:
                            nc += 0x40;
                            latin+=(char)nc;
                        break;
                        default:
                            latin+=(char)invalid_char;
                        break;
                    }
                    i+=1;
                    continue;
                } else { // damaged utf8
                    latin+=(char)invalid_char;
                    return latin;
                }
            } else {
                if ((c & 0b11110000) == 0b11100000) { // 3 byte utf8
                    i+=2;
                    latin+=(char)invalid_char;
                    continue;
                } else {
                    if ((c & 0b11111000) == 0b11110000) { // 4 byte utf8
                        i+=3;
                        latin+=(char)invalid_char;
                        continue;
                    } else { // damaged utf8
                        latin+=(char)invalid_char;
                        return latin;  // we can't continue parsing
                    }
                }
            }
        }
    }
    return latin;
}

String latinToUtf8(String latin) {
    /*! Convert a latin1 (ISO 8859-1) string into UTF-8

    @param latin ISO 8869-1 (latin1) encoded string
    @return UTF8-encoded string  
    */

    String utf8str="";
    unsigned char c;
    for (unsigned int i=0; i<latin.length(); i++) {
        c=latin[i];
        if (c<0x80) utf8str+=(char)c;
        else {
            if (c<0xc0) {
                utf8str+=(char)0xc2;
                utf8str+=(char)c;
            } else {
                utf8str+=(char)0xc3;
                c-=0x40;
                utf8str+=(char)c;
            }
        }
    }
    return utf8str;
}

}  // namespace ustd
