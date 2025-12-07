// packet_bulk_ack.hpp - Bulk ACK packet
#pragma once
#include "packet_base.hpp"
#include "packet_types.hpp"
#include "lora_config.h"
#include <Arduino.h>
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════════════════
// BULK ACK PACKET
// ═══════════════════════════════════════════════════════════════════════════
#pragma pack(push, 1)

// Bulk ACK packet - up to 10 ACKs in one packet
class PacketBulkAck : public PacketBase
{
public:
    uint8_t count;                          // number of ACKs (1-10)
    PacketId_t ackedIds[10];                // array of packetIds to acknowledge (unified type)
    
    PacketBulkAck() : count(0) {
        packetType = CMD_BULK_ACK;
        payloadLen = sizeof(count);
        ackRequired = false;     // ACK не должен требовать ACK
        highPriority = true;     // ACK должен лететь немедленно
        service = true;          // служебный пакет
        for(int i = 0; i < 10; i++) ackedIds[i] = 0;
    }
    
    bool addAck(PacketId_t packetId) {
        if (count >= 10) return false;
        
        // Check if ID already exists (avoid duplicate ACKs)
        for (uint8_t i = 0; i < count; i++) {
            if (ackedIds[i] == packetId) {
                return true; // Already in list, no need to add
            }
        }
        
        // ID is unique, add it
        ackedIds[count] = packetId;
        count++;
        payloadLen = sizeof(count) + (count * sizeof(PacketId_t));
        return true;
    }
    
    void clear() {
        count = 0;
        payloadLen = sizeof(count);
        for(int i = 0; i < 10; i++) ackedIds[i] = 0;
    }
    
    bool isFull() const { return count >= 10; }
    bool isEmpty() const { return count == 0; }

    // Debug method to display BULK ACK contents
    String getDebugInfo() const {
        String result = "BulkACK(" + String(count) + "): ";
        for (uint8_t i = 0; i < count; i++) {
            if (i > 0) result += ",";
            result += String(ackedIds[i]);
        }
        if (count == 0) result += "empty";
        return result;
    }

    // Check for duplicate IDs (for debugging)
    bool hasDuplicates() const {
        for (uint8_t i = 0; i < count; i++) {
            for (uint8_t j = i + 1; j < count; j++) {
                if (ackedIds[i] == ackedIds[j]) {
                    return true;
                }
            }
        }
        return false;
    }
};

#pragma pack(pop)
