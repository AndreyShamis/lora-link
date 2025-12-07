// packet_command_response.hpp - Command response packet
#pragma once
#include "packet_base.hpp"
#include "packet_types.hpp"

// ═══════════════════════════════════════════════════════════════════════════
// COMMAND RESPONSE PACKET
// ═══════════════════════════════════════════════════════════════════════════
#pragma pack(push, 1)

// Command response packet
class PacketCommandResponse : public PacketBase
{
public:
    PacketCommandResponse() {
        packetType = CMD_COMMAND_RESPONSE;
        payloadLen = 0; // size will be set on send
    }
};

#pragma pack(pop)
