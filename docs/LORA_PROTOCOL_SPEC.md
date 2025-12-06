# LoRa Protocol Specification v1.0

## Overview

This document describes the complete LoRa communication protocol used for boat-to-mission-control communication. The protocol supports both LoRa and GFSK modulation, adaptive profile switching, and efficient packet acknowledgment.

---

## 1. Physical Layer

### 1.1. Hardware
- **Transceiver**: SX1262
- **Frequency**: 863.21 MHz (EU863-870 ISM band)
- **Max Power**: 22 dBm
- **Interface**: SPI (4-wire)

### 1.2. Modulation Modes

#### LoRa Mode (Profiles 0-8):
| Profile | SF | BW (kHz) | CR | Bitrate* | ToA** (50B) | Range*** |
|---------|----|-----------|----|----------|-------------|----------|
| 0       | 12 | 125       | 7  | ~250 bps | ~2000ms     | 15+ km   |
| 1       | 11 | 125       | 7  | ~440 bps | ~1100ms     | 12 km    |
| 2       | 10 | 125       | 7  | ~980 bps | ~560ms      | 10 km    |
| 3       | 9  | 250       | 6  | ~2.2 kbps| ~280ms      | 8 km     |
| 4       | 8  | 250       | 6  | ~4.4 kbps| ~140ms      | 6 km     |
| 5       | 7  | 250       | 5  | ~9.4 kbps| ~66ms       | 4 km     |
| 6       | 9  | 500       | 5  | ~4.8 kbps| ~130ms      | 7 km     |
| 7       | 8  | 500       | 5  | ~9.8 kbps| ~65ms       | 5 km     |
| 8       | 7  | 500       | 5  | ~21 kbps | ~30ms       | 3 km     |

*Approximate effective bitrate  
**Time on Air for 50-byte packet  
***Estimated range in ideal conditions

#### GFSK Mode (Profiles 9-12):
| Profile | Bitrate | Deviation | RxBW | ToA (50B) | Range*** |
|---------|---------|-----------|------|-----------|----------|
| 9       | 19.2 kbps | 10 kHz | 117.3 kHz | ~21ms | 2 km |
| 10      | 38.4 kbps | 20 kHz | 156.2 kHz | ~10ms | 1.5 km |
| 11      | 50 kbps   | 25 kHz | 187.2 kHz | ~8ms  | 1 km |
| 12      | 100 kbps  | 50 kHz | 234.3 kHz | ~4ms  | 500m |

### 1.3. Radio Settings
```cpp
// Common settings
frequency = 863.21 MHz
txPower = 22 dBm
preambleLength = 8 symbols (LoRa) or 32 bits (GFSK)
syncWord = 0x16
crcEnabled = true (hardware CRC via RadioLib)
```

---

## 2. Data Link Layer

### 2.1. Frame Structure

```
┌──────────────────────────────────────────────────────────────┐
│ PHYSICAL LAYER (RadioLib adds: Preamble, Sync, CRC)        │
└──────────────────────────────────────────────────────────────┘
         ↓
┌──────────────────────────────────────────────────────────────┐
│ APPLICATION PACKET (our protocol)                            │
│ ┌────────────┬─────────────┬──────────────┬─────────────┐   │
│ │   Header   │   Payload   │   Optional   │   CRC16     │   │
│ │  (5 bytes) │ (0-85 bytes)│   App-level  │  (optional) │   │
│ └────────────┴─────────────┴──────────────┴─────────────┘   │
└──────────────────────────────────────────────────────────────┘
```

### 2.2. Header Format (5 bytes)

```cpp
#pragma pack(push, 1)
struct LoRaPacket {
    uint8_t  senderId;      // 0: Source device ID
    uint8_t  receiverId;    // 1: Destination device ID (0xFF = broadcast)
    uint8_t  packetType;    // 2: Packet type (see section 3)
    uint8_t  packetId;      // 3: Sequence number (0-255, wraps around)
    uint8_t  payloadLen;    // 4: Payload length (0-85)
    uint8_t  payload[85];   // 5+: Actual payload data
};
#pragma pack(pop)
```

**Field Descriptions**:

- **senderId**: Unique device identifier
  - `0x01` = Boat
  - `0x02` = Mission Control
  - `0x03-0xFE` = Reserved for additional devices
  - `0xFF` = Invalid/broadcast

- **receiverId**: Target device
  - Specific device ID or `0xFF` for broadcast
  - Devices ignore packets not addressed to them (except broadcast)

