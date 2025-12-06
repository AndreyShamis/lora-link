// lora_packets.hpp - LoRa Packet Structures
#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "lora_config.h"

// Maximum payload size for LoRa packets
static constexpr size_t MAX_LORA_PAYLOAD = 85;

// Maximum constants
static constexpr size_t MAX_ARGS = 6;                                             // up to 6 arguments in command
static constexpr size_t CRC_SIZE = 2;                                             // 2 bytes CRC
static constexpr size_t HEADER_SIZE = sizeof(uint8_t) * 1 + sizeof(uint16_t) * 2; // = 5

#define LoraAddress uint8_t 
// ═══════════════════════════════════════════════════════════════════════════
// MAIN LORA PACKET STRUCTURE
// ═══════════════════════════════════════════════════════════════════════════
#pragma pack(push, 1)
struct LoRaPacket
{
    LoraAddress_t senderId;
    LoraAddress_t receiverId;
    uint8_t packetType;
    PacketId_t packetId;    // Unified packet ID type
    uint8_t payloadLen = 0;
    uint8_t payload[MAX_LORA_PAYLOAD];

    // Getter functions
    LoraAddress_t getSenderId() const { return senderId; }
    LoraAddress_t getReceiverId() const { return receiverId; }
    uint8_t getPacketType() const { return packetType; }

    // Setter functions
    void setSenderId(LoraAddress_t id) { senderId = id; }
    void setReceiverId(LoraAddress_t id) { receiverId = id; }
};
static_assert(sizeof(LoRaPacket) <= 150, "LoRaPacket too large!");
#pragma pack(pop)

// ═══════════════════════════════════════════════════════════════════════════
// PACKET BASE CLASS
// ═══════════════════════════════════════════════════════════════════════════
class PacketBase
{
public:
    uint8_t packetType;     // 'C','T','I','S','A','G','H'=Heartbeat
    PacketId_t packetId;    // sequential number 0…255 (unified type)
    uint8_t payloadLen = 0; // body length (without header and CRC)
};

// Free function for converting PacketBase to string (safer than member function)
inline String PacketBaseToString(const PacketBase& base) {
    return "PacketBase[type=" + String((char)base.packetType) +
           ", id=" + String(base.packetId) +
           ", payloadLen=" + String(base.payloadLen) + "]";
}

// Helper function to convert LoRaPacket to string
inline String LoRaPacketToStr(const LoRaPacket &pkt)
{
    String s;
    s += "[" + String(pkt.senderId) + "->" + String(pkt.receiverId) + "], T=[" + String((char)pkt.packetType);
    s += "/" + String((int)pkt.packetType) + "], id=" + String(pkt.packetId);
    s += ", plLen=" + String(pkt.payloadLen);
    
    if (pkt.payloadLen > MAX_LORA_PAYLOAD)
    {
        s += ", pl=❌CORRUPTED_LEN=" + String(pkt.payloadLen);
    }
    else if (pkt.payloadLen > 0)
    {
        s += ", pl=";
        for (int i = 0; i < pkt.payloadLen && i < MAX_LORA_PAYLOAD; i++)
        {
            char buf[4];
            snprintf(buf, sizeof(buf), "%02X ", pkt.payload[i]);
            s += buf;
        }
    }

    s += "]";
    return s;
}

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

