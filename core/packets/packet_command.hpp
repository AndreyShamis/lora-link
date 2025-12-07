// packet_command.hpp - Command packet
#pragma once
#include "packet_base.hpp"
#include "packet_types.hpp"
#include "lora_packet.hpp"
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════════════════
// COMMAND PACKET
// ═══════════════════════════════════════════════════════════════════════════
#pragma pack(push, 1)
static constexpr size_t MAX_ARGS = 6;                                             // up to 6 arguments in command

// Command packet: cmdId + variable number of arguments
class PacketCommand : public PacketBase
{
public:
    uint8_t cmdId;         // from CommandID
    uint8_t argCount;      // actually used args
    int8_t args[MAX_ARGS]; // each –128…127


    PacketCommand() : cmdId(0), argCount(0) {
        packetType = CMD_COMMAND_STRING;
        ackRequired = true;     // ACK не должен требовать ACK
        highPriority = false;     // ACK должен лететь немедленно
        service = false;          // служебный пакет
        payloadLen = sizeof(cmdId) + sizeof(argCount) + sizeof(args);
        for(int i = 0; i < MAX_ARGS; i++) args[i] = 0;
    }
};

#pragma pack(pop)