- **packetType**: Message type (ASCII char, see section 3)

- **packetId**: Sequential counter
  - Increments for each new packet
  - Wraps around at 255 → 0
  - Used for ACK matching and duplicate detection

- **payloadLen**: Number of valid bytes in payload
  - Must be ≤ 85 (MAX_LORA_PAYLOAD)
  - Can be 0 for control packets (PING, PONG)

### 2.3. Payload

Variable length (0-85 bytes). Content depends on packetType.

### 2.4. CRC16-CCITT (Optional Application-Level)

RadioLib provides hardware CRC, but application-level CRC can be added for extra validation:

```cpp
uint16_t calcCRC16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}
```

---

## 3. Packet Types

| Type Code | Name | Direction | Description | Payload Size |
|-----------|------|-----------|-------------|--------------|
| `'C'` | COMMAND_STRING | MC→Boat | Text command | Variable |
| `'Y'` | COMMAND_RESPONSE | Boat→MC | Command response | Variable |
| `'T'` | TELEMETRY_FRAGMENT | Boat→MC | JSON telemetry | Variable |
| `'I'` | INFO_ENGINE | Boat→MC | Engine info | 3 bytes |
| `'S'` | STATUS | Boat→MC | System status | 1 byte |
| `'F'` | CONFIG | MC→Boat | Configuration | 2 bytes |
| `'G'` | NAV | Boat→MC | GPS data | 10 bytes |
| `'K'` | ACK | Both | Single ACK | 1 byte |
| `'B'` | BULK_ACK | Both | Bulk ACK (up to 10) | 1-11 bytes |
| `')'` | REQUEST_ASA | Both | Request profile switch | 1 byte |
| `'('` | RESPONSE_ASA | Both | Confirm profile switch | 1 byte |
| `'Q'` | GET_BOAT_STATUS | MC→Boat | Request status | 0 bytes |
| `'D'` | BOAT_STATUS_REPORT | Boat→MC | Full status report | Variable |
| `'W'` | REQUEST_INFO | Both | Generic info request | 1 byte |
| `'-'` | PING | Both | Connection test | 0 bytes |
| `'O'` | PONG | Both | PING response | 0 bytes |
| `'R'` | RSSI_REPORT | Boat→MC | Signal quality | 8 bytes |

---

## 4. Packet Definitions

### 4.1. COMMAND_STRING (`'C'`)

**Purpose**: Send text command from Mission Control to Boat

**Payload**:
```
null-terminated string or binary command structure
```

**Example**:
```cpp
// Command: "M:S,100,0" (set motor speed)
LoRaPacket pkt;
pkt.senderId = 0x02;     // Mission Control
pkt.receiverId = 0x01;   // Boat
pkt.packetType = 'C';
pkt.packetId = 42;
pkt.payloadLen = 9;
memcpy(pkt.payload, "M:S,100,0", 9);
```

### 4.2. ACK (`'K'`)

**Purpose**: Acknowledge receipt of a packet

**Payload** (1 byte):
```cpp
struct {
    uint8_t ackedId;  // packetId being acknowledged
};
```

**Example**:
```cpp
// ACK for packet ID 42
PacketAck ack;
ack.packetType = 'K';
ack.packetId = 43;       // this ACK's own ID
ack.ackedId = 42;        // acknowledging packet 42
ack.payloadLen = 1;
```

### 4.3. BULK_ACK (`'B'`)

**Purpose**: Acknowledge multiple packets in one transmission (up to 10)

**Payload**:
```cpp
struct {
    uint8_t count;           // number of ACKs (1-10)
    uint8_t ackedIds[10];    // array of packetIds
};
```

**Example**:
```cpp
// Bulk ACK for packets 40, 41, 42
PacketBulkAck bulk;
bulk.packetType = 'B';
bulk.packetId = 50;
bulk.count = 3;
bulk.ackedIds[0] = 40;
bulk.ackedIds[1] = 41;
bulk.ackedIds[2] = 42;
bulk.payloadLen = 1 + 3;  // count + 3 IDs
```

**Rules**:
- No duplicate IDs in same BULK_ACK
- Send when buffer is full (10 ACKs) or timeout (250-1800ms depending on profile)

### 4.4. REQUEST_ASA (`')'`) / RESPONSE_ASA (`'('`)

**Purpose**: Adaptive Signal Adaptation - switch to better/worse profile

