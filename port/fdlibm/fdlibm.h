/*
 * port/fdlibm/fdlibm.h — vendored fdlibm transcendental surface.
 *
 * The locked surface (PLAN §2, issue #10): sin, cos, tan, atan, atan2,
 * pow. Doubles only. Every TU using these must be compiled with
 * -ffp-contract=off.
 *
 * Provenance + licenses: see fdlibm.c header and NOTICES.
 */
#ifndef PORT_FDLIBM_FDLIBM_H_
#define PORT_FDLIBM_FDLIBM_H_

double fd_sin(double x);
double fd_cos(double x);
double fd_tan(double x);
double fd_atan(double x);
double fd_atan2(double y, double x);
double fd_pow(double x, double y);

#endif /* PORT_FDLIBM_FDLIBM_H_ */
