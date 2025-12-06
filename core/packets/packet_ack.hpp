// packet_ack.hpp - ACK packet
#pragma once
#include "packet_base.hpp"
#include "packet_types.hpp"
#include "lora_config.h"
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════════════════
// ACK PACKET
// ═══════════════════════════════════════════════════════════════════════════
#pragma pack(push, 1)

// ACK packet
class PacketAck : public PacketBase
{
public:
    PacketId_t ackedId; // packetId being acknowledged (unified type)
    
    PacketAck() : ackedId(0) {
        packetType = CMD_ACK;
        payloadLen = sizeof(ackedId);
    }
};

#pragma pack(pop)
