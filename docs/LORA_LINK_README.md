# LoRa-Link Project - Standalone LoRa Communication System

A standalone, reusable LoRa communication library with adaptive profile switching, robust acknowledgment system, and explicit state machine design.

## 🎯 Project Goals

- **Clean Architecture**: Separation of core logic, platform dependencies, and applications
- **Explicit FSM**: Clear state machine for debugging and reliability
- **Adaptive Communication**: Automatic profile switching based on signal quality
- **Efficient ACKs**: Bulk acknowledgment system to reduce air time
- **Testability**: Unit tests and integration test framework
- **Portability**: Easy to integrate into other projects

## 📁 Project Structure

```
lora-link/
├── core/                      # Core LoRa logic (platform-independent)
│   ├── lora_hal.hpp/cpp       # Hardware abstraction layer
│   ├── lora_protocol.hpp/cpp  # Packet definitions and parsing
│   ├── lora_fsm.hpp/cpp       # Finite state machine
│   ├── lora_config.h          # Configuration and profiles
│   ├── lora_ack.hpp/cpp       # Acknowledgment system
│   └── lora_asa.hpp/cpp       # Adaptive Signal Adaptation
├── platform/                  # Platform-specific implementations
│   └── esp32_sx1262/         # ESP32 + SX1262 implementation
│       ├── platform_init.cpp
│       └── platform_config.h
├── apps/                      # Example applications
│   ├── master_node/          # Master node (Mission Control)
│   │   ├── main.cpp
│   │   └── master_logic.hpp
│   └── slave_node/           # Slave node (Boat)
│       ├── main.cpp
│       └── slave_logic.hpp
├── tools/                     # Testing and debugging tools
│   ├── pc_client.py          # Python CLI for testing
│   ├── packet_analyzer.py    # Log analysis tool
│   └── duty_cycle_checker.py # EU868 compliance checker
├── test/                      # Unit and integration tests
│   ├── test_protocol.cpp
│   ├── test_fsm.cpp
│   └── test_ack.cpp
├── docs/                      # Documentation
│   ├── FSM.md                # State machine design
│   ├── PROTOCOL.md           # Protocol specification
│   ├── API.md                # API reference
│   └── EXAMPLES.md           # Usage examples
├── platformio.ini            # PlatformIO configuration
├── README.md                 # This file
└── LICENSE
```

## 🔧 Hardware Requirements

### Minimum Setup (2 nodes):
- 2× ESP32-S3 microcontrollers
- 2× SX1262 LoRa transceivers
- USB cables for programming/debugging
- Antennas (863-870 MHz for EU)

### Tested Hardware:
- **Heltec Wireless Stick Lite V3** (ESP32-S3 + SX1262 integrated)
- Frequency: 863.21 MHz
- Max power: 22 dBm

### Pin Configuration (ESP32-S3):
```cpp
#define LORA_SCK   9
#define LORA_MISO  11
#define LORA_MOSI  10
#define LORA_SS    8
#define LORA_RST   12
#define LORA_DIO1  14
#define LORA_BUSY  13
```

## 📡 Features

### Communication Modes
- **LoRa**: 9 profiles (SF7-12, BW 125-500 kHz)
- **GFSK**: 4 profiles (19.2-100 kbps)
- **Auto-switching**: Based on RSSI/SNR thresholds

### Adaptive Signal Adaptation (ASA)
Automatically switches between profiles for optimal speed/range:
- Strong signal → Fast profiles (GFSK 100 kbps, LoRa SF7)
- Weak signal → Robust profiles (LoRa SF12)
- Dynamic timeout and retry adjustments

### Acknowledgment System
- **Single ACK**: For critical packets
- **Bulk ACK**: Aggregate up to 10 ACKs in one packet (90% reduction in ACK traffic)
- **Adaptive retries**: 2-4 attempts depending on profile

### State Machine
Explicit FSM with 10 states:
1. UNINITIALIZED
2. INIT
3. IDLE
4. RX_IN_PROGRESS
5. RX_PROCESSING
6. TX_PREPARE
7. TX_TRANSMIT
8. TX_WAIT_ACK
9. PROFILE_SWITCH
10. ERROR

All transitions logged for debugging.

## 🚀 Quick Start

### 1. Clone and Build

```bash
git clone https://github.com/yourusername/lora-link.git
cd lora-link
pio run
```

### 2. Flash Master Node

```bash
pio run -e master_node --target upload
```

### 3. Flash Slave Node

```bash
pio run -e slave_node --target upload
```

### 4. Test Communication

```bash
# Connect to master node
pio device monitor -e master_node

# In serial monitor:
> ping
[TX] PING sent, ID=1
[RX] PONG received, ID=2 (RTT: 67ms)

> send hello
[TX] Sent "hello", ID=3
[RX] ACK received, ID=3 (45ms)
```

## 📊 Performance

| Profile | Mode | Throughput | Range* | Latency (50B) |
|---------|------|------------|--------|---------------|
| 0       | LoRa SF12/125 | ~250 bps | 15+ km | 2000ms |
| 4       | LoRa SF8/250  | ~4 kbps  | 6 km   | 140ms |
| 8       | LoRa SF7/500  | ~21 kbps | 3 km   | 30ms |
| 12      | GFSK 100k     | ~80 kbps | 500m   | 4ms |

*Estimated range in ideal conditions (LOS, no interference)

## 🧪 Testing

### Unit Tests
```bash
pio test -e native
```

### Integration Tests
```bash
# Ping-Pong test
pio test -e master_node -f test_ping_pong

# Profile switching test
pio test -e master_node -f test_profile_switch

# Stress test (1000 packets)
pio test -e master_node -f test_stress
```

