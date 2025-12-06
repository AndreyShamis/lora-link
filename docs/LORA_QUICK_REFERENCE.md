# LoRa-Link Quick Reference Card

## 🎯 Quick Commands

### Serial Monitor Commands (Master Node)

```
# Basic Communication
ping              - Test connection
send <text>       - Send text message
stats             - Show statistics

# Profile Management
profile <0-12>    - Switch to specific profile
profile auto      - Enable adaptive switching
profile info      - Show current profile

# Diagnostics
rssi              - Show current RSSI/SNR
log clear         - Clear log buffer
log dump          - Dump all logs
reset             - Reset device

# Testing
stress <count>    - Send N test packets
range             - Range test mode
scan              - Scan for devices
```

## 📊 Profile Quick Reference

| # | Mode | SF/Bitrate | Range | Speed | Use Case |
|---|------|------------|-------|-------|----------|
| 0 | LoRa | SF12/125kHz | 15km+ | 250bps | Maximum range |
| 4 | LoRa | SF8/250kHz | 6km | 4kbps | Balanced |
| 8 | LoRa | SF7/500kHz | 3km | 21kbps | Fast LoRa |
| 12 | GFSK | 100kbps | 500m | 80kbps | Maximum speed |

## 🔤 Packet Types

| Code | Name | Direction | Purpose |
|------|------|-----------|---------|
| 'C' | COMMAND | MC→Boat | Control command |
| 'K' | ACK | Both | Single acknowledgment |
| 'B' | BULK_ACK | Both | Multiple ACKs (up to 10) |
| ')' | REQUEST_ASA | Both | Request profile switch |
| '(' | RESPONSE_ASA | Both | Confirm profile switch |
| '-' | PING | Both | Connection test |
| 'O' | PONG | Both | PING response |
| 'R' | RSSI_REPORT | Boat→MC | Signal quality |
| 'T' | TELEMETRY | Boat→MC | Sensor data |
| 'G' | NAV | Boat→MC | GPS data |

## 🔧 Hardware Pins (ESP32-S3)

```
SPI Bus:
  SCK:  GPIO 9
  MISO: GPIO 11
  MOSI: GPIO 10
  SS:   GPIO 8

LoRa Control:
  RST:  GPIO 12
  DIO1: GPIO 14
  BUSY: GPIO 13
```

## 📡 Radio Settings

```
Frequency: 863.21 MHz
TX Power:  22 dBm (158 mW)
Bandwidth: 125/250/500 kHz (LoRa)
           117-234 kHz (GFSK)
SF:        7-12 (LoRa)
Bitrate:   19.2k-100k (GFSK)
```

## 🚦 FSM States

```
0: UNINITIALIZED  - Not started
1: INIT          - Configuring hardware
2: IDLE          - Ready (listening)
3: RX_IN_PROGRESS - Receiving packet
4: RX_PROCESSING  - Parsing data
5: TX_PREPARE     - Preparing to send
6: TX_TRANSMIT    - Transmitting
7: TX_WAIT_ACK    - Waiting for ACK
8: ERROR          - Error state
9: PROFILE_SWITCH - Changing profile
```

## 📈 RSSI/SNR Thresholds

| RSSI (dBm) | SNR (dB) | Recommended Profile |
|------------|----------|---------------------|
| > -75 | > +10 | 12 (GFSK 100k) |
| -75 to -85 | +6 to +10 | 10-11 (GFSK 38-50k) |
| -85 to -95 | +2 to +6 | 8-9 (LoRa SF7/GFSK 19k) |
| -95 to -110 | -4 to +2 | 5-7 (LoRa SF7-9 fast) |
| -110 to -118 | -10 to -4 | 2-4 (LoRa SF8-10) |
| < -118 | < -10 | 0-1 (LoRa SF11-12) |

## 🔍 Debugging Snippets

### Check if LoRa is working:
```cpp
if (!lora.begin()) {
    Serial.println("ERROR: LoRa init failed");
    while(1);
}
Serial.println("LoRa OK");
```

