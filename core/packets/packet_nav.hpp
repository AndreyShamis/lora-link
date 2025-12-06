// packet_nav.hpp - Navigation packet
#pragma once
#include "packet_base.hpp"
#include "packet_types.hpp"
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════════════════
// NAVIGATION PACKET
// ═══════════════════════════════════════════════════════════════════════════
#pragma pack(push, 1)

// Navigation packet: GPS
class PacketNav : public PacketBase
{
public:
    int32_t lat;   // 1e-7°
    int32_t lon;   // 1e-7°
    uint16_t hdop; // HDOP×100
    
    PacketNav() : lat(0), lon(0), hdop(0) {
        packetType = CMD_NAV;
        payloadLen = sizeof(lat) + sizeof(lon) + sizeof(hdop);
    }
};

#pragma pack(pop)
