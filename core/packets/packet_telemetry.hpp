// packet_telemetry.hpp - Telemetry packet
#pragma once
#include "packet_base.hpp"
#include "packet_types.hpp"
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════════════════
// TELEMETRY PACKET
// ═══════════════════════════════════════════════════════════════════════════
#pragma pack(push, 1)

// Telemetry packet: speed, course, battery level
class PacketTelemetry : public PacketBase
{
public:
    uint8_t speed;     // 0–255
    uint8_t course;    // 0–179 (0–359°/2)
    uint8_t battLevel; // 0–100%
    
    PacketTelemetry() : speed(0), course(0), battLevel(0) {
        packetType = CMD_TELEMETRY_FRAGMENT;
        payloadLen = sizeof(speed) + sizeof(course) + sizeof(battLevel);
    }
};

#pragma pack(pop)