### Monitor state changes:
```cpp
lora.setLogLevel(LOG_DEBUG);
lora.onStateChange([](LoRaState from, LoRaState to) {
    Serial.printf("FSM: %d -> %d\n", from, to);
});
```

### Force profile:
```cpp
lora.setProfile(4);  // SF8/250kHz
lora.enableASA(false);  // Disable auto-switching
```

### Read signal quality:
```cpp
float rssi = lora.getRSSI();
float snr = lora.getSNR();
Serial.printf("RSSI: %.1f dBm, SNR: %.1f dB\n", rssi, snr);
```

## ⚡ Performance Tips

### Maximize Range:
- Use profile 0 (SF12/125kHz)
- Ensure good antenna placement
- Avoid obstacles
- Position antennas vertically

### Maximize Speed:
- Use profile 12 (GFSK 100k) when RSSI > -75 dBm
- Enable bulk ACKs
- Reduce packet size
- Keep distance < 500m

### Minimize Power:
- Use lowest TX power that works
- Enable sleep mode between transmissions
- Use slow profiles (less retries needed)

### Improve Reliability:
- Enable adaptive switching (ASA)
- Use bulk ACKs
- Monitor duty cycle
- Implement exponential backoff for retries

## 🛠️ Troubleshooting

### No packets received:
```
1. Check wiring (SPI pins)
2. Verify frequency matches (863.21 MHz)
3. Check antenna connection
4. Enable verbose logging
5. Try profile 0 (most robust)
```

### Frequent timeouts:
```
1. Check RSSI (if < -115 dBm, reduce distance)
2. Switch to slower profile
3. Increase retry count
4. Check for interference
```

### High packet loss:
```
1. Enable ASA (auto-adaptation)
2. Check duty cycle (< 1% for EU)
3. Reduce TX power if too close
4. Try different profile
```

### CRC errors:
```
1. Check for EMI sources
2. Improve antenna
3. Reduce distance
4. Switch to more robust profile
```

## 📞 Error Codes

| Code | Meaning | Action |
|------|---------|--------|
| -2 | CHIP_NOT_FOUND | Check wiring, SPI config |
| -4 | TX_TIMEOUT | Reduce packet size, lower power |
| -5 | RX_TIMEOUT | Increase timeout, check peer |
| -6 | CRC_MISMATCH | Check for interference |
| -7 | INVALID_BANDWIDTH | Fix configuration |
| -8 | INVALID_SF | Use SF 7-12 |

## 🧪 Test Scenarios

### Basic Connectivity:
```
1. Flash both nodes
2. Send PING from master
3. Expect PONG reply (< 100ms)
4. Success: RTT displayed
```

### Profile Adaptation:
```
1. Start at profile 0
2. Walk closer (monitor RSSI)
3. Observe auto-switch to faster profiles
4. Walk away, observe switch back
```

### Stress Test:
```
1. Send 100 packets rapidly
2. Monitor ACK rate (should be > 95%)
3. Check average latency
4. Verify no memory leaks
```

### Range Test:
```
1. Set profile 0 (maximum range)
2. Send PING every 5 seconds
3. Walk away slowly
4. Note distance when packets start failing
```

## 🎓 Best Practices

### DO:
- ✅ Use bulk ACKs for non-critical data
- ✅ Enable ASA in mobile scenarios
- ✅ Monitor duty cycle
- ✅ Log all state transitions
- ✅ Use hardware CRC

### DON'T:
- ❌ Disable retries for important commands
- ❌ Send packets faster than 1 per 5 seconds (SF12)
- ❌ Ignore RSSI/SNR trends
- ❌ Use broadcast for everything
- ❌ Exceed 1% duty cycle (EU)

## 🔐 Security Notes

**Current**: No encryption (plaintext)

**Production**: Must implement:
- AES-128 encryption
- HMAC authentication
- Rolling codes
- Device pairing

## 📚 More Info

- Full docs: `docs/`
- Protocol spec: `docs/PROTOCOL.md`
- FSM design: `docs/FSM.md`
- API reference: `docs/API.md`
- Examples: `apps/`

---

**Print this page for quick reference during development/testing!**

Last updated: November 26, 2025
