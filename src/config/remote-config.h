/* Copyright (C) 2015-2020, Wazuh Inc.
 * Copyright (C) 2009 Trend Micro Inc.
 * All right reserved.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation
 */

#ifndef CLOGREMOTE_H
#define CLOGREMOTE_H

#define SYSLOG_CONN 1
#define SECURE_CONN 2

#define REMOTED_PROTO_TCP     (0x1 << 0)
#define REMOTED_PROTO_UDP     (0x1 << 1)
#define REMOTED_PROTO_DEFAULT REMOTED_PROTO_TCP

#define REMOTED_PROTO_TCP_STR "TCP"
#define REMOTED_PROTO_UDP_STR "UDP"
#define REMOTED_PROTO_DEFAULT_STR  (REMOTED_PROTO_DEFAULT == REMOTED_PROTO_TCP \
                                   ? REMOTED_PROTO_TCP_STR : REMOTED_PROTO_UDP_STR)

#include "shared.h"
#include "global-config.h"

/* socklen_t header */
typedef struct _remoted {
    int *proto;
    int *port;
    int *conn;
    int *ipv6;

    char **lip;
    os_ip **allowips;
    os_ip **denyips;

    int m_queue;
    int sock;
    int position;
    int nocmerged;
    socklen_t peer_size;
    long queue_size;
    bool worker_node;
    int rids_closing_time;
    _Config global;
} remoted;

#endif /* CLOGREMOTE_H */
