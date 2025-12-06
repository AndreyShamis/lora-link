// lora_packet.hpp - Main LoRa packet structure
#pragma once
#include <Arduino.h>
#include <stdint.h>
#include "lora_config.h"

// Maximum payload size for LoRa packets
static constexpr size_t MAX_LORA_PAYLOAD = 85;

// Maximum constants
static constexpr size_t MAX_ARGS = 6;                                             // up to 6 arguments in command
static constexpr size_t CRC_SIZE = 2;                                             // 2 bytes CRC
static constexpr size_t HEADER_SIZE = sizeof(uint8_t) * 1 + sizeof(uint16_t) * 2; // = 5

#define LoraAddress uint8_t 

// ═══════════════════════════════════════════════════════════════════════════
// MAIN LORA PACKET STRUCTURE
// ═══════════════════════════════════════════════════════════════════════════
#pragma pack(push, 1)
struct LoRaPacket
{
    LoraAddress_t senderId;
    LoraAddress_t receiverId;
    uint8_t packetType;
    PacketId_t packetId;    // Unified packet ID type
    uint8_t payloadLen = 0;
    uint8_t payload[MAX_LORA_PAYLOAD];

    // Getter functions
    LoraAddress_t getSenderId() const { return senderId; }
    LoraAddress_t getReceiverId() const { return receiverId; }
    uint8_t getPacketType() const { return packetType; }

    // Setter functions
    void setSenderId(LoraAddress_t id) { senderId = id; }
    void setReceiverId(LoraAddress_t id) { receiverId = id; }
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
    
    if (pkt.payloadLen > MAX_LORA_PAYLOAD)
    {
        s += ", pl=❌CORRUPTED_LEN=" + String(pkt.payloadLen);
    }
    else if (pkt.payloadLen > 0)
    {
        s += ", pl=";
        for (int i = 0; i < pkt.payloadLen && i < MAX_LORA_PAYLOAD; i++)
        {
            char buf[4];
            snprintf(buf, sizeof(buf), "%02X ", pkt.payload[i]);
            s += buf;
        }
    }

    s += "]";
    return s;
}

// Pending send tracking structure
struct PendingSend
{
    LoRaPacket pkt = {}; // Initialize to zero
    uint32_t timestamp;
    uint8_t retries;
};