// Helper function to parse ASA request
inline bool parseAsaRequest(const uint8_t *buf, size_t len, uint8_t &profileIndex)
{
    if (len != sizeof(uint8_t))
        return false;
    profileIndex = *buf;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// PACKET STRUCTURES
// ═══════════════════════════════════════════════════════════════════════════
#pragma pack(push, 1)

// ASA Exchange packet (for ASA requests and confirmations)
struct PacketAsaExchange : public PacketBase {
    uint8_t profileIndex;

    PacketAsaExchange(CommandType type = CMD_REQUEST_ASA) {
        packetType = type;
        packetId = 0;
        payloadLen = sizeof(profileIndex);
        profileIndex = 0;
    }

    void setProfile(uint8_t index) {
        profileIndex = index;
        payloadLen = sizeof(profileIndex);
    }

    uint8_t getProfile() const {
        return profileIndex;
    }
};

// Command packet: cmdId + variable number of arguments
class PacketCommand : public PacketBase
{
public:
    uint8_t cmdId;         // from CommandID
    uint8_t argCount;      // actually used args
    int8_t args[MAX_ARGS]; // each –128…127
    
    PacketCommand() : cmdId(0), argCount(0) {
        packetType = CMD_COMMAND_STRING;
        payloadLen = sizeof(cmdId) + sizeof(argCount) + sizeof(args);
        for(int i = 0; i < MAX_ARGS; i++) args[i] = 0;
    }
};

// Telemetry packet: speed, course, battery level
class PacketTelemetry : public PacketBase
{
public:
    uint8_t speed;     // 0–255
    uint8_t course;    // 0–179 (0–359°/2)
    uint8_t battLevel; // 0–100%
    
    PacketTelemetry() : speed(0), course(0), battLevel(0) {
        packetType = CMD_TELEMETRY_FRAGMENT;
        payloadLen = sizeof(speed) + sizeof(course) + sizeof(battLevel);
    }
};

// Engine info packet: RPM and temperature
class PacketInfoEngine : public PacketBase
{
public:
    int16_t rpm; // 0–20000
    int8_t temp; // –40…85
    
    PacketInfoEngine() : rpm(0), temp(0) {
        packetType = CMD_INFO_ENGINE;
        payloadLen = sizeof(rpm) + sizeof(temp);
    }
};

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

// Status packet: bit flags
class PacketStatus : public PacketBase
{
public:
    uint8_t statusFlags; // bit0=OK,1=LowBatt,2=SensorErr…
    
    PacketStatus() : statusFlags(0) {
        packetType = CMD_STATUS;
        payloadLen = sizeof(statusFlags);
    }
};

// ACK packet
class PacketAck : public PacketBase
{
public:
    PacketId_t ackedId; // packetId being acknowledged (unified type)
    
    PacketAck() : ackedId(0) {
        packetType = CMD_ACK;
        payloadLen = sizeof(ackedId);
    }
};

// Bulk ACK packet - up to 10 ACKs in one packet
class PacketBulkAck : public PacketBase
{
public:
    uint8_t count;                          // number of ACKs (1-10)
    PacketId_t ackedIds[10];                // array of packetIds to acknowledge (unified type)
    
    PacketBulkAck() : count(0) {
        packetType = CMD_BULK_ACK;
        payloadLen = sizeof(count);
        for(int i = 0; i < 10; i++) ackedIds[i] = 0;
    }
    
    bool addAck(PacketId_t packetId) {
        if (count >= 10) return false;
        
        // Check if ID already exists (avoid duplicate ACKs)
        for (uint8_t i = 0; i < count; i++) {
            if (ackedIds[i] == packetId) {
                return true; // Already in list, no need to add
            }
        }
        
        // ID is unique, add it
        ackedIds[count] = packetId;
        count++;
        payloadLen = sizeof(count) + (count * sizeof(PacketId_t));
        return true;
    }
    
    void clear() {
        count = 0;
        payloadLen = sizeof(count);
        for(int i = 0; i < 10; i++) ackedIds[i] = 0;
    }
    
    bool isFull() const { return count >= 10; }
    bool isEmpty() const { return count == 0; }

    // Debug method to display BULK ACK contents
    String getDebugInfo() const {
        String result = "BulkACK(" + String(count) + "): ";
        for (uint8_t i = 0; i < count; i++) {
            if (i > 0) result += ",";
            result += String(ackedIds[i]);
        }
        if (count == 0) result += "empty";
        return result;
    }

    // Check for duplicate IDs (for debugging)
    bool hasDuplicates() const {
        for (uint8_t i = 0; i < count; i++) {
            for (uint8_t j = i + 1; j < count; j++) {
                if (ackedIds[i] == ackedIds[j]) {
                    return true;
                }
            }
        }
        return false;
    }
};

// Config packet (parameter change)
class PacketConfig : public PacketBase
{
public:
    uint8_t paramId;
    int8_t value;
    
    PacketConfig() : paramId(0), value(0) {
        packetType = CMD_CONFIG;
        payloadLen = sizeof(paramId) + sizeof(value);
    }
};

// Navigation packet: GPS
class PacketNav : public PacketBase
{
public:
    int32_t lat;   // 1e-7°
    int32_t lon;   // 1e-7°
    uint16_t hdop; // HDOP×100
    
    PacketNav() : lat(0), lon(0), hdop(0) {
        packetType = CMD_NAV;
        payloadLen = sizeof(lat) + sizeof(lon) + sizeof(hdop);
    }
};

// Heartbeat packet
class PacketHeartbeat : public PacketBase
{
public:
    uint32_t count; // arbitrary counter
    
    PacketHeartbeat() : count(0) {
        packetType = CMD_HEARTBEAT; // or another suitable type for heartbeat
        payloadLen = sizeof(count);
    }
};

// PING packet
class PacketPing : public PacketBase
{
public:
    PacketPing() {
        packetType = CMD_PING;
        payloadLen = 0; // PING contains no data
    }
};

// PONG packet
class PacketPong : public PacketBase
{
public:
    PacketPong() {
        packetType = CMD_PONG;
        payloadLen = 0; // PONG contains no data
    }
};

// Request info packet
class PacketRequestInfo : public PacketBase
{
public:
    uint8_t requestType; // type of requested information
    
    PacketRequestInfo() : requestType(0) {
        packetType = CMD_REQUEST_INFO;
        payloadLen = sizeof(requestType);
    }
};

// Command response packet
class PacketCommandResponse : public PacketBase
{
public:
    PacketCommandResponse() {
        packetType = CMD_COMMAND_RESPONSE;
        payloadLen = 0; // size will be set on send
    }
};

// Telemetry fragment packet (JSON data)
class PacketTelemetryFragment : public PacketBase
{
public:
    PacketTelemetryFragment() {
        packetType = CMD_TELEMETRY_FRAGMENT;
        payloadLen = 0; // size will be set on send
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// AGGREGATED PACKET - Multiple packets in one transmission
// ═══════════════════════════════════════════════════════════════════════════
// Format: [type1:1][len1:1][payload1:len1][type2:1][len2:1][payload2:len2]...

class PacketAggregated : public PacketBase
{
public:
    static constexpr uint8_t MAX_SUB_PACKETS = 5;
    
    PacketAggregated() {
        packetType = CMD_AGR;
        payloadLen = 0;
    }
    
    // Add a sub-packet: returns true if added, false if no space
    bool addSubPacket(uint8_t pktType, const uint8_t* pktPayload, uint8_t pktPayloadLen) {
        // Check if we have space: type(1) + len(1) + payload
        uint8_t requiredSpace = 1 + 1 + pktPayloadLen;
        
        if (payloadLen + requiredSpace > MAX_LORA_PAYLOAD) {
            return false; // Not enough space
        }
        
        payloadLen += requiredSpace;
        return true;
    }
    
    // Serialize into buffer for transmission
    // Returns number of bytes written
    uint8_t serialize(uint8_t* buffer, uint8_t maxLen, 
                      const uint8_t* types, const uint8_t** payloads, 
                      const uint8_t* lens, uint8_t count) const {
        if (!buffer || maxLen == 0 || count == 0) return 0;
        
        uint8_t offset = 0;
        
        for (uint8_t i = 0; i < count; i++) {
            // Write packet type
            if (offset + 1 > maxLen) return 0;
            buffer[offset++] = types[i];
            
            // Write payload length
            if (offset + 1 > maxLen) return 0;
            buffer[offset++] = lens[i];
            
            // Write payload
            if (lens[i] > 0) {
                if (offset + lens[i] > maxLen) return 0;
                if (payloads[i]) {
                    memcpy(buffer + offset, payloads[i], lens[i]);
                }
                offset += lens[i];
            }
        }
        
        return offset;
    }
    
    // Deserialize from buffer
    // Calls callback for each sub-packet found
    // Callback signature: void callback(uint8_t type, const uint8_t* payload, uint8_t len)
    template<typename Callback>
    bool deserialize(const uint8_t* buffer, uint8_t bufferLen, Callback callback) const {
        if (!buffer || bufferLen == 0) return false;
        
        uint8_t offset = 0;
        
        while (offset < bufferLen) {
            // Read packet type
            if (offset + 1 > bufferLen) return false;
            uint8_t type = buffer[offset++];
            
            // Read payload length
            if (offset + 1 > bufferLen) return false;
            uint8_t len = buffer[offset++];
            
            // Validate payload length
            if (len > MAX_LORA_PAYLOAD) return false;
            
            // Read payload
            const uint8_t* payload = nullptr;
            if (len > 0) {
                if (offset + len > bufferLen) return false;
                payload = buffer + offset;
                offset += len;
            }
            
            // Call callback with sub-packet data
            callback(type, payload, len);
        }
        
        return true;
    }
    
    // Calculate how much space is left
    uint8_t getAvailableSpace() const {
        return MAX_LORA_PAYLOAD - payloadLen;
    }
    
    // Check if we can fit a packet of given size
    bool canFit(uint8_t pktPayloadLen) const {
        uint8_t required = 1 + 1 + pktPayloadLen; // type + len + payload
        return (payloadLen + required) <= MAX_LORA_PAYLOAD;
    }
};

#pragma pack(pop)

// Pending send tracking structure
struct PendingSend
{
    LoRaPacket pkt = {}; // Initialize to zero
    uint32_t timestamp;
    uint8_t retries;
};
