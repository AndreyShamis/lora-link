// Universal LoRa Node - Can work as Master, Slave, or Boat
// LoRa-Link Universal Application
#include <Arduino.h>
#include "LoRaCore.hpp"

// Device ID for this node
#ifdef ROLE_MASTER
const uint8_t MY_DEVICE_ID = DEVICE_ID_MASTER;
const uint8_t TARGET_DEVICE_ID = DEVICE_ID_SLAVE;
#else
const uint8_t MY_DEVICE_ID = DEVICE_ID_SLAVE;
const uint8_t TARGET_DEVICE_ID = DEVICE_ID_MASTER;
#endif

// LoRa core instance
LoRaCore* lora = nullptr;

// Statistics
unsigned long packetsReceived = 0;
unsigned long packetsSent = 0;
unsigned long lastStatsTime = 0;
unsigned long lastPingTime = 0;
unsigned long lastHeartbeatTime = 0;
bool autoHeartbeat = true;
unsigned long heartbeatInterval = 30000; // 30 seconds default
uint32_t heartbeatCounter = 0; // Persistent heartbeat counter
PacketId_t lastHeartbeatPacketId = 0; // Track last heartbeat packet ID

// Boat mode settings
bool boatMode = false;
unsigned long lastActivityTime = 0;
const unsigned long BOAT_IDLE_TIMEOUT = 60000; // 1 minute without activity
const unsigned long BOAT_SLEEP_CHECK_INTERVAL = 5000; // Check every 5 seconds
unsigned long lastBoatCheck = 0;
int boatIdleProfile = 0; // Profile 0 - most reliable, lowest power
int boatActiveProfile = 3; // Profile 3 - balanced

