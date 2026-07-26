#pragma once

#if defined(__linux__)
    #include "port_linux.h"
#elif defined(__FreeBSD__)
    #include "port_freebsd.h"
#elif defined(__OpenBSD__)
    #include "port_openbsd.h"
#else
    #error "Unsupported platform"
#endif