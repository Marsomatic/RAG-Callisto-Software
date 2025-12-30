#include "SpiceUsr.h"
#include <stdio.h>
#include <time.h>

int main(void) {
    SpiceDouble values[10];
    SpiceInt found;
    SpiceInt n;

    furnsh_c("/home/matej/cspice/kernels/naif0012.tls");    // leapseconds
    furnsh_c("/home/matej/cspice/kernels/de435.bsp");      // planetary ephemeris
    furnsh_c("/home/matej/cspice/kernels/pck00011.tpc");    // Earth orientation & shape
    furnsh_c("/home/matej/cspice/kernels/earth_000101_251219_250923.bpc");    /* whatever you have loaded already */

    /* check for pole RA coeffs */
    gdpool_c("BODY399_POLE_RA", 0, 10, &n, values, &found);
    if (!found) {
        printf("BODY399_POLE_RA not in kernel pool.\n");
    } else {
        printf("BODY399_POLE_RA found, %d values.\n", n);
    }

    /* check for other required vars similarly */
    gdpool_c("BODY399_POLE_DEC", 0, 10, &n, values, &found);
    printf("BODY399_POLE_DEC %s\n", found ? "found" : "MISSING");

    gdpool_c("BODY399_PM", 0, 10, &n, values, &found);
    printf("BODY399_PM %s\n", found ? "found" : "MISSING");

    time_t rawtime = time(NULL);
    char utc_str[80];
    SpiceDouble ephemeris_t;

    struct tm *utc = gmtime(&rawtime);
    strftime(utc_str, sizeof(utc_str), "%Y-%m-%dT%H:%M:%S", utc);
    str2et_c(utc_str, &ephemeris_t);

    return ephemeris_t;
    SpiceDouble xform[3][3];

    pxform_c("ITRF93","J2000",ephemeris_t, xform);

    kclear_c();
    return 0;
}