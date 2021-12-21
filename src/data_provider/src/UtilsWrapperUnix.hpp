/*
 * Wazuh SYSINFO
 * Copyright (C) 2015-2021, Wazuh Inc.
 * December 17, 2021.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#ifndef _UTILS_WRAPPER_UNIX_H
#define _UTILS_WRAPPER_UNIX_H

#include <stdexcept>
#include <sys/socket.h>
#include <sys/ioctl.h>

class UtilsWrapperUnix
{
    public:
        static int createSocket(int domain, int type, int protocol);
        static int createIoctl(int d, unsigned long request, char *argp);
};

#endif // _UTILS_WRAPPER_UNIX_H