void processSerialCommands() {
    if (!Serial.available()) return;
    
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd == "ping") {
        Serial.println("Sending PING...");
        PacketPing ping;
        uint8_t dummy = 0;
        lora->sendPacketBase(TARGET_DEVICE_ID, &ping, &dummy);
        packetsSent++;
        lastPingTime = millis();
        lastActivityTime = millis();
        
    } else if (cmd.startsWith("send ")) {
        String msg = cmd.substring(5);
        Serial.println("Sending: " + msg);
        
        PacketCommand pkt;
        pkt.payloadLen = min((int)msg.length(), (int)MAX_LORA_PAYLOAD);
        
        lora->sendPacketBase(TARGET_DEVICE_ID, &pkt, (const uint8_t*)msg.c_str());
        packetsSent++;
        lastActivityTime = millis();
        
    } else if (cmd.startsWith("profile ")) {
        int profile = cmd.substring(8).toInt();
        if (profile >= 0 && profile < LORA_PROFILE_COUNT) {
            Serial.printf("Switching to profile %d...\n", profile);
            if (lora->applyProfileFromSettings(profile)) {
                Serial.println("✓ Profile switched successfully");
                Serial.println(lora->getCurrentProfileInfo());
            } else {
                Serial.println("✗ Failed to switch profile");
            }
        } else {
            Serial.printf("Invalid profile. Use 0-%d\n", LORA_PROFILE_COUNT-1);
        }
        
    } else if (cmd == "profiles") {
        Serial.println("\n=== Available Profiles ===");
        for (int i = 0; i < LORA_PROFILE_COUNT; i++) {
            const auto& p = loraProfiles[i];
            if (p.mode == RadioProfileMode::LORA) {
                Serial.printf("%d: LoRa SF%d CR4/%d BW%.1f kHz\n", 
                             i, p.spreadingFactor, p.codingRate, p.bandwidth);
            } else {
                Serial.printf("%d: FSK %lu bps Dev:%lu Hz BW:%.1f kHz\n", 
                             i, p.bitrate, p.deviation, p.bandwidth);
            }
        }
        Serial.printf("\nCurrent: %d - %s\n", 
                     lora->getCurrentProfileIndex(),
                     lora->getCurrentProfileInfo().c_str());
        Serial.println("==========================\n");
        
    } else if (cmd == "stats") {
        Serial.println("\n=== Statistics ===");
        Serial.printf("Packets sent: %lu\n", packetsSent);
        Serial.printf("Packets received: %lu\n", packetsReceived);
        Serial.printf("Uptime: %lu min\n", millis() / 60000);
        Serial.printf("Current profile: %d - %s\n", 
                     lora->getCurrentProfileIndex(),
                     lora->getCurrentProfileInfo().c_str());
        Serial.println(lora->getQueueStatus());
        Serial.println(lora->getAdaptiveRetryInfo());
        Serial.println(lora->getPendingPacketsInfo());
        Serial.println("==================\n");
        
    } else if (cmd == "rssi") {
        float rssi = lora->getRadio().getRSSI();
        float snr = lora->getRadio().getSNR();
        Serial.printf("RSSI: %.1f dBm\n", rssi);
        Serial.printf("SNR: %.1f dB\n", snr);
        Serial.printf("Frequency: %.3f MHz\n", LORA_FREQUENCY);
        
    } else if (cmd == "status") {
        Serial.println("\n=== System Status ===");
        Serial.printf("Device ID: %d\n", MY_DEVICE_ID);
        Serial.printf("Target ID: %d\n", TARGET_DEVICE_ID);
        Serial.printf("Mode: %s\n", lora->mode() == RadioMode::LORA ? "LoRa" : "FSK");
        Serial.printf("Manual mode: %s\n", lora->isManualMode() ? "Yes" : "No");
        Serial.printf("Boat mode: %s\n", boatMode ? "ENABLED" : "DISABLED");
        if (boatMode) {
            unsigned long idleTime = millis() - lastActivityTime;
            Serial.printf("Idle time: %lu s\n", idleTime / 1000);
            Serial.printf("Boat profiles: idle=%d, active=%d\n", boatIdleProfile, boatActiveProfile);
        }
        Serial.println(lora->getCurrentProfileInfo());
        Serial.println(lora->getQueueStatus());
        float rssi = lora->getRadio().getRSSI();
        float snr = lora->getRadio().getSNR();
        Serial.printf("RSSI: %.1f dBm, SNR: %.1f dB\n", rssi, snr);
        Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
        Serial.printf("Uptime: %lu min\n", millis() / 60000);
        Serial.printf("Auto heartbeat: %s (interval: %lu ms)\n", 
                     autoHeartbeat ? "ON" : "OFF", heartbeatInterval);
        Serial.println("=====================\n");
        
    } else if (cmd == "queue") {
        Serial.println("\n=== Queue Status ===");
        Serial.println(lora->getQueueStatus());
        Serial.println(lora->getPendingPacketsInfo());
        Serial.println("====================\n");
        
    } else if (cmd == "clear") {
        Serial.println("Clearing pending packets...");
        lora->clearPending();
        Serial.println("✓ Pending queue cleared");
        
    } else if (cmd == "reset") {
        Serial.println("Resetting statistics...");
        packetsSent = 0;
        packetsReceived = 0;
        lastStatsTime = millis();
        Serial.println("✓ Statistics reset");
        
    } else if (cmd == "reboot") {
        Serial.println("Rebooting...");
        delay(100);
        ESP.restart();
        
    } else if (cmd == "lora") {
        Serial.println("Switching to LoRa mode...");
        lora->forceMode(RadioMode::LORA);
        Serial.println("✓ LoRa mode active");
        
    } else if (cmd == "fsk") {
        Serial.println("Switching to FSK mode...");
        lora->forceMode(RadioMode::FSK);
        Serial.println("✓ FSK mode active");
        
    } else if (cmd == "auto") {
        Serial.println("Clearing manual mode...");
        lora->clearManualMode();
        Serial.println("✓ Automatic mode active");
        
    } else if (cmd == "heartbeat on") {
        autoHeartbeat = true;
        lastHeartbeatTime = millis();
        Serial.printf("✓ Auto heartbeat enabled (interval: %lu ms)\n", heartbeatInterval);
        
    } else if (cmd == "heartbeat off") {
        autoHeartbeat = false;
        Serial.println("✓ Auto heartbeat disabled");
        
    } else if (cmd.startsWith("heartbeat ")) {
        String arg = cmd.substring(10);
        if (arg.startsWith("interval ")) {
            unsigned long interval = arg.substring(9).toInt();
            if (interval >= 1000 && interval <= 300000) {
                heartbeatInterval = interval;
                Serial.printf("✓ Heartbeat interval set to %lu ms\n", heartbeatInterval);
            } else {
                Serial.println("✗ Invalid interval. Use 1000-300000 ms");
            }
        } else {
            Serial.println("Usage: heartbeat on|off|interval <ms>");
        }
        
    } else if (cmd == "boat on") {
        boatMode = true;
        lastActivityTime = millis();
        Serial.println("✓ Boat mode ENABLED");
        Serial.printf("  Idle profile: %d (power saving)\n", boatIdleProfile);
        Serial.printf("  Active profile: %d (normal operation)\n", boatActiveProfile);
        Serial.printf("  Idle timeout: %lu s\n", BOAT_IDLE_TIMEOUT / 1000);
        
    } else if (cmd == "boat off") {
        boatMode = false;
        Serial.println("✓ Boat mode DISABLED");
        
    } else if (cmd.startsWith("boat idle ")) {
        int profile = cmd.substring(10).toInt();
        if (profile >= 0 && profile < LORA_PROFILE_COUNT) {
            boatIdleProfile = profile;
            Serial.printf("✓ Boat idle profile set to %d\n", profile);
        } else {
            Serial.printf("✗ Invalid profile. Use 0-%d\n", LORA_PROFILE_COUNT-1);
        }
        
    } else if (cmd.startsWith("boat active ")) {
        int profile = cmd.substring(12).toInt();
        if (profile >= 0 && profile < LORA_PROFILE_COUNT) {
            boatActiveProfile = profile;
            Serial.printf("✓ Boat active profile set to %d\n", profile);
        } else {
            Serial.printf("✗ Invalid profile. Use 0-%d\n", LORA_PROFILE_COUNT-1);
        }
        
    } else if (cmd == "info") {
        Serial.println("\n=== Device Info ===");
        Serial.printf("Chip: %s\n", ESP.getChipModel());
        Serial.printf("Cores: %d\n", ESP.getChipCores());
        Serial.printf("CPU Freq: %d MHz\n", ESP.getCpuFreqMHz());
        Serial.printf("Flash: %d KB\n", ESP.getFlashChipSize() / 1024);
        Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
        Serial.printf("SDK: %s\n", ESP.getSdkVersion());
        Serial.println("===================\n");
        
    } else if (cmd == "log") {
        Serial.println("\n=== Log Buffer ===");
        Serial.printf("Log entries: %d\n", lora->getLogBufferSize());
        Serial.println("==================\n");
    } else if (cmd == "request info") {
        Serial.println("Requesting info...");
        PacketRequestInfo pkt;
        pkt.packetType = CMD_REQUEST_INFO;
        pkt.payloadLen = 0;
        lora->sendPacketBase(TARGET_DEVICE_ID, &pkt, nullptr);
        packetsSent++;
        lastActivityTime = millis();
        
    } else if (cmd.startsWith("asa ")) {
        int profileIndex = cmd.substring(4).toInt();
        if (profileIndex >= 0 && profileIndex < LORA_PROFILE_COUNT) {
            Serial.printf("Sending ASA request for profile %d...\n", profileIndex);
            lora->sendAsaRequest(profileIndex, TARGET_DEVICE_ID);
            packetsSent++;
            lastActivityTime = millis();
        } else {
            Serial.printf("✗ Invalid profile. Use 0-%d\n", LORA_PROFILE_COUNT-1);
        }
        
    } else if (cmd.startsWith("setid ")) {
        int newId = cmd.substring(6).toInt();
        if (newId >= 0 && newId <= 255) {
            lora->setSrcAddress(newId);
            Serial.printf("✓ Device ID set to %d\n", newId);
        } else {
            Serial.println("✗ Invalid device ID. Use 0-255");
        }
        
    } else if (cmd.startsWith("settarget ")) {
        int newTarget = cmd.substring(10).toInt();
        if (newTarget >= 0 && newTarget <= 255) {
            lora->setDstAddress(newTarget);
            Serial.printf("✓ Target ID set to %d\n", newTarget);
        } else {
            Serial.println("✗ Invalid target ID. Use 0-255");
        }
        
    } else if (cmd == "help") {
        Serial.println("\n╔════════════════════════════════════════════╗");
        Serial.println("║        LoRa-Link Command Reference        ║");
        Serial.println("╠════════════════════════════════════════════╣");
        Serial.println("║ COMMUNICATION                              ║");
        Serial.println("║  ping              Send PING               ║");
        Serial.println("║  send <text>       Send text message       ║");
        Serial.println("║  request status    Request status          ║");
        Serial.println("║  request info      Request info            ║");
        Serial.println("║  asa <0-12>        ASA profile request     ║");
        Serial.println("║                                            ║");
        Serial.println("║ CONFIGURATION                              ║");
        Serial.println("║  profile <0-12>    Switch to profile       ║");
        Serial.println("║  profiles          List all profiles       ║");
        Serial.println("║  lora              Force LoRa mode         ║");
        Serial.println("║  fsk               Force FSK mode          ║");
        Serial.println("║  auto              Auto mode selection     ║");
        Serial.println("║                                            ║");
        Serial.println("║ MONITORING                                 ║");
        Serial.println("║  stats             Show statistics         ║");
        Serial.println("║  status            Show system status      ║");
        Serial.println("║  rssi              Show RSSI/SNR/freq      ║");
        Serial.println("║  queue             Show queue status       ║");
        Serial.println("║  log               Show log buffer info    ║");
        Serial.println("║  info              Show device info        ║");
        Serial.println("║                                            ║");
        Serial.println("║ HEARTBEAT                                  ║");
        Serial.println("║  heartbeat on      Enable auto heartbeat   ║");
        Serial.println("║  heartbeat off     Disable auto heartbeat  ║");
        Serial.println("║  heartbeat interval <ms>  Set interval     ║");
        Serial.println("║                                            ║");
        Serial.println("║ BOAT MODE                                  ║");
        Serial.println("║  boat on           Enable boat mode        ║");
        Serial.println("║  boat off          Disable boat mode       ║");
        Serial.println("║  boat idle <0-12>  Set idle profile        ║");
        Serial.println("║  boat active <0-12> Set active profile     ║");
        Serial.println("║                                            ║");
        Serial.println("║ SYSTEM                                     ║");
        Serial.println("║  setid <0-255>     Set device ID           ║");
        Serial.println("║  settarget <0-255> Set target ID           ║");
        Serial.println("║  clear             Clear pending packets   ║");
        Serial.println("║  reset             Reset statistics        ║");
        Serial.println("║  reboot            Reboot device           ║");
        Serial.println("║  help              Show this help          ║");
        Serial.println("╚════════════════════════════════════════════╝\n");
        
    } else {
        Serial.println("["+cmd+"]Unknown command. Type 'help' for commands.");
    }
}

