/*
Author: Matej Markovic
compile with: gcc -o cspice_test.exe cspice_test.c -I/path/to/cspice/include -L/path/to/cspice/lib -lm -lcspices
*/

#include <stdio.h>
#include <time.h>
#include <math.h>
#include "SpiceUsr.h"

// encoder ticks per revolution of output shaft(encoder ticks * gearbox ratio)
#define TICKS_PER_REV 60000

// GPIO pins
#define STEP_PIN  13
#define DIR_PIN   5
#define EN_PIN    6
#define ENC_A     17
#define ENC_B     27


SpiceDouble ephemeris_t; // ephemeris time past J2000
SpiceDouble obs_lat = 0.790213649;
SpiceDouble obs_lon = 0.239491811;
SpiceDouble obs_alt = 0.2235;

SpiceDouble getEphemerisTime(){
    time_t rawtime = time(NULL);
    char utc_str[80];

    struct tm *utc = gmtime(&rawtime);
    strftime(utc_str, sizeof(utc_str), "%Y-%m-%dT%H:%M:%S", utc);
    str2et_c(utc_str, &ephemeris_t);

    return ephemeris_t;
}



SpiceDouble getHa(){
    SpiceDouble ephemeris_t = getEphemerisTime();

    // returns earth radii at different locations to account for ellipsoid shape. Loaded from kernel pck00011.tpc
    SpiceDouble radii[3];
    SpiceInt n;
    bodvrd_c("EARTH", "RADII", 3, &n, radii);
    SpiceDouble equatorial = radii[0];
    SpiceDouble polar      = radii[2];
    SpiceDouble flattening = (equatorial - polar) / equatorial;

    // Observer vector in ITRF93 standard
    SpiceDouble obs_itrf[3];
    georec_c(obs_lon, obs_lat, obs_alt, equatorial, flattening, obs_itrf);

    // Sun position wrt Earth in J2000
    SpiceDouble sun_j2000[3];
    SpiceDouble lt;
    spkpos_c("SUN", ephemeris_t, "J2000", "LT+S", "EARTH", sun_j2000, &lt);

    // Observer vector in J2000
    SpiceDouble xform[3][3]; // transformation matrix
    SpiceDouble obs_j2000[3];
    pxform_c("ITRF93", "J2000", ephemeris_t, xform);
    mxv_c(xform, obs_itrf, obs_j2000);

    // Topocentric Sun vector
    SpiceDouble sun_obs_j2000[3];
    vsub_c(sun_j2000, obs_j2000, sun_obs_j2000); // vector subtraction

    // RA
    SpiceDouble ra  = atan2(sun_obs_j2000[1], sun_obs_j2000[0]);
    if (ra < 0) ra += twopi_c();

    // Greenwich sidereal RA (RA of ITRF x-axis in J2000)
    SpiceDouble x_itrf[3] = {1.0, 0.0, 0.0};
    SpiceDouble x_itrf_j2000[3];
    mxv_c(xform, x_itrf, x_itrf_j2000); // matrix times vector
    SpiceDouble ra_greenwich = atan2(x_itrf_j2000[1], x_itrf_j2000[0]);
    if (ra_greenwich < 0) ra_greenwich += twopi_c();

    // Local Sidereal Time
    SpiceDouble lst = ra_greenwich + obs_lon;
    lst = fmod(lst, twopi_c());
    if (lst < 0) lst += twopi_c();

    // Hour Angle
    SpiceDouble ha = lst - ra;
    while (ha <= -pi_c()) ha += twopi_c();
    while (ha >  pi_c()) ha -= twopi_c();

    return ha;
}

int main(int argc, char **argv){
    SpiceDouble ha;
    SpiceInt setpoint;
    furnsh_c("/home/matej/cspice/kernels/naif0012.tls");    // leapseconds
    furnsh_c("/home/matej/cspice/kernels/de435.bsp");      // planetary ephemeris
    furnsh_c("/home/matej/cspice/kernels/pck00011.tpc");    // Earth orientation & shape
    furnsh_c("/home/matej/cspice/kernels/earth_000101_260327_251229.bpc"); // earth binary pck

    ha = getHa();
    setpoint = ha/twopi_c() * TICKS_PER_REV + TICKS_PER_REV/4;

    printf("HA: %f;    Setpoint: %d\n", ha, setpoint);

    kclear_c();

    return 0;
}
