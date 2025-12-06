// packet_heartbeat.hpp - Heartbeat packet
#pragma once
#include "packet_base.hpp"
#include "packet_types.hpp"
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════════════════
// HEARTBEAT PACKET
// ═══════════════════════════════════════════════════════════════════════════
#pragma pack(push, 1)

// Heartbeat packet
class PacketHeartbeat : public PacketBase
{
public:
    uint32_t count; // arbitrary counter
    
    PacketHeartbeat() : count(0) {
        packetType = CMD_HEARTBEAT; // or another suitable type for heartbeat
        payloadLen = sizeof(count);
    }
};

#pragma pack(pop)