void processIncomingPackets() {
    LoRaPacket pkt;
    while (lora->receive(pkt)) {
        packetsReceived++;
        lastActivityTime = millis(); // Reset activity timer on any incoming packet
        
        // Serial.printf("[RX] From %d, Type='%c', ID=%d, Len=%d\n",
        //              pkt.getSenderId(),
        //              pkt.packetType,
        //              pkt.packetId,
        //              pkt.payloadLen);
        
        // Handle PING - auto respond with PONG
        if (pkt.packetType == CMD_PING) {
            Serial.println("PING received, sending PONG...");
            PacketPong pong;
            uint8_t dummy = 0;
            lora->sendPacketBase(pkt.getSenderId(), &pong, &dummy);
            packetsSent++;
        }
        
        // Handle PONG
        else if (pkt.packetType == CMD_PONG) {
            unsigned long rtt = millis() - lastPingTime;
            Serial.printf("PONG received! RTT: %lu ms\n", rtt);
        }
        
        // Handle text messages - echo back
        else if (pkt.packetType == CMD_COMMAND_STRING && pkt.payloadLen > 0) {
            String msg = String((char*)pkt.payload, pkt.payloadLen);
            Serial.println("Message: " + msg);
            
            // Echo back with prefix
            String echo = "Echo: " + msg;
            PacketBase echoPkt;
            echoPkt.packetType = CMD_COMMAND_STRING;
            echoPkt.payloadLen = min((int)echo.length(), (int)MAX_LORA_PAYLOAD);
            lora->sendPacketBase(pkt.getSenderId(), &echoPkt, (const uint8_t*)echo.c_str());
            packetsSent++;
        }
    }
}

