# LoRa-Link

Standalone LoRa communication library with adaptive profile switching, bulk acknowledgment system, and explicit finite state machine.

## 🚀 Quick Start

### Hardware
- 2× ESP32-S3 boards (Heltec Wireless Stick Lite V3)
- 2× LoRa antennas (863-870 MHz)

### Build & Flash

```bash
# Master node
pio run -e master_node --target upload

# Slave node  
pio run -e slave_node --target upload
```

### Test Communication

```bash
# Monitor master node
pio device monitor -e master_node

# Commands:
ping          # Test connection
send hello    # Send message
stats         # Show statistics
profile 4     # Switch to profile 4
```

## 📁 Project Structure

```
lora-link/
├── core/                   # Core LoRa logic
│   ├── LoRaCore.hpp       # Main LoRa class
│   ├── LoRaCore.cpp
│   ├── lora_protocol.hpp  # Packet definitions
│   └── lora_config.h      # Configuration
├── platform/              # Platform-specific code
│   └── esp32_sx1262/
├── apps/                  # Example applications
│   ├── master_node/
│   └── slave_node/
├── tools/                 # Python utilities
├── test/                  # Unit tests
├── docs/                  # Documentation
└── platformio.ini
```

## 🌟 Features

- **13 Adaptive Profiles**: 9 LoRa (SF7-12) + 4 GFSK (19.2-100 kbps)
- **Broadcast Support**: Send packets to all nodes (0xFF address)
- **Bulk ACK System**: Up to 10 ACKs in one packet (90% reduction)
- **Adaptive Retry Logic**: Dynamic timeouts and retry counts
- **RSSI/SNR Based Switching**: Automatic profile optimization
- **FreeRTOS Integration**: Queues, semaphores, tasks

## 📊 Profiles

| Profile | Mode | SF/Bitrate | Range | Speed | Use Case |
|---------|------|------------|-------|-------|----------|
| 0 | LoRa | SF12/125kHz | 15km+ | 250bps | Maximum range |
| 4 | LoRa | SF8/250kHz | 6km | 4kbps | Balanced |
| 8 | LoRa | SF7/500kHz | 3km | 21kbps | Fast LoRa |
| 12 | GFSK | 100kbps | 500m | 80kbps | Maximum speed |

## 📖 Documentation

- [Protocol Specification](../aboat/docs/LORA_PROTOCOL_SPEC.md)
- [FSM Design](../aboat/docs/LORA_FSM_DESIGN.md)
- [Quick Reference](../aboat/docs/LORA_QUICK_REFERENCE.md)

## 🧪 Testing

```bash
# Unit tests
pio test -e native

# Integration tests
pio test -e master_node
```

## ⚖️ License

MIT License - See LICENSE file

## 🙏 Credits

- RadioLib by jgromes
- Original implementation from boat control project

---

**Status**: ✅ Ready for development  
**Version**: 0.1.0  
**Last Updated**: November 26, 2025
