// packet_rssi_report.hpp - RSSI Report packet
#pragma once
#include "packet_base.hpp"
#include "packet_types.hpp"
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════════════════
// RSSI REPORT PACKET
// ═══════════════════════════════════════════════════════════════════════════
#pragma pack(push, 1)

// RSSI Report packet
struct PacketRssiReport : PacketBase
{
public:
    float rawRssi = 0;
    float smoothedRssi = 0;
    
    PacketRssiReport() : rawRssi(0.0f), smoothedRssi(0.0f) {
        packetType = CMD_RSSI_REPORT;
        payloadLen = sizeof(rawRssi) + sizeof(smoothedRssi);
    }
};

#pragma pack(pop)
