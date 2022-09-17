#pragma once

#include <math.h>

namespace ustd {

const double C_PI = 3.1415926535897932384626433;  //!< PI
const double C_D2R = C_PI / 180.0;                //!< degree -> radians conversion
const double C_R2D = 180.0 / C_PI;                //!< radians -> degrees
const double C_AU = 149597870700.0;               //!< astronomical unit, meter
const double C_C = 299792458.0;                   //!< speed of light, m/s
const double C_CAUD = C_C * 60 * 60 * 24 / C_AU;  //!< AUs per day, approx 173
const double C_MJD = 2400000.5;                   //!< MJD = JD - C_MJD

/*! \brief mupplet helper for some astronomical calculations: sunrise and sunset

Warning: WIP!
*/
class Astro {
  public:
    double lon, lat, utcOffset;

#ifdef USTD_FEATURE_FILESYSTEM
    Astro() {
        /*! This will at some point contain initialization from filesystem */
    }
#endif
    Astro(double lat, double lon, double utcOffset)
        : lat(lat), lon(lon), utcOffset(utcOffset) {
        /*! Instantiate an Astro object

        @param lat lattitude in degree
        @param lon longitude in degree
        @param utcOffset UTC time offset in seconds
        */
    }

    static long julianDayNumber(int year, uint8_t month, uint8_t day) {
        /*! Calculate the julian day number

        from Wikipedia: The Julian day number (JDN) is the integer assigned to a whole solar day
        in the Julian day count starting from noon Universal time, with Julian day number 0 assigned
        to the day starting at noon on Monday, January 1, 4713 BC, proleptic Julian calendar
        (November 24, 4714 BC, in the proleptic Gregorian calendar), a date at which three
        multi-year cycles started (which are: Indiction, Solar, and Lunar cycles) and which preceded
        any dates in recorded history.
        The 7,980-year Julian Period was formed by multiplying the 15-year indiction cycle,
        the 28-year solar cycle and the 19-year Metonic cycle.
        Indiction: (Y + 2) mod 15 + 1, with year AD.

        see also: \ref julianDate(), \ref modifiedJulianDate()

        @param year 4-digit year, e.g. 2021
        @param month [1-12]
        @param day [1-31]
        @return Integer julian day number (JDN)
        */
        long JDN = (1461L * ((long)year + 4800L + ((long)month - 14L) / 12L)) / 4L +
                   (367L * ((long)month - 2L - 12L * (((long)month - 14L) / 12L))) / 12L -
                   (3L * (((long)year + 4900L + ((long)month - 14L) / 12L) / 100L)) / 4L +
                   (long)day - 32075L;
        return JDN;
    }

    static double fracDay(uint8_t hour, uint8_t min, double sec) {
        /*! calculate fractional day [0.0 - 1.0[
            @param hour [0..23]
            @param min [0..59]
            @param sec [0 .. 59.999..]
            @return fractional day [0(midnight) .. 0.9999..(23:59:59.999)]
            */
        double dayFrac = ((double)hour + (double)min / 60.0 + (double)sec / 3600.0) / 24.0;
        return dayFrac;
    }

    static double julianDate(int year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min,
                             double sec) {
        /*! fractional julian date

        The Julian date (JD) of any instant is the Julian day number plus the fraction of a day
        since the preceding noon in Universal Time. Julian dates are expressed as a Julian day
        number (JDN) with a decimal fraction added. See: \ref julianDayNumber().

        Warning: this function exceeds the calculation precision of double on 8-bit MCUs, use \ref
        modifiedJulianDate() instead.

        @param year 4-digit year, e.g. 2021
        @param month [1-12]
        @param day [1-31]
        @param hour [0-23]
        @param min [0-59]
        @param sec [0.0-59.99999..]
        @return fractional julian day
        */
        long JDN = julianDayNumber(year, month, day);
        double dfrac = fracDay(hour, min, sec) - 0.5;
        double JD = (double)JDN + dfrac;
        return JD;
    }