**Payload** (1 byte):
```cpp
struct {
    uint8_t profileIndex;  // 0-12
};
```

**Protocol Flow**:
```
Device A (stronger signal)          Device B (weaker signal)
    |                                      |
    | ─── REQUEST_ASA (profile=8) ───────→ |
    |                                      | (applies profile 8)
    | ←─── RESPONSE_ASA (profile=8) ────── |
    | (applies profile 8)                  |
    | ─── ACK_ASA ────────────────────────→ |
    |                                      |
    [Both now on profile 8]
```

**Rules**:
- Initiator proposes profile based on local RSSI/SNR
- Responder must apply profile before responding
- Both devices use new profile after handshake
- Timeout: 15 seconds (revert to previous profile if no response)

### 4.5. PING (`'-'`) / PONG (`'O'`)

**Purpose**: Test connectivity and measure round-trip time

**Payload**: None (0 bytes)

**Example**:
```
Mission Control                    Boat
    |                               |
    | ─── PING (ID=100) ──────────→ |
    |                               |
    | ←─── PONG (ID=101) ────────── |
    |                               |
    RTT = time_pong - time_ping
```

### 4.6. TELEMETRY_FRAGMENT (`'T'`)

**Purpose**: Send telemetry data (JSON fragments)

**Payload**: Variable, typically JSON string

**Example**:
```json
{"lat":51.5074,"lon":-0.1278,"spd":5.2}
```

### 4.7. INFO_ENGINE (`'I'`)

**Purpose**: Engine/motor information

**Payload** (3 bytes):
```cpp
struct {
    int16_t rpm;   // 0-20000 RPM
    int8_t temp;   // -40 to +85 °C
};
```

### 4.8. NAV (`'G'`)

**Purpose**: GPS navigation data

**Payload** (10 bytes):
```cpp
struct {
    int32_t lat;    // latitude * 1e7 (decimal degrees)
    int32_t lon;    // longitude * 1e7
    uint16_t hdop;  // HDOP * 100
};
```

**Example**:
```cpp
// GPS: 51.5074° N, 0.1278° W, HDOP=1.2
PacketNav nav;
nav.lat = 515074000;   // 51.5074 * 1e7
nav.lon = -1278000;    // -0.1278 * 1e7
nav.hdop = 120;        // 1.2 * 100
```

### 4.9. RSSI_REPORT (`'R'`)

**Purpose**: Report signal quality metrics

**Payload** (8 bytes):
```cpp
struct {
    float rawRssi;       // current RSSI (dBm)
    float smoothedRssi;  // filtered RSSI (dBm)
};
```

---

## 5. Adaptive Signal Adaptation (ASA)

### 5.1. Profile Selection Algorithm

```cpp
int selectProfileByRSSI(float rssi, float snr) {
    for (int i = 0; i < rssiProfileCount; i++) {
        if (rssi >= rssiToProfileTable[i].minRssi &&
            snr >= rssiToProfileTable[i].minSnr) {
            return rssiToProfileTable[i].profileIndex;
        }
    }
    return 0; // fallback to most robust profile
}
```

### 5.2. RSSI/SNR Thresholds

| Profile | Min RSSI (dBm) | Min SNR (dB) | Mode |
|---------|----------------|--------------|------|
| 12      | -75            | +10          | GFSK 100k |
| 11      | -80            | +8           | GFSK 50k |
| 10      | -85            | +6           | GFSK 38.4k |
| 9       | -90            | +4           | GFSK 19.2k |
| 8       | -95            | +2           | LoRa SF7/500 |
| 7       | -100           | 0            | LoRa SF8/500 |
| 6       | -105           | -2           | LoRa SF9/500 |
| 5       | -110           | -4           | LoRa SF7/250 |
| 4       | -114           | -6           | LoRa SF8/250 |
| 3       | -116           | -8           | LoRa SF9/250 |
| 2       | -118           | -10          | LoRa SF10/125 |
| 1       | -119           | -12          | LoRa SF11/125 |
| 0       | -120           | -15          | LoRa SF12/125 |

### 5.3. Adaptive Parameters

Each profile has dynamic timeout and retry settings:

