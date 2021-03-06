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

int16_t parseToken(String arg, const char **tokenList, int16_t defaultVal = -1) {
    /*! Parses a string argument against a token list
     *
     * The parser is not case sensistive and returns
     * the index of the detected token. If no token matches
     * the parser returns -1
     *
     * @param arg           The argument to parse
     * @param tokenList     An array of constant zero terminated char string pointers containing the
     *                      tokens. The tokens *must* be lowercase. The last token in the list
     *                      *must* be null pointer.
     * @param defaultVal    The default value to return in case the entered argument is invalid
     *                      (default: -1)
     * @return The index of the found token, or -1 if no token matches.
     */
    arg.trim();
    arg.toLowerCase();
    for (const char **pToken = tokenList; *pToken; pToken++) {
        if (!strcmp(arg.c_str(), *pToken)) {
            return pToken - tokenList;
        }
    }
    return defaultVal;
}

long parseLong(String arg, long defaultVal) {
    /*! Parses a string argument for a valid integer value
     *
     * The parser checks if the parsed value is an interger number and returns eitherthe entered
     * value or the supplied default value.
     *
     * @param arg           The argument to parse
     * @param defaultVal    The default value to return in case the entered argument is invalid
     * @return The parsed value
     */
    arg.trim();
    if (arg.length() == 0) {
        return defaultVal;
    }
    for (const char *pPtr = arg.c_str(); *pPtr; pPtr++) {
        if (*pPtr < '0' || *pPtr > '9') {
            if (pPtr == arg.c_str() && *pPtr != '-') {
                return defaultVal;
            }
        }
    }
    return atol(arg.c_str());
}

