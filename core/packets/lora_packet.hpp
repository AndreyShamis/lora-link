// lora_packet.hpp - Main LoRa packet structure
#pragma once
#include <Arduino.h>
#include <stdint.h>
#include "lora_config.h"



enum LoRaPacketFlags : uint8_t {
    LORA_PKT_FLAG_ACK_REQUIRED  = 0x01, // 1-й бит
    LORA_PKT_FLAG_HIGH_PRIORITY = 0x02, // 2-й бит
    LORA_PKT_FLAG_SERVICE       = 0x04, // 3-й бит (служебный/системный)
    LORA_PKT_FLAG_NO_RETRY      = 0x08, // 4-й бит (не ретраить, fire-and-forget)

    LORA_PKT_FLAG_ENCRYPTED     = 0x10, // 5-й бит (шифрованный payload)
    LORA_PKT_FLAG_COMPRESSED    = 0x20, // 6-й бит (payload сжат)
    LORA_PKT_FLAG_AGGREGATED    = 0x40, // 7-й бит (это AGR кадр/агрегированная посылка)
    LORA_PKT_FLAG_INTERNAL      = 0x80  // 8-й бит (внутренний/локальный/для логики)
};



#pragma pack(push, 1)
struct LoRaPacket
{
    LoraAddress_t senderId;
    LoraAddress_t receiverId;
    uint8_t packetType;
    PacketId_t packetId;    // Unified packet ID type
    uint8_t payloadLen = 0;
    uint8_t      flags = 0;
    uint8_t payload[MAX_LORA_PAYLOAD];

    // Getter functions
    LoraAddress_t getSenderId() const { return senderId; }
    LoraAddress_t getReceiverId() const { return receiverId; }
    uint8_t getPacketType() const { return packetType; }

    // Setter functions
    void setSenderId(LoraAddress_t id) { senderId = id; }
    void setReceiverId(LoraAddress_t id) { receiverId = id; }
    // ---- FLAG HELPERS ----
    bool isAckRequired()        const { return flags & LORA_PKT_FLAG_ACK_REQUIRED; }
    bool isHighPriority()       const { return flags & LORA_PKT_FLAG_HIGH_PRIORITY; }
    bool isService()            const { return flags & LORA_PKT_FLAG_SERVICE; }
    bool isNoRetry()            const { return flags & LORA_PKT_FLAG_NO_RETRY; }
    bool isEncrypted()          const { return flags & LORA_PKT_FLAG_ENCRYPTED; }
    bool isCompressed()         const { return flags & LORA_PKT_FLAG_COMPRESSED; }
    bool isAggregatedFrame()    const { return flags & LORA_PKT_FLAG_AGGREGATED; }
    bool isInternalLocalOnly()  const { return flags & LORA_PKT_FLAG_INTERNAL; }

    void setAckRequired(bool v)       { v ? flags |=  LORA_PKT_FLAG_ACK_REQUIRED  : flags &= ~LORA_PKT_FLAG_ACK_REQUIRED; }
    void setHighPriority(bool v)      { v ? flags |=  LORA_PKT_FLAG_HIGH_PRIORITY : flags &= ~LORA_PKT_FLAG_HIGH_PRIORITY; }
    void setService(bool v)           { v ? flags |=  LORA_PKT_FLAG_SERVICE       : flags &= ~LORA_PKT_FLAG_SERVICE; }
    void setNoRetry(bool v)           { v ? flags |=  LORA_PKT_FLAG_NO_RETRY      : flags &= ~LORA_PKT_FLAG_NO_RETRY; }
    void setEncrypted(bool v)         { v ? flags |=  LORA_PKT_FLAG_ENCRYPTED     : flags &= ~LORA_PKT_FLAG_ENCRYPTED; }
    void setCompressed(bool v)        { v ? flags |=  LORA_PKT_FLAG_COMPRESSED    : flags &= ~LORA_PKT_FLAG_COMPRESSED; }
    void setAggregatedFrame(bool v)   { v ? flags |=  LORA_PKT_FLAG_AGGREGATED    : flags &= ~LORA_PKT_FLAG_AGGREGATED; }
    void setInternalLocalOnly(bool v) { v ? flags |=  LORA_PKT_FLAG_INTERNAL      : flags &= ~LORA_PKT_FLAG_INTERNAL; }

};
static_assert(sizeof(LoRaPacket) <= 150, "LoRaPacket too large!");
#pragma pack(pop)


// Helper function to convert LoRaPacket to string
inline String LoRaPacketToStr(const LoRaPacket &pkt)
{
    String s;
    s += "[" + String(pkt.senderId) + "->" + String(pkt.receiverId) + "], T=[" + String((char)pkt.packetType);
    s += "/" + String((int)pkt.packetType) + "], id=" + String(pkt.packetId);
    s += ", plLen=" + String(pkt.payloadLen);
    
    if (pkt.payloadLen > MAX_LORA_PAYLOAD) {
        s += ", pl=❌CORRUPTED_LEN=" + String(pkt.payloadLen);
    } else if (pkt.payloadLen > 0) {
        s += ", pl=";
        for (int i = 0; i < pkt.payloadLen && i < MAX_LORA_PAYLOAD; i++) {
            char buf[4];
            snprintf(buf, sizeof(buf), "%02X ", pkt.payload[i]);
            s += buf;
        }
    }

    s += "]";
    return s;
}


// Pending send tracking structure
struct PendingSend {
    LoRaPacket pkt = {}; // Initialize to zero
    uint32_t timestamp;
    uint8_t retries;
};