```cpp
void updateRetryParameters(int profile) {
    if (profile <= 8) {  // LoRa
        int sf = profiles[profile].spreadingFactor;
        float bw = profiles[profile].bandwidth;
        
        // Calculate packet time
        float symbolTime = (1 << sf) / (bw * 1000);
        float packetTime = (8 + 4.25 + payloadSymbols) * symbolTime * 1000;
        
        // Adaptive timeout
        currentRetryTimeoutMs = max(8500, (uint32_t)(packetTime * 3.5 + 1000));
        
        // Adaptive retries
        if (sf <= 7) currentMaxRetries = 2;
        else if (sf <= 9) currentMaxRetries = 3;
        else currentMaxRetries = 4;
        
        // Bulk ACK timing
        if (sf <= 7) {
            BULK_ACK_INTERVAL_MS = 1800;
            BULK_ACK_MAX_WAIT_MS = 1200;
        } else if (sf <= 9) {
            BULK_ACK_INTERVAL_MS = 2500;
            BULK_ACK_MAX_WAIT_MS = 1500;
        } else {
            BULK_ACK_INTERVAL_MS = 3000;
            BULK_ACK_MAX_WAIT_MS = 1800;
        }
    } else {  // GFSK
        uint32_t bitrate = profiles[profile].bitrate;
        float packetTime = (50 * 8 * 1000.0) / bitrate;
        
        currentRetryTimeoutMs = max(1500, (uint32_t)(packetTime * 2.5 + 600));
        currentMaxRetries = (bitrate >= 19200) ? 2 : 3;
        
        BULK_ACK_INTERVAL_MS = 600;
        BULK_ACK_MAX_WAIT_MS = 250;
    }
}
```

---

## 6. Quality of Service

### 6.1. Packet Priority

Not explicitly implemented, but effective priority via queue management:

1. **Highest**: ACK, PONG (respond immediately)
2. **High**: COMMAND_STRING, REQUEST_ASA
3. **Medium**: TELEMETRY, STATUS
4. **Low**: RSSI_REPORT, periodic heartbeats

### 6.2. Retry Logic

```cpp
struct PendingSend {
    LoRaPacket pkt;
    uint32_t timestamp;
    uint8_t retries;
};

// Retry decision
if (millis() - pending.timestamp > currentRetryTimeoutMs) {
    pending.retries++;
    if (pending.retries < currentMaxRetries) {
        // Retry
        transmit(pending.pkt);
        pending.timestamp = millis();
    } else {
        // Give up
        log_error("Packet ID %d failed after %d retries", 
                  pending.pkt.packetId, currentMaxRetries);
        removePending(pending.pkt.packetId);
    }
}
```

### 6.3. Duplicate Detection

```cpp
// Simple method: track last N received packet IDs
#define RECENT_PACKET_BUFFER_SIZE 10
uint8_t recentPackets[RECENT_PACKET_BUFFER_SIZE] = {0xFF, ...};
int recentIndex = 0;

bool isDuplicate(uint8_t packetId) {
    for (int i = 0; i < RECENT_PACKET_BUFFER_SIZE; i++) {
        if (recentPackets[i] == packetId) return true;
    }
    recentPackets[recentIndex] = packetId;
    recentIndex = (recentIndex + 1) % RECENT_PACKET_BUFFER_SIZE;
    return false;
}
```

---

## 7. Performance Metrics

### 7.1. Expected Throughput

| Profile | Effective Throughput* | Latency (50B) | Packet Loss @ 1km |
|---------|----------------------|----------------|-------------------|
| 0       | ~200 bps             | 2000ms         | < 0.1%            |
| 4       | ~3 kbps              | 150ms          | < 1%              |
| 8       | ~15 kbps             | 35ms           | < 5%              |
| 12      | ~60 kbps             | 6ms            | < 10%             |

*Including protocol overhead and ACKs

### 7.2. Reliability

With retry logic (max 4 retries):

| Packet Loss Rate | Delivery Success Rate |
|------------------|-----------------------|
| 5%               | > 99.9%               |
| 10%              | > 99.5%               |
| 20%              | > 98%                 |
| 50%              | > 90%                 |

---

## 8. Security Considerations

### 8.1. Current Status
- **Encryption**: None (plaintext)
- **Authentication**: Device ID only (not cryptographically secure)
- **Integrity**: CRC16 (detects errors, not tampering)

### 8.2. Future Enhancements
- AES-128 encryption for sensitive payloads
- HMAC for authentication
- Rolling codes for replay attack prevention
- Key exchange protocol

---

## 9. Compliance