void processBoatMode() {
    if (!boatMode) return;
    
    // Check boat mode state periodically
    if (millis() - lastBoatCheck < BOAT_SLEEP_CHECK_INTERVAL) return;
    lastBoatCheck = millis();
    
    unsigned long idleTime = millis() - lastActivityTime;
    int currentProfile = lora->getCurrentProfileIndex();
    
    // Switch to idle profile if no activity
    if (idleTime > BOAT_IDLE_TIMEOUT && currentProfile != boatIdleProfile) {
        Serial.printf("[BOAT] Switching to idle profile %d (no activity for %lu s)\n", 
                     boatIdleProfile, idleTime / 1000);
        lora->applyProfileFromSettings(boatIdleProfile);
    }
    // Switch back to active profile on activity
    else if (idleTime < BOAT_IDLE_TIMEOUT && currentProfile == boatIdleProfile) {
        Serial.printf("[BOAT] Switching to active profile %d (activity detected)\n", 
                     boatActiveProfile);
        lora->applyProfileFromSettings(boatActiveProfile);
    }
}

void setup() {
    Serial.begin(921600);
    delay(1000);
    
    Serial.println("\n\n╔════════════════════════════════════════════╗");
    Serial.println("║         LoRa-Link Universal Node         ║");
    Serial.println("╚════════════════════════════════════════════╝");
#ifdef ROLE_MASTER
    Serial.println("Role: MASTER (Mission Control)");
#else
    Serial.println("Role: SLAVE (Remote Device)");
#endif
    Serial.println("Device ID: " + String(MY_DEVICE_ID));
    Serial.println("Target ID: " + String(TARGET_DEVICE_ID));
    Serial.println("============================================\n");
    
    // Initialize LoRa
    lora = new LoRaCore(MY_DEVICE_ID, TARGET_DEVICE_ID);
    
    if (!lora->begin()) {
        Serial.println("ERROR: Failed to initialize LoRa!");
        while(1) delay(1000);
    }
    
    Serial.println("✓ LoRa initialized successfully");
    Serial.println(lora->getCurrentProfileInfo());
    Serial.println("\nReady! Type 'help' for commands.\n");
    
    lastStatsTime = millis();
    lastHeartbeatTime = millis();
    lastActivityTime = millis();
    lastBoatCheck = millis();
}