    static double modifiedJulianDate(int year, uint8_t month, uint8_t day, uint8_t hour,
                                     uint8_t min, double sec) {
        /*! fractional modified julian date

        calculated as julianDay()-2400000.5, avoiding exhaustion of precision of 8-bit MCUs.

        from Wikipedia: The Modified Julian Date (MJD) was introduced by the Smithsonian
        Astrophysical Observatory in 1957 to record the orbit of Sputnik via an IBM 704 (36-bit
        machine) and using only 18 bits until August 7, 2576. MJD is the epoch of VAX/VMS and its
        successor OpenVMS, using 63-bit date/time, which allows times to be stored up to July 31,
        31086, 02:48:05.47. The MJD has a starting point of midnight on November 17, 1858 and is
        computed by MJD = JD - 2400000.5

        Warning: using \ref julianDate() on 8-bit MCUs results in inprecise results due to exceeded
        precision, whereas this function has higher precision due to it's modified range.

        see also: \ref julianDayNumber() (JDN)

        @param year 4-digit year, e.g. 2021
        @param month [1-12]
        @param day [1-31]
        @param hour [0-23]
        @param min [0-59]
        @param sec [0.0-59.99999..]
        */
        long JDN = julianDayNumber(year, month, day);
        double dfrac = fracDay(hour, min, sec) - 0.5;
        double JD = (double)(JDN - C_MJD) + dfrac;
        return JD;
    }

    static bool calculateSunRiseSet(int year, int month, int day, double lat, double lon,
                                    int localOffset, int daylightSavings, bool bRising,
                                    double *pSunTime) {
        /*!
        // Source: http://edwilliams.org/sunrise_sunset_algorithm.htm

        localOffset will be <0 for western hemisphere and >0 for eastern hemisphere
        daylightSavings should be 1 if it is in effect during the summer otherwise it should be 0

        Warning: WIP! Isn't integrated with class functions yet...
        */
        // 1. first calculate the day of the year
        const double ZENITH = 90.0 + 50.0 / 60.0;
        double N1 = floor(275.0 * month / 9.0);
        double N2 = floor((month + 9.0) / 12.0);
        double N3 = (1.0 + floor((year - 4 * floor(year / 4.0) + 2.0) / 3.0));
        double N = N1 - (N2 * N3) + day - 30.0;

        // 2. convert the longitude to hour value and calculate an approximate time
        double lonHour = lon / 15.0;
        double t;
        if (bRising)
            t = N + ((6 - lonHour) / 24);  // if rising time is desired:
        else
            t = N + ((18 - lonHour) / 24);  // if setting time is desired:

        // 3. calculate the Sun's mean anomaly
        double M = (0.9856 * t) - 3.289;

        // 4. calculate the Sun's true longitude
        double L =
            fmod(M + (1.916 * sin(C_D2R * M)) + (0.020 * sin(2 * C_D2R * M)) + 282.634, 360.0);

        // 5a. calculate the Sun's right ascension
        double RA = fmod(C_R2D * atan(0.91764 * tan(C_D2R * L)), 360.0);

        // 5b. right ascension value needs to be in the same quadrant as L
        double Lquadrant = floor(L / 90.0) * 90.0;
        double RAquadrant = floor(RA / 90.0) * 90.0;
        RA = RA + (Lquadrant - RAquadrant);

        // 5c. right ascension value needs to be converted into hours
        RA = RA / 15.0;

        // 6. calculate the Sun's declination
        double sinDec = 0.39782 * sin(C_D2R * L);
        double cosDec = cos(asin(sinDec));

        // 7a. calculate the Sun's local hour angle
        // double cosH = (sin(C_D2R * ZENITH) - (sinDec * sin(C_D2R * lat))) /
        //              (cosDec * cos(C_D2R * lat));
        double cosH =
            (cos(C_D2R * ZENITH) - (sinDec * sin(lat * C_D2R))) / (cosDec * cos(C_D2R * lat));

        if (bRising) {
            if (cosH > 1.0) {  // the sun never rises on this location (on the specified date)
                *pSunTime = -1.0;
                return false;
            }
        } else {
            if (cosH < -1.0) {  // the sun never sets on this location (on the specified date)
                *pSunTime = -1.0;
                return false;
            }
        }

        // 7b. finish calculating H and convert into hours
        double H;
        if (bRising)
            H = 360.0 - C_R2D * acos(cosH);  //   if if rising time is desired:
        else
            H = C_R2D * acos(cosH);  //   if setting time is desired:
        H = H / 15.0;

        // 8. calculate local mean time of rising/setting
        double T = H + RA - (0.06571 * t) - 6.622;

        // 9. adjust back to UTC
        double UT = fmod(T - lonHour, 24.0);

        // 10. convert UT value to local time zone of latitude/longitude
        *pSunTime = UT + localOffset + daylightSavings;
        return true;
    }
};

}  // namespace ustd