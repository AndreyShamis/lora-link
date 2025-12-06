// packet_aggregated.hpp - Aggregated packet for multiple packets in one transmission
#pragma once
#include "packet_base.hpp"
#include "packet_types.hpp"
#include "lora_packet.hpp"
#include <stdint.h>
#include <string.h>

// ═══════════════════════════════════════════════════════════════════════════
// AGGREGATED PACKET - Multiple packets in one transmission
// ═══════════════════════════════════════════════════════════════════════════
// Format: [type1:1][len1:1][payload1:len1][type2:1][len2:1][payload2:len2]...

#pragma pack(push, 1)

class PacketAggregated : public PacketBase
{
public:
    static constexpr uint8_t MAX_SUB_PACKETS = 5;
    
    PacketAggregated() {
        packetType = CMD_AGR;
        payloadLen = 0;
    }
    
    // Add a sub-packet: returns true if added, false if no space
    bool addSubPacket(uint8_t pktType, const uint8_t* pktPayload, uint8_t pktPayloadLen) {
        // Check if we have space: type(1) + len(1) + payload
        uint8_t requiredSpace = 1 + 1 + pktPayloadLen;
        
        if (payloadLen + requiredSpace > MAX_LORA_PAYLOAD) {
            return false; // Not enough space
        }
        
        payloadLen += requiredSpace;
        return true;
    }
    
    // Serialize into buffer for transmission
    // Returns number of bytes written
    uint8_t serialize(uint8_t* buffer, uint8_t maxLen, 
                      const uint8_t* types, const uint8_t** payloads, 
                      const uint8_t* lens, uint8_t count) const {
        if (!buffer || maxLen == 0 || count == 0) return 0;
        
        uint8_t offset = 0;
        
        for (uint8_t i = 0; i < count; i++) {
            // Write packet type
            if (offset + 1 > maxLen) return 0;
            buffer[offset++] = types[i];
            
            // Write payload length
            if (offset + 1 > maxLen) return 0;
            buffer[offset++] = lens[i];
            
            // Write payload
            if (lens[i] > 0) {
                if (offset + lens[i] > maxLen) return 0;
                if (payloads[i]) {
                    memcpy(buffer + offset, payloads[i], lens[i]);
                }
                offset += lens[i];
            }
        }
        
        return offset;
    }
    
    // Deserialize from buffer
    // Calls callback for each sub-packet found
    // Callback signature: void callback(uint8_t type, const uint8_t* payload, uint8_t len)
    template<typename Callback>
    bool deserialize(const uint8_t* buffer, uint8_t bufferLen, Callback callback) const {
        if (!buffer || bufferLen == 0) return false;
        
        uint8_t offset = 0;
        
        while (offset < bufferLen) {
            // Read packet type
            if (offset + 1 > bufferLen) return false;
            uint8_t type = buffer[offset++];
            
            // Read payload length
            if (offset + 1 > bufferLen) return false;
            uint8_t len = buffer[offset++];
            
            // Validate payload length
            if (len > MAX_LORA_PAYLOAD) return false;
            
            // Read payload
            const uint8_t* payload = nullptr;
            if (len > 0) {
                if (offset + len > bufferLen) return false;
                payload = buffer + offset;
                offset += len;
            }
            
            // Call callback with sub-packet data
            callback(type, payload, len);
        }
        
        return true;
    }
    
    // Calculate how much space is left
    uint8_t getAvailableSpace() const {
        return MAX_LORA_PAYLOAD - payloadLen;
    }
    
    // Check if we can fit a packet of given size
    bool canFit(uint8_t pktPayloadLen) const {
        uint8_t required = 1 + 1 + pktPayloadLen; // type + len + payload
        return (payloadLen + required) <= MAX_LORA_PAYLOAD;
    }
};

#pragma pack(pop)