### 9.1. EU863-870 ISM Band Regulations
- **Frequency**: 863-870 MHz
- **Max ERP**: 25 mW (14 dBm) for duty cycle unlimited, or 500 mW (27 dBm) with < 1% duty cycle
- **Current Config**: 22 dBm (158 mW) - compliant if duty cycle < 1%

**Duty Cycle Calculation**:
```
Duty cycle = (TX time) / (Observation period)
For SF12, 50-byte packet: ~2 seconds
To stay under 1%: max 1 packet every 200 seconds
For SF7, 50-byte packet: ~50ms
To stay under 1%: max 1 packet every 5 seconds
```

**Recommendation**: Implement duty cycle limiter in firmware.

---

## 10. Testing & Debugging

### 10.1. Packet Log Format

```
[timestamp] [DIR] [FROM→TO] Type=X, ID=Y, Len=Z, RSSI=R, SNR=S [payload_hex]
```

Example:
```
[12345] [TX] [0x02→0x01] Type='C', ID=42, Len=9, RSSI=N/A, SNR=N/A [4D3A532C3130302C30]
[12400] [RX] [0x01→0x02] Type='K', ID=43, Len=1, RSSI=-85.2, SNR=8.5 [2A]
```

### 10.2. Protocol Analyzer

Python script to parse binary logs:

```python
def parse_packet(data):
    pkt = {
        'senderId': data[0],
        'receiverId': data[1],
        'packetType': chr(data[2]),
        'packetId': data[3],
        'payloadLen': data[4],
        'payload': data[5:5+data[4]]
    }
    return pkt

def print_packet(pkt, direction, rssi=None, snr=None):
    print(f"[{direction}] [{pkt['senderId']:02X}→{pkt['receiverId']:02X}] "
          f"Type='{pkt['packetType']}', ID={pkt['packetId']}, "
          f"Len={pkt['payloadLen']}", end="")
    if rssi: print(f", RSSI={rssi:.1f}", end="")
    if snr: print(f", SNR={snr:.1f}", end="")
    print(f" [{pkt['payload'].hex()}]")
```

---

## 11. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0     | 2025-11-26 | Initial | First complete specification |

---

## Appendix A: Packet Type Reference

Quick lookup table for all packet types with typical use cases.

| Type | Name | Typical Use | ACK Required | Max Payload |
|------|------|-------------|--------------|-------------|
| C | COMMAND_STRING | Motor control, navigation | Yes | 85 |
| Y | COMMAND_RESPONSE | Command acknowledgment | No | 85 |
| T | TELEMETRY_FRAGMENT | Sensor data streaming | No | 85 |
| I | INFO_ENGINE | Motor diagnostics | No | 3 |
| S | STATUS | System health check | No | 1 |
| F | CONFIG | Change settings | Yes | 2 |
| G | NAV | GPS position | No | 10 |
| K | ACK | Single confirmation | No | 1 |
| B | BULK_ACK | Multiple confirmations | No | 1-11 |
| ) | REQUEST_ASA | Propose profile switch | Yes | 1 |
| ( | RESPONSE_ASA | Confirm profile switch | Yes | 1 |
| Q | GET_BOAT_STATUS | Request full status | Yes | 0 |
| D | BOAT_STATUS_REPORT | Full status dump | No | 85 |
| W | REQUEST_INFO | Generic query | Yes | 1 |
| - | PING | Connection test | Yes | 0 |
| O | PONG | PING reply | No | 0 |
| R | RSSI_REPORT | Signal quality | No | 8 |

---

## Appendix B: Error Codes

RadioLib error codes (for reference):

| Code | Name | Description |
|------|------|-------------|
| 0 | ERR_NONE | Success |
| -1 | ERR_UNKNOWN | Unknown error |
| -2 | ERR_CHIP_NOT_FOUND | SX1262 not responding |
| -3 | ERR_PACKET_TOO_LONG | Packet exceeds max size |
| -4 | ERR_TX_TIMEOUT | Transmission timeout |
| -5 | ERR_RX_TIMEOUT | Reception timeout |
| -6 | ERR_CRC_MISMATCH | CRC check failed |
| -7 | ERR_INVALID_BANDWIDTH | Invalid BW parameter |
| -8 | ERR_INVALID_SPREADING_FACTOR | Invalid SF parameter |
| -9 | ERR_INVALID_CODING_RATE | Invalid CR parameter |
| -10 | ERR_INVALID_FREQUENCY | Frequency out of range |
| -11 | ERR_INVALID_OUTPUT_POWER | TX power out of range |

---

**End of Protocol Specification**
