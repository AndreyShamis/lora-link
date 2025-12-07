// packet_info_engine.hpp - Engine info packet
#pragma once
#include "packet_base.hpp"
#include "packet_types.hpp"
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════════════════
// ENGINE INFO PACKET
// ═══════════════════════════════════════════════════════════════════════════
#pragma pack(push, 1)

// Engine info packet: RPM and temperature
class PacketInfoEngine : public PacketBase
{
public:
    int16_t rpm; // 0–20000
    int8_t temp; // –40…85
    
    PacketInfoEngine() : rpm(0), temp(0) {
        packetType = CMD_INFO_ENGINE;
        payloadLen = sizeof(rpm) + sizeof(temp);
    }
};

#pragma pack(pop)
