// packet_telemetry_fragment.hpp - Telemetry fragment packet
#pragma once
#include "packet_base.hpp"
#include "packet_types.hpp"

// ═══════════════════════════════════════════════════════════════════════════
// TELEMETRY FRAGMENT PACKET
// ═══════════════════════════════════════════════════════════════════════════
#pragma pack(push, 1)

// Telemetry fragment packet (JSON data)
class PacketTelemetryFragment : public PacketBase
{
public:
    PacketTelemetryFragment() {
        packetType = CMD_TELEMETRY_FRAGMENT;
        payloadLen = 0; // size will be set on send
    }
};

#pragma pack(pop)
