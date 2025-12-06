// packet_base.hpp - Base packet structures and definitions
#pragma once
#include <Arduino.h>
#include <stdint.h>
#include "lora_config.h"

// ═══════════════════════════════════════════════════════════════════════════
// PACKET BASE CLASS
// ═══════════════════════════════════════════════════════════════════════════
class PacketBase
{
public:
    uint8_t packetType;     // 'C','T','I','S','A','G','H'=Heartbeat
    PacketId_t packetId;    // sequential number 0…255 (unified type)
    uint8_t payloadLen = 0; // body length (without header and CRC)
};

// Free function for converting PacketBase to string (safer than member function)
inline String PacketBaseToString(const PacketBase& base) {
    return "PacketBase[type=" + String((char)base.packetType) +
           ", id=" + String(base.packetId) +
           ", payloadLen=" + String(base.payloadLen) + "]";
}
