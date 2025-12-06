// packet_request_info.hpp - Request info packet
#pragma once
#include "packet_base.hpp"
#include "packet_types.hpp"
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════════════════
// REQUEST INFO PACKET
// ═══════════════════════════════════════════════════════════════════════════
#pragma pack(push, 1)

// Request info packet
class PacketRequestInfo : public PacketBase
{
public:
    uint8_t requestType; // type of requested information
    
    PacketRequestInfo() : requestType(0) {
        packetType = CMD_REQUEST_INFO;
        payloadLen = sizeof(requestType);
    }
};

#pragma pack(pop)
