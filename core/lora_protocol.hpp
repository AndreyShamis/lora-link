// lora_protocol.hpp - LoRa Protocol Core Functions
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "lora_packets.hpp"

// ═══════════════════════════════════════════════════════════════════════════
// CRC16-CCITT (polynomial 0x1021)
// ═══════════════════════════════════════════════════════════════════════════
static uint16_t calcCRC16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; ++b)
        {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}