### Python Testing Tools

```bash
# Interactive CLI
python tools/pc_client.py --port COM17

# Analyze logs
python tools/packet_analyzer.py logs/session_2025-11-26.log

# Check duty cycle compliance
python tools/duty_cycle_checker.py logs/session_2025-11-26.log
```

## 📖 API Usage

### Basic Send/Receive

```cpp
#include "lora_core.hpp"

LoRaCore lora(0x01);  // Device ID 0x01

void setup() {
    lora.begin();
}

void loop() {
    // Send packet
    LoRaPacket pkt;
    pkt.senderId = 0x01;
    pkt.receiverId = 0x02;
    pkt.packetType = 'C';  // Command
    pkt.payloadLen = 5;
    memcpy(pkt.payload, "hello", 5);
    lora.send(pkt, true);  // wait for ACK
    
    // Receive packet
    LoRaPacket rxPkt;
    if (lora.receive(rxPkt)) {
        Serial.printf("Received from %d: %s\n", 
                      rxPkt.senderId, rxPkt.payload);
    }
}
```

### Profile Management

```cpp
// Manual profile switch
lora.setProfile(8);  // Switch to LoRa SF7/500

// Enable adaptive switching
lora.enableASA(true);

// Get current profile info
String info = lora.getCurrentProfileInfo();
Serial.println(info);  // "LoRa #8: SF=7, CR=5, BW=500.0kHz"
```

### Callbacks

```cpp
// ACK callback
lora.setAckCallback([](uint8_t packetId, bool success) {
    if (success) {
        Serial.printf("Packet %d acknowledged\n", packetId);
    } else {
        Serial.printf("Packet %d failed\n", packetId);
    }
});

// RSSI/SNR monitoring
lora.setSignalCallback([](float rssi, float snr) {
    Serial.printf("Signal: RSSI=%.1f dBm, SNR=%.1f dB\n", rssi, snr);
});
```

## 🔍 Debugging

### Enable Verbose Logging

```cpp
lora.setLogLevel(LOG_DEBUG);  // or LOG_TRACE for packet dumps
```

### State Machine Logging

All FSM transitions are logged:
```
[12345] [FSM] IDLE + EVT_TX_REQUEST -> TX_PREPARE
[12350] [FSM] TX_PREPARE -> TX_TRANSMIT [to=0x02]
[12370] [FSM] TX_TRANSMIT + EVT_TX_DONE -> TX_WAIT_ACK [timeout=3200ms]
[12650] [FSM] TX_WAIT_ACK + EVT_ACK_RECEIVED -> IDLE [success]
```

### Packet Analyzer

```bash
python tools/packet_analyzer.py session.log
```

Output:
```
=== Packet Statistics ===
Total packets: 150
TX: 75 (50.0%)
RX: 75 (50.0%)
ACKs sent: 60
ACKs received: 58
Lost packets: 2 (2.7%)
Average RSSI: -89.5 dBm
Average SNR: 7.2 dB
Profile distribution:
  Profile 8: 100 packets (66.7%)
  Profile 4: 50 packets (33.3%)
```

## ⚠️ Compliance

### EU863-870 ISM Band
- **Frequency**: 863-870 MHz
- **Max ERP**: 25 mW (14 dBm) unlimited duty cycle, or 500 mW (27 dBm) with < 1% duty cycle
- **Current setting**: 22 dBm (158 mW)

**Important**: Duty cycle limiter is implemented in firmware to ensure < 1% duty cycle compliance.

Monitor duty cycle:
```bash
python tools/duty_cycle_checker.py session.log
```

## 🤝 Contributing

Contributions welcome! Please see [CONTRIBUTING.md](CONTRIBUTING.md).

### Development Workflow
1. Fork repository
2. Create feature branch (`git checkout -b feature/my-feature`)
3. Write tests
4. Implement feature
5. Run tests (`pio test`)
6. Commit changes
7. Push to branch
8. Create Pull Request

## 📝 License

MIT License - see [LICENSE](LICENSE) for details.

## 🙏 Credits

- **RadioLib**: Excellent LoRa library by [jgromes](https://github.com/jgromes/RadioLib)
- **PlatformIO**: Build system
- **ESP32**: Platform support
- Original implementation in boat control system

## 📞 Support

- **Issues**: [GitHub Issues](https://github.com/yourusername/lora-link/issues)
- **Documentation**: [docs/](docs/)
- **Examples**: [apps/](apps/)

## 🗺️ Roadmap

### v1.0 (Current)
- [x] Core protocol implementation
- [x] FSM design
- [x] Bulk ACK system
- [x] Adaptive profile switching
- [ ] Unit tests (80% coverage)
- [ ] Integration tests
- [ ] Python tools

### v1.1 (Planned)
- [ ] AES-128 encryption
- [ ] HMAC authentication
- [ ] Web dashboard (real-time RSSI/SNR graphs)
- [ ] OTA firmware update via LoRa

### v2.0 (Future)
- [ ] Mesh networking support
- [ ] Power optimization (sleep modes)
- [ ] Multiple frequency channels
- [ ] Longer range optimizations

## 📚 Additional Documentation

- [State Machine Design (FSM.md)](docs/FSM.md)
- [Protocol Specification (PROTOCOL.md)](docs/PROTOCOL.md)
- [API Reference (API.md)](docs/API.md)
- [Examples (EXAMPLES.md)](docs/EXAMPLES.md)

---

**Status**: 🚧 In Development - Not Yet Ready for Production

**Last Updated**: November 26, 2025
