// packet_config.hpp - Config packet
#pragma once
#include "packet_base.hpp"
#include "packet_types.hpp"
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════════════════
// CONFIG PACKET
// ═══════════════════════════════════════════════════════════════════════════
#pragma pack(push, 1)

// Config packet (parameter change)
class PacketConfig : public PacketBase
{
public:
    uint8_t paramId;
    int8_t value;
    
    PacketConfig() : paramId(0), value(0) {
        packetType = CMD_CONFIG;
        payloadLen = sizeof(paramId) + sizeof(value);
    }
};

#pragma pack(pop)