void loop() {
    // Process serial commands
    processSerialCommands();
    
    // Process incoming packets
    processIncomingPackets();
    
    // Check bulk ACK timeout
    lora->processBulkAckTimeout(TARGET_DEVICE_ID);
    
    // Process pending ASA profile switch
    lora->processAsaProfileSwitch();
    
    // Process boat mode (adaptive power management)
    processBoatMode();
    
    // Auto heartbeat
    if (autoHeartbeat && (millis() - lastHeartbeatTime > heartbeatInterval)) {
        // Check if previous heartbeat is still pending
        bool previousHeartbeatPending = false;
        if (lastHeartbeatPacketId > 0) {
            previousHeartbeatPending = lora->isPacketPending(lastHeartbeatPacketId);
        }
        
        // Only send new heartbeat if previous one is not pending
        if (!previousHeartbeatPending) {
            PacketHeartbeat hb;
            hb.count = heartbeatCounter++; // Increment counter for each heartbeat
            lastHeartbeatPacketId = lora->sendBroadcast(&hb, (uint8_t*)&hb.count); // Broadcast heartbeat!
            lastHeartbeatTime = millis();
            
#ifdef ROLE_SLAVE
            Serial.printf("[♥️ HB-BC] Heartbeat broadcast #%lu (TX: %lu, RX: %lu)\r\n", hb.count, packetsSent, packetsReceived);
#endif
        } else {
#ifdef ROLE_SLAVE
            Serial.printf("[♥️ HB] Skipping heartbeat - previous one still pending (ID: %u)\r\n", lastHeartbeatPacketId);
#endif
        }
    }
    
    // Periodic statistics
    if (millis() - lastStatsTime > 300000) { // Every 5 minutes
        lastStatsTime = millis();
        Serial.printf("[INFO] Uptime: %lu min, TX: %lu, RX: %lu, Heap: %d bytes\r\n",
                     millis() / 60000, packetsSent, packetsReceived, ESP.getFreeHeap());
        if (boatMode) {
            unsigned long idleTime = millis() - lastActivityTime;
            Serial.printf("[BOAT] Mode: %s, Idle: %lu s\r\n", 
                         boatMode ? "ACTIVE" : "OFF", idleTime / 1000);
        }
    }
    
    delay(1);
}
