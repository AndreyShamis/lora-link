// lora_config.h - LoRa Link Configuration
#pragma once
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════════════════
// PACKET ID TYPE
// ═══════════════════════════════════════════════════════════════════════════
typedef uint8_t PacketId_t;
typedef uint8_t LoraAddress_t;

// ═══════════════════════════════════════════════════════════════════════════
// PROFILE CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════
#define LORA_PROFILE_COUNT 13  // 9 LoRa + 4 GFSK profiles

// Radio modes
enum class RadioProfileMode : uint8_t {
    LORA = 0,
    FSK = 1
};

// Universal profile structure: LoRa + GFSK
static constexpr struct {
    RadioProfileMode mode;     // LORA or FSK (GFSK for SX1262)
    float bandwidth;           // kHz (for LoRa) or rxBandwidth (for GFSK)
    int spreadingFactor;       // 7–12 (LoRa only, 0 for GFSK)
    int codingRate;            // 5–8 (LoRa only, 0 for GFSK)
    uint32_t bitrate;          // bit/s (GFSK only, 0 for LoRa)
    uint32_t deviation;        // Hz (GFSK only, 0 for LoRa)
} loraProfiles[LORA_PROFILE_COUNT] = {
    // LoRa profiles (0-8): от максимально надёжного до быстрого
    {RadioProfileMode::LORA, 125.0, 12, 7, 0, 0},      // 0: Maximum reliability (very slow, max range)
    {RadioProfileMode::LORA, 125.0, 11, 7, 0, 0},      // 1: Very good stability
    {RadioProfileMode::LORA, 125.0, 10, 7, 0, 0},      // 2: Reliable compromise
    {RadioProfileMode::LORA, 250.0,  9, 6, 0, 0},      // 3: Medium mode (urban)
    {RadioProfileMode::LORA, 250.0,  8, 6, 0, 0},      // 4: Medium, open terrain
    {RadioProfileMode::LORA, 250.0,  7, 5, 0, 0},      // 5: Fast with good signal
    {RadioProfileMode::LORA, 500.0,  9, 5, 0, 0},      // 6: Speed + range
    {RadioProfileMode::LORA, 500.0,  8, 5, 0, 0},      // 7: Very fast, LOS preferred
    {RadioProfileMode::LORA, 500.0,  7, 5, 0, 0},      // 8: Maximum LoRa speed
    // GFSK profiles (9-12): SX1262/RadioLib supports GFSK
    {RadioProfileMode::FSK, 117.3, 0, 0, 19200, 10000}, // 9: GFSK standard
    {RadioProfileMode::FSK, 156.2, 0, 0, 38400, 20000}, // 10: GFSK medium  
    {RadioProfileMode::FSK, 187.2, 0, 0, 50000, 25000}, // 11: GFSK fast 
    {RadioProfileMode::FSK, 234.3, 0, 0, 100000, 50000} // 12: GFSK maximum speed
};

// Extended RSSI mapping table: RSSI >= X → profile Y (including FSK)
// AGGRESSIVE ADAPTATION: Lower thresholds for faster profile upgrades
static constexpr struct {
    float minRssi;
    float minSnr;       // SNR for more accurate determination
    int profileIndex;
} rssiToProfileTable[] = {
    // GFSK profiles for excellent signal (very aggressive thresholds)
    { -75.0f, 10.0f, 12 }, // Maximum GFSK speed (very low threshold)
    { -80.0f,  8.0f, 11 }, // GFSK fast (aggressive threshold)
    { -85.0f,  6.0f, 10 }, // GFSK medium (aggressive threshold)
    { -90.0f,  4.0f,  9 }, // GFSK conservative (aggressive threshold)
    
    // LoRa profiles for standard conditions (very aggressive thresholds)
    { -95.0f,  2.0f,  8 }, // LoRa maximum speed (very aggressive)
    { -100.0f, 0.0f,  7 }, // LoRa very fast (very aggressive)
    { -105.0f, -2.0f, 6 }, // LoRa speed + range (very aggressive)
    { -110.0f, -4.0f, 5 }, // LoRa fast with good signal (very aggressive)
    { -114.0f, -6.0f, 4 }, // LoRa medium, open terrain (very aggressive)
    { -116.0f, -8.0f, 3 }, // LoRa medium mode (very aggressive)
    { -118.0f, -10.0f, 2 }, // LoRa reliable compromise (very aggressive)
    { -119.0f, -12.0f, 1}, // LoRa very good stability (very aggressive)
    { -120.0f, -15.0f, 0}  // LoRa maximum reliability
};

// Number of entries in the table
constexpr size_t rssiProfileCount = sizeof(rssiToProfileTable) / sizeof(rssiToProfileTable[0]);



// ═══════════════════════════════════════════════════════════════════════════
// DEVICE IDS
// ═══════════════════════════════════════════════════════════════════════════
#define DEVICE_ID_MASTER    0x01    // Master node (Mission Control)
#define DEVICE_ID_SLAVE     0x02    // Slave node (Boat/Remote device)
#define DEVICE_ID_BROADCAST 0xFF    // Broadcast address

// ═══════════════════════════════════════════════════════════════════════════
// LORA PARAMETERS
// ═══════════════════════════════════════════════════════════════════════════
#define LORA_FREQUENCY      863.0  // MHz
#define LORA_BANDWIDTH      125.0   // kHz (can try 62.5 for longer range)
#define LORA_SF             12      // Spreading Factor (6 to 12, 11/12 for range)
#define LORA_CODING_RATE    7       // Coding Rate (5 to 8, 7/8 for reliability)
#define LORA_SYNC_WORD      0x16    // Must match on all devices
#define LORA_TX_POWER       22      // dBm (max 22 for SX1262, check regional limits)
#define LORA_PREAMBLE_LEN   8       // Preamble length (typically 8)

// ═══════════════════════════════════════════════════════════════════════════
// QUEUE SIZES
// ═══════════════════════════════════════════════════════════════════════════
#define LORA_INCOMING_QUEUE_SIZE 35
#define LORA_OUTGOING_QUEUE_SIZE 45

// ═══════════════════════════════════════════════════════════════════════════
// HARDWARE PIN CONFIGURATION (ESP32-S3 + SX1262)
// ═══════════════════════════════════════════════════════════════════════════
// Default pins for Heltec Wireless Stick Lite V3
#define LORA_SCK   9
#define LORA_MISO  11
#define LORA_MOSI  10
#define LORA_SS    8
#define LORA_RST   12
#define LORA_DIO1  14
#define LORA_BUSY  13
