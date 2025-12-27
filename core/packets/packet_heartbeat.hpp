// packet_heartbeat.hpp - Heartbeat packet
#pragma once
#include "packet_base.hpp"
#include "packet_types.hpp"
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════════════════
// HEARTBEAT PACKET
// ═══════════════════════════════════════════════════════════════════════════
#pragma pack(push, 1)

// Heartbeat packet - BROADCAST MESSAGE
// Отправляется всем узлам в сети для объявления присутствия
class PacketHeartbeat : public PacketBase
{
public:
    uint32_t count; // arbitrary counter
    
    PacketHeartbeat() : count(0) {
        packetType      = CMD_HEARTBEAT;
        payloadLen      = sizeof(count);
        ackRequired     = false;        // Broadcast не требует ACK
        highPriority    = false;        // Обычный приоритет
        service         = true;         // Служебный пакет
        noRetry         = true;         // Не ретраить broadcast
        broadcast       = true;         // Это broadcast пакет!
    }
};

#pragma pack(pop)
