1.9.1


Changes:

1)

In `civetweb.c` commented:
#ifdef __clang__
/* Avoid warnings for Xopen 7.00 and higher */
#pragma clang diagnostic ignored "-Wno-reserved-id-macro"
#pragma clang diagnostic ignored "-Wno-keyword-macro"
#endif
