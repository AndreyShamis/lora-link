// packet_pong.hpp - PONG packet
#pragma once
#include "packet_base.hpp"
#include "packet_types.hpp"

// ═══════════════════════════════════════════════════════════════════════════
// PONG PACKET
// ═══════════════════════════════════════════════════════════════════════════
#pragma pack(push, 1)

// PONG packet
class PacketPong : public PacketBase
{
public:
    PacketPong() {
        packetType = CMD_PONG;
        payloadLen = 0; // PONG contains no data
    }
};

#pragma pack(pop)