long parseRangedLong(String arg, long minVal, long maxVal, long minDefaultVal, long maxDefaultVal) {
    /*! Parses a string argument for a valid integer value
     *
     * The parser checks if the parsed value stays between the defined boundaries and returns either
     * the entered value or one the defaults depending upon the entered value beeing lower than the
     * minimum value or higher than the maximum value.
     *
     * @param arg           The argument to parse
     * @param minVal        The minimum acceptable value
     * @param maxVal        The maximum acceptable value
     * @param minDefaultVal The default value to return in case the enetered value is lower than
     *                      `minVal` or not a number
     * @param maxDefaultVal The default value to return in case the enetered value is higher than
     *                      `maxVal`
     * @return The parsed value
     */
    long val = parseLong(arg, minDefaultVal);
    if (val == minDefaultVal) {
        // maybe not a number?
        return minDefaultVal;
    } else if (val < minVal) {
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

| Number of bytes | First code point | Last code point | Byte 1   | Byte 2   | Byte 3   | Byte 4
| --------------- | ---------------- | --------------- | -------- | -------- | -------- | --------
| 1               | U+0000           | U+007F          | 0xxxxxxx |          |          |
| 2               | U+0080           | U+07FF          | 110xxxxx | 10xxxxxx |          |
| 3               | U+0800           | U+FFFF          | 1110xxxx | 10xxxxxx | 10xxxxxx |
| 4               | U+10000          | U+10FFFF        | 11110xxx | 10xxxxxx | 10xxxxxx | 10xxxxxx

Mapping between UTF-8 and latin1 ISO 8859-1

* <https://www.utf8-chartable.de/>

*/
// clang-format on

bool isAscii(String utf8string) {
    /*! Check, if an arbitrary UTF-8 string only consists of ASCII characters
     *
     * @param utf8string    The string to check
     * @return `true`, if ASCII compliant
     */
    for (unsigned int i = 0; i < utf8string.length(); i++) {
        unsigned char c = utf8string[i];
        if ((c & 0x80) != 0)
            return false;
    }
    return true;
}

bool isNumber(String string, bool integer = false) {
    /*! Check, if an arbitrary string contains a numerical value or not.
     *
     * @param string    The string to check
     * @param integer   If `true`, the routine checks if the string contains an integer value
     *                  (default: `false`)
     * @return  `true` if the supplied string is a number or an integer value
     */
    return isNumber(string.c_str(), integer);
}

bool isNumber(const char *value, bool integer = false) {
    /*! Check, if an arbitrary string contains a numerical value or not.
     *
     * @param value     The string to check
     * @param integer   If `true`, the routine checks if the string contains an integer value
     *                  (default: `false`)
     * @return  `true` if the supplied string is a number or an integer value
     */
    if (!value) {
        return false;
    }
    if (*value == '-') {
        value++;
    }
    bool decimalpoint = false;
    while (*value) {
        if (*value < '0' || *value > '9') {
            if (integer || decimalpoint || *value != '.') {
                return false;
            } else {
                decimalpoint = true;
            }
        }
        value++;
    }
    return true;
}

String utf8ToLatin(String utf8string, char invalid_char = '_') {
    /*! Convert an arbitrary UTF-8 string into latin1 (ISO 8859-1)
     *
     * The function \ref isAscii() can be used to check if a conversion is necessary at
     * all. (No conversion is necessary, if the UTF-8 string only consists of ASCII chars,
     * in that case both encodings are identical.)
     *
     * See \ref latinToUtf8() for the opposite conversion.
     *
     * Note: This converts arbitrary multibyte utf-8 strings to latin1 on best-effort
     * basis. Characters that are not in the target code-page are replaced by invalid_char.
     * The conversion is aborted, if an invalid UTF8-encoding is encountered.
     *
     * @param utf8string    utf8-encoded string
     * @param invalid_char  character that is used for to replace characters that are not in latin1
     * @return latin1 (ISO 8859-1) encoded string
     */
    String latin = "";
    unsigned char c, nc;
    for (unsigned int i = 0; i < utf8string.length(); i++) {
        c = utf8string[i];
        if ((c & 0x80) == 0) {  // 1 byte utf8
            latin += (char)c;
            continue;
        } else {
            if ((c & 0b11100000) == 0b11000000) {  // 2 byte utf8
                if (i < utf8string.length() - 1) {
                    nc = utf8string[i + 1];
                    switch (c) {
                    case 0xc2:
                        latin += (char)nc;
                        break;
                    case 0xc3:
                        nc += 0x40;
                        latin += (char)nc;
                        break;
                    default:
                        latin += (char)invalid_char;
                        break;
                    }
                    i += 1;
                    continue;
                } else {  // damaged utf8
                    latin += (char)invalid_char;
                    return latin;
                }
            } else {
                if ((c & 0b11110000) == 0b11100000) {  // 3 byte utf8
                    i += 2;
                    latin += (char)invalid_char;
                    continue;
                } else {
                    if ((c & 0b11111000) == 0b11110000) {  // 4 byte utf8
                        i += 3;
                        latin += (char)invalid_char;
                        continue;
                    } else {  // damaged utf8
                        latin += (char)invalid_char;
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
     *
     * See \ref utf8ToLatin() for the opposite conversion.
     *
     * @param latin ISO 8869-1 (latin1) encoded string
     * @return UTF8-encoded string
     */
    String utf8str = "";
    unsigned char c;
    for (unsigned int i = 0; i < latin.length(); i++) {
        c = latin[i];
        if (c < 0x80)
            utf8str += (char)c;
        else {
            if (c < 0xc0) {
                utf8str += (char)0xc2;
                utf8str += (char)c;
            } else {
                utf8str += (char)0xc3;
                c -= 0x40;
                utf8str += (char)c;
            }
        }
    }
    return utf8str;
}

/*
const char *hdmap = "→←"
                    " 。「」、・ヲァィゥェォャュョッ"
                    "ーアイウエオカキクケコサシスセソ"
                    "タチツテトナニヌネノハヒフヘホマ"
                    "ミムメモヤユヨラリルレロワン゛゜"
                    "αäßεμσρg√⁻¹j˟₵₤ñö"
                    "pqΘ∞ΩüΣπx̅y不万円÷ █";
*/
class HD44780Charset {
  public:
    HD44780Charset(char *pLookupTable = nullptr) {
    }

    static String toHD_ASCII(String utf8string, char invalid_char = '_') {
        /*! Convert an arbitrary UTF-8 string into HD44780 with Japanese charset.
         *
         * Note: This converts arbitrary multibyte utf-8 strings to the HD44780 charset on
         * best-effort basis. Characters that are not in the target code-page are replaced by
         * invalid_char.
         * The conversion is aborted, if an invalid UTF8-encoding is encountered.
         *
         * This function only handles ASCII [32..125], it uses larger-matrix versions for lowercase
         * characters with descenders ('j','p','q','g','y')
         *
         * See \ref HD44780ASCIIToUtf8() for the opposite conversion.
         *
         * @param utf8string    utf8-encoded string
         * @param invalid_char  character that is used for to replace characters that are not in
         *                      HD44780 charset
         * @return HD44780 encoded string
         */
        String hdstr = "";
        unsigned char c;
        for (unsigned int i = 0; i < utf8string.length(); i++) {
            c = utf8string[i];
            Serial.println(c);
            if ((c & 0x80) == 0) {  // 1 byte utf8
                if (c < 0x7e) {     // and not 0x7e or 0x7f
                    if (strchr("gjpqy", c) != nullptr)
                        c += 0x80;  // Use extended matrix for lowercase descenders.
                    hdstr += (char)c;
                } else {
                    hdstr += (char)invalid_char;
                }
                continue;
            } else {
                if ((c & 0b11100000) == 0b11000000) {  // 2 byte utf8
                    if (i < utf8string.length() - 1) {
                        hdstr += (char)invalid_char;
                        i += 1;
                        continue;
                    } else {  // damaged utf8
                        hdstr += (char)invalid_char;
                        return hdstr;
                    }
                } else {
                    if ((c & 0b11110000) == 0b11100000) {  // 3 byte utf8
                        i += 2;
                        hdstr += (char)invalid_char;
                        continue;
                    } else {
                        if ((c & 0b11111000) == 0b11110000) {  // 4 byte utf8
                            i += 3;
                            hdstr += (char)invalid_char;
                            continue;
                        } else {  // damaged utf8
                            hdstr += (char)invalid_char;
                            return hdstr;  // we can't continue parsing
                        }
                    }
                }
            }
        }
        return hdstr;
    }

    static String toUtf8(String hdstr, char invalid_char = '_') {
        /*! Convert a HD44780 ASCII subset string into UTF-8
         *
         * See \ref utf8ToHD44780ASCII() for the opposite conversion.
         *
         * @param hdstr HD44780-ASCII encoded string
         * @return UTF8-encoded string
         */

        String utf8str = "";
        unsigned char c;
        for (unsigned int i = 0; i < hdstr.length(); i++) {
            c = hdstr[i];
            if (c < 0x7e)
                utf8str += (char)c;
            else {
                char cf = (char)(c - 0x80);
                if (strchr("gjpqy", cf) != nullptr)
                    utf8str += cf;
                else
                    utf8str += (char)invalid_char;
            }
        }
        return utf8str;
    }
};
}  // namespace ustd
