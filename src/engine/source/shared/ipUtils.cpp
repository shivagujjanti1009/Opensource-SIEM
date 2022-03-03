/* Copyright (C) 2015-2021, Wazuh Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#include "ipUtils.hpp"

namespace utils::ip {

uint32_t IPv4ToUInt(const std::string ipStr)
{
    int a, b, c, d {};
    char z {}; // Character after IP
    uint32_t ipUInt = 0;

    if (sscanf(ipStr.c_str(), "%d.%d.%d.%d%c", &a, &b, &c, &d, &z) != 4) {
        throw std::invalid_argument("Invalid IPv4 address");
    }
    else if (a < 0 || a > 255 || b < 0 || b > 255 || c < 0 || c > 255 || d < 0 || d > 255) {
        throw std::invalid_argument("Invalid IPv4 address");
    }

    ipUInt = a << 24;
    ipUInt |= b << 16;
    ipUInt |= c << 8;
    ipUInt |= d;
    return ipUInt;
}

uint32_t IPv4MaskUInt(const std::string maskStr) {

    uint32_t maskUInt = 0;

    if (maskStr.find('.') != std::string::npos) {
        // Thow an exception if the mask is not valid
        maskUInt = IPv4ToUInt(maskStr);
    }
    else {
        size_t afterMask = 0;
        // Thow an `invalid_argument` exception if the mask is not a number
        auto intMask = std::stoi(maskStr, &afterMask);
        if (intMask < 0 || intMask > 32) {
            throw std::invalid_argument("Invalid IPv4 mask");
        }
        else if (afterMask != maskStr.size()) {
            throw std::invalid_argument("Invalid IPv4 mask");
        }
        else {
            maskUInt = 0xFFFFFFFF << (32 - intMask);
        }
    }

    return maskUInt;

}

} // namespace utils::ip
