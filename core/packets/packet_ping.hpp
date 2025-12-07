// packet_ping.hpp - PING packet
#pragma once
#include "packet_base.hpp"
#include "packet_types.hpp"

// ═══════════════════════════════════════════════════════════════════════════
// PING PACKET
// ═══════════════════════════════════════════════════════════════════════════
#pragma pack(push, 1)

// PING packet
class PacketPing : public PacketBase
{
public:
    PacketPing() {
        packetType = CMD_PING;
        payloadLen = 0; // PING contains no data
        ackRequired = false;     // ACK не должен требовать ACK
        highPriority = false;     // ACK должен лететь немедленно
        service = false;          // служебный пакет
    }
};

#pragma pack(pop)
