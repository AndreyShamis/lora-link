// packet_status.hpp - Status packet
#pragma once
#include "packet_base.hpp"
#include "packet_types.hpp"
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════════════════
// STATUS PACKET
// ═══════════════════════════════════════════════════════════════════════════
#pragma pack(push, 1)

// Status packet: bit flags
class PacketStatus : public PacketBase
{
public:
    uint8_t statusFlags; // bit0=OK,1=LowBatt,2=SensorErr…
    
    PacketStatus() : statusFlags(0) {
        packetType = CMD_STATUS;
        payloadLen = sizeof(statusFlags);
    }
};

#pragma pack(pop)
