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
    CMD_ACK                 = 33,       // ACK  'A'
    CMD_BULK_ACK            = 34,       // Bulk ACK  'B'
    CMD_HEARTBEAT           = 39,       // 'H'
    CMD_REQUEST_ASA         = 40,       // 'a'
    CMD_RESPONCE_ASA        = 41,       // 'b'
    CMD_COMMAND_STRING      = 42,       // 'C' Command packet
    CMD_AGR                 = 43,       // Aggregated packet

    CMD_PONG                = 60,       // 'O'
    CMD_PING                = 62,       // 'P'

    CMD_CONFIG              = 'F',      // Config packet
    CMD_INFO_ENGINE         = 'I',      // Engine info packet
    CMD_NAV                 = 'N',      // Navigation packet
    CMD_RSSI_REPORT         = 'R',      // RSSI report packet
    CMD_STATUS              = 'S',      // Status packet
    CMD_TELEMETRY_FRAGMENT  = 'T',      // Telemetry fragment packet
    CMD_REQUEST_INFO        = 'i',      // Request info packet
    CMD_COMMAND_RESPONSE    = 'r',      // Command response packet

};
