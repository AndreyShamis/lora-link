// packet_types.hpp - Packet type enumerations
#pragma once
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════════════════
// COMMAND IDS
// ═══════════════════════════════════════════════════════════════════════════
enum CommandID : uint8_t
{
    CMD_SET_MOTOR = 1,       // args: [motorIndex, power]
    CMD_SET_RUDDER = 2,      // args: [angle]
    CMD_START_TELEMETRY = 3, // args: []
    CMD_STOP_TELEMETRY = 4,
    CMD_REQUEST_STATUS = 5,
    CMD_REQUEST_ENGINE = 6,
    //CMD_HEARTBEAT = 7,
};

// ═══════════════════════════════════════════════════════════════════════════
// PACKET TYPES (used as packetType field)
// ═══════════════════════════════════════════════════════════════════════════
enum CommandType : uint8_t
{
    CMD_ACK = 'A',
    CMD_BULK_ACK = 'B',
    CMD_COMMAND_STRING = 'C',
    CMD_CONFIG = 'F',
    CMD_INFO_ENGINE = 'I',
    CMD_NAV = 'N',
    CMD_PING = 'P',
    CMD_HEARTBEAT = 'H',
    CMD_PONG = 'Q',
    CMD_RSSI_REPORT = 'R',
    CMD_STATUS = 'S',
    CMD_TELEMETRY_FRAGMENT = 'T',
    CMD_REQUEST_INFO = 'i',
    CMD_COMMAND_RESPONSE = 'r',
    CMD_REQUEST_ASA = 'a',
    CMD_REPOSNCE_ASA = 'b',
    CMD_AGR = 'G',  // Aggregated packet
};
