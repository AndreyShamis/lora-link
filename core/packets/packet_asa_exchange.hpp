// packet_asa_exchange.hpp - ASA Exchange packet
#pragma once
#include "packet_base.hpp"
#include "packet_types.hpp"
#include <stdint.h>
#include <stddef.h>

// Helper function to parse ASA request
inline bool parseAsaRequest(const uint8_t *buf, size_t len, uint8_t &profileIndex)
{
    if (len != sizeof(uint8_t))
        return false;
    profileIndex = *buf;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// ASA EXCHANGE PACKET
// ═══════════════════════════════════════════════════════════════════════════
#pragma pack(push, 1)

// ASA Exchange packet (for ASA requests and confirmations)
struct PacketAsaExchange : public PacketBase {
    uint8_t profileIndex;

    PacketAsaExchange(CommandType type = CMD_REQUEST_ASA) {
        packetType = type;
        packetId = 0;
        payloadLen = sizeof(profileIndex);
        profileIndex = 0;
    }

    void setProfile(uint8_t index) {
        profileIndex = index;
        payloadLen = sizeof(profileIndex);
    }

    uint8_t getProfile() const {
        return profileIndex;
    }
};

#pragma pack(pop)
