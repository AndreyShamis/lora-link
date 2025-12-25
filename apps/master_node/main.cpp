// Master Node - Mission Control
// LoRa-Link Example Application
#include <Arduino.h>
#include "LoRaCore.hpp"
#include <vector>

// Device ID for this node
const uint8_t MY_DEVICE_ID = DEVICE_ID_MASTER;
const uint8_t TARGET_DEVICE_ID = DEVICE_ID_SLAVE;

// LoRa core instance
LoRaCore* lora = nullptr;

// Statistics
unsigned long packetsReceived = 0;
unsigned long packetsSent = 0;
unsigned long lastStatsTime = 0;
unsigned long lastPingTime = 0;
unsigned long lastHeartbeatTime = 0;
bool autoHeartbeat = false;
unsigned long heartbeatInterval = 5000; // 5 seconds default
uint32_t heartbeatCounter = 0; // Persistent heartbeat counter
PacketId_t lastHeartbeatPacketId = 0; // Track last heartbeat packet ID

// Buffered logging system
std::vector<String> logBuffer;
unsigned long lastLogFlushTime = 0;
const unsigned long LOG_FLUSH_INTERVAL = 100; // Flush every 100ms

void log(const String& message) {
    logBuffer.push_back(message);
}

void logf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    logBuffer.push_back(String(buffer));
}

void flushLogs() {
    if (logBuffer.empty()) return;
    
    for (const auto& msg : logBuffer) {
        Serial.println(msg);
    }
    logBuffer.clear();
    lastLogFlushTime = millis();
}

// Serial command buffer
static String serialBuffer = "";

// Forward declaration
void processCommand(const String& cmd, const String& cmd_lower);

void processSerialCommands() {
    // Read available characters and echo them
    while (Serial.available()) {
        char c = Serial.read();
        
        // Echo the character back to terminal
        if (c >= 32 && c <= 126) { // Printable characters
            Serial.print(c);
        } else if (c == '\r' || c == '\n') {
            Serial.println(); // Echo newline
        }
        
        // Process command on Enter
        if (c == '\n' || c == '\r') {
            if (serialBuffer.length() > 0) {
                String cmd = serialBuffer;
                serialBuffer = ""; // Clear buffer
                
                cmd.trim();
                String cmd_lower = cmd;
                cmd_lower.toLowerCase();
                
                // Process the command
                processCommand(cmd, cmd_lower);
            }
            return;
        }
        
        // Handle backspace
        if (c == '\b' || c == 127) {
            if (serialBuffer.length() > 0) {
                serialBuffer.remove(serialBuffer.length() - 1);
                Serial.print("\b \b"); // Erase character on screen
            }
            continue;
        }
        
        // Add to buffer
        if (c >= 32 && c <= 126) { // Only printable characters
            serialBuffer += c;
        }
    }
}

void processCommand(const String& cmd, const String& cmd_lower) {
    
    if (cmd == "ping") {
        log("Sending PING...");
        PacketPing ping;
        uint8_t dummy = 0;
        lora->sendPacketBase(TARGET_DEVICE_ID, &ping, &dummy);
        packetsSent++;
        lastPingTime = millis();
        
    } else if (cmd_lower.startsWith("send ")) {
        String msg = cmd.substring(5);
        log("Sending: " + msg);
        
        PacketCommand pkt;
        pkt.payloadLen = min((int)msg.length(), (int)MAX_LORA_PAYLOAD);
        
        lora->sendPacketBase(TARGET_DEVICE_ID, &pkt, (const uint8_t*)msg.c_str());
        packetsSent++;
        
    } else if (cmd_lower.startsWith("profile ") || cmd_lower.startsWith("prof ")) {
        int profile = cmd.substring(8).toInt();
        if (profile >= 0 && profile < LORA_PROFILE_COUNT) {
            logf("Switching to profile %d...", profile);
            if (lora->applyProfileFromSettings(profile)) {
                log("✓ Profile switched successfully");
                log(lora->getCurrentProfileInfo());
            } else {
                log("✗ Failed to switch profile");
            }
        } else {
            logf("Invalid profile. Use 0-%d", LORA_PROFILE_COUNT-1);
        }
        
    } else if (cmd_lower == "profiles" || cmd_lower == "profs") {
        log("\n=== Available Profiles ===");
        for (int i = 0; i < LORA_PROFILE_COUNT; i++) {
            const auto& p = loraProfiles[i];
            if (p.mode == RadioProfileMode::LORA) {
                logf("%d: LoRa SF%d CR4/%d BW%.1f kHz", 
                             i, p.spreadingFactor, p.codingRate, p.bandwidth);
            } else {
                logf("%d: FSK %lu bps Dev:%lu Hz BW:%.1f kHz", 
                             i, p.bitrate, p.deviation, p.bandwidth);
            }
        }
        logf("\nCurrent: %d - %s", 
                     lora->getCurrentProfileIndex(),
                     lora->getCurrentProfileInfo().c_str());
        log("==========================\n");
        
    } else if (cmd_lower == "stats") {
        log("\n=== Statistics ===");
        logf("Packets sent: %lu", packetsSent);
        logf("Packets received: %lu", packetsReceived);
        logf("Uptime: %lu min", millis() / 60000);
        logf("Current profile: %d - %s", 
                     lora->getCurrentProfileIndex(),
                     lora->getCurrentProfileInfo().c_str());
        log(lora->getQueueStatus());
        log(lora->getAdaptiveRetryInfo());
        log(lora->getPendingPacketsInfo());
        log("==================\n");
        
    } else if (cmd_lower == "rssi") {
        float rssi = lora->getRadio().getRSSI();
        float snr = lora->getRadio().getSNR();
        logf("RSSI: %.1f dBm", rssi);
        logf("SNR: %.1f dB", snr);
        logf("Frequency: %.3f MHz", LORA_FREQUENCY);
        
    } else if (cmd_lower == "status") {
        log("\n=== System Status ===");
        logf("Device ID: %d", MY_DEVICE_ID);
        logf("Target ID: %d", TARGET_DEVICE_ID);
        logf("Mode: %s", lora->mode() == RadioMode::LORA ? "LoRa" : "FSK");
        logf("Manual mode: %s", lora->isManualMode() ? "Yes" : "No");
        log(lora->getCurrentProfileInfo());
        log(lora->getQueueStatus());
        float rssi = lora->getRadio().getRSSI();
        float snr = lora->getRadio().getSNR();
        logf("RSSI: %.1f dBm, SNR: %.1f dB", rssi, snr);
        logf("Free heap: %d bytes", ESP.getFreeHeap());
        logf("Uptime: %lu min", millis() / 60000);
        logf("Auto heartbeat: %s (interval: %lu ms)", 
                     autoHeartbeat ? "ON" : "OFF", heartbeatInterval);
        log("=====================\n");
        
    } else if (cmd_lower == "queue") {
        log("\n=== Queue Status ===");
        log(lora->getQueueStatus());
        log(lora->getPendingPacketsInfo());
        log("====================\n");
        
    } else if (cmd_lower == "clear") {
        log("Clearing pending packets...");
        lora->clearPending();
        log("✓ Pending queue cleared");
        
    } else if (cmd_lower == "reset") {
        log("Resetting statistics...");
        packetsSent = 0;
        packetsReceived = 0;
        lastStatsTime = millis();
        log("✓ Statistics reset");
        
    } else if (cmd_lower == "reboot") {
        log("Rebooting...");
        flushLogs();
        delay(100);
        ESP.restart();
        
    } else if (cmd_lower == "lora") {
        log("Switching to LoRa mode...");
        lora->forceMode(RadioMode::LORA);
        log("✓ LoRa mode active");
        
    } else if (cmd_lower == "fsk") {
        log("Switching to FSK mode...");
        lora->forceMode(RadioMode::FSK);
        log("✓ FSK mode active");
        
    } else if (cmd_lower == "auto") {
        log("Clearing manual mode...");
        lora->clearManualMode();
        log("✓ Automatic mode active");
        
    } else if (cmd_lower == "heartbeat on") {
        autoHeartbeat = true;
        lastHeartbeatTime = millis();
        logf("✓ Auto heartbeat enabled (interval: %lu ms)", heartbeatInterval);
        
    } else if ( cmd_lower == "heartbeat off") {
        autoHeartbeat = false;
        log("✓ Auto heartbeat disabled");
        
    } else if (cmd_lower.startsWith("heartbeat ")|| cmd_lower.startsWith("hb ")) {
        String arg = cmd.substring(10);
        if (arg.startsWith("interval ") || arg.startsWith("i ")) {
            unsigned long interval = arg.substring(9).toInt();
            if (interval >= 100 && interval <= 60000) {
                heartbeatInterval = interval;
                logf("✓ Heartbeat interval set to %lu ms", heartbeatInterval);
            } else {
                log("✗ Invalid interval. Use 100-60000 ms");
            }
        } else {
            log("Usage: \r\nheartbeat on|off|interval <ms>");
            log("Usage: \r\nheartbeat interval 1000");
        }
        
    } else if (cmd_lower == "info") {
        log("\n=== Device Info ===");
        logf("Chip: %s", ESP.getChipModel());
        logf("Cores: %d", ESP.getChipCores());
        logf("CPU Freq: %d MHz", ESP.getCpuFreqMHz());
        logf("Flash: %d KB", ESP.getFlashChipSize() / 1024);
        logf("Free heap: %d bytes", ESP.getFreeHeap());
        logf("SDK: %s", ESP.getSdkVersion());
        log("===================\n");
        
    } else if (cmd_lower == "log") {
        log("\n=== Log Buffer ===");
        logf("Log entries: %d", lora->getLogBufferSize());
        log("==================\n");
        
    } else if (cmd_lower == "clients") {
        log("\n=== Connected Clients ===");
        logf("Total clients: %d", lora->getClientsCount());
        
        auto clients = lora->getAllClients();
        if (clients.empty()) {
            log("No clients found.");
        } else {
            log("\nAddr | LastSeen  | RX | TX | RSSI(flt) | SNR   | Raw RSSI | Status");
            log("-----|-----------|----|----|-----------|-------|----------|--------");
            for (const auto& client : clients) {
                char buf[120];
                
                if (!client.hasReceivedPackets) {
                    // Клиент никогда не отправлял нам пакеты - только TX
                    snprintf(buf, sizeof(buf), " %3u |   Never   | %4u | %4u |    N/A    |  N/A  |   N/A    | TX only",
                        client.address,
                        client.packetsReceived,
                        client.packetsSent);
                } else {
                    // Нормальная статистика с RSSI/SNR
                    unsigned long timeSince = client.getTimeSinceLastSeen();
                    char timeStr[12];
                    
                    if (timeSince > 3600000) {
                        snprintf(timeStr, sizeof(timeStr), "%luh", timeSince / 3600000);
                    } else if (timeSince > 60000) {
                        snprintf(timeStr, sizeof(timeStr), "%lumin", timeSince / 60000);
                    } else if (timeSince > 1000) {
                        snprintf(timeStr, sizeof(timeStr), "%lus", timeSince / 1000);
                    } else {
                        snprintf(timeStr, sizeof(timeStr), "%lums", timeSince);
                    }
                    
                    const char* status = client.isActive(30000) ? "Active" : "Idle";
                    
                    snprintf(buf, sizeof(buf), " %3u | %9s | %4u | %4u | %6.1f | %5.1f | %7.1f | %s",
                        client.address,
                        timeStr,
                        client.packetsReceived,
                        client.packetsSent,
                        client.getFilteredRssi(),
                        client.lastSnr,
                        client.lastRawRssi,
                        status);
                }
                log(buf);
            }
        }
        log("======================================================================\n");
        
    } else if (cmd_lower == "request info") {
        log("Requesting info from slave...");
        PacketRequestInfo pkt;
        pkt.payloadLen = 0;
        lora->sendPacketBase(TARGET_DEVICE_ID, &pkt, nullptr);
        packetsSent++;
        
    } else if (cmd_lower.startsWith("asa ")) {
        int profileIndex = cmd.substring(4).toInt();
        if (profileIndex >= 0 && profileIndex < LORA_PROFILE_COUNT) {
            logf("Sending ASA request for profile %d...", profileIndex);
            lora->sendAsaRequest(profileIndex, TARGET_DEVICE_ID);
            packetsSent++;
        } else {
            logf("✗ Invalid profile. Use 0-%d", LORA_PROFILE_COUNT-1);
        }
        
    } else if (cmd_lower == "autoasa on") {
        lora->setAutoAsaEnabled(true);
        log("✓ Auto-ASA enabled");
        
    } else if (cmd_lower == "autoasa off") {
        lora->setAutoAsaEnabled(false);
        log("✓ Auto-ASA disabled");
        
    } else if (cmd_lower.startsWith("autoasa interval ")) {
        int interval = cmd.substring(17).toInt();
        if (interval >= 1000 && interval <= 300000) {
            lora->setAutoAsaCheckInterval(interval);
            logf("✓ Auto-ASA interval set to %d ms", interval);
        } else {
            log("✗ Invalid interval. Use 1000-300000 ms");
        }
        
    } else if (cmd_lower.startsWith("autoasa hysteresis ")) {
        float hysteresis = cmd.substring(19).toFloat();
        if (hysteresis >= 0.5f && hysteresis <= 10.0f) {
            lora->setAutoAsaRssiHysteresis(hysteresis);
            logf("✓ Auto-ASA hysteresis set to %.1f dBm", hysteresis);
        } else {
            log("✗ Invalid hysteresis. Use 0.5-10.0 dBm");
        }
        
    } else if (cmd_lower == "autoasa status") {
        log("\n=== Auto-ASA Status ===");
        logf("Enabled: %s", lora->isAutoAsaEnabled() ? "Yes" : "No");
        logf("Check interval: %lu ms", lora->getAutoAsaCheckInterval());
        logf("RSSI hysteresis: %.1f dBm", lora->getAutoAsaRssiHysteresis());
        log("=======================\n");
        
    } else if (cmd_lower.startsWith("setid ")) {
        int newId = cmd.substring(6).toInt();
        if (newId >= 0 && newId <= 255) {
            lora->setSrcAddress(newId);
            logf("✓ Device ID set to %d", newId);
        } else {
            log("✗ Invalid device ID. Use 0-255");
        }
        
    } else if (cmd_lower.startsWith("settarget ")) {
        int newTarget = cmd.substring(10).toInt();
        if (newTarget >= 0 && newTarget <= 255) {
            lora->setDstAddress(newTarget);
            logf("✓ Target ID set to %d", newTarget);
        } else {
            log("✗ Invalid target ID. Use 0-255");
        }
        
    } else if (cmd_lower == "help") {
        // Help is printed directly to avoid buffer overflow

        Serial.println("╠════════════════════════════════════════════╣");
        Serial.println("║ COMMUNICATION                              ║");
        Serial.println("║  ping              Send PING to slave      ║");
        Serial.println("║  send <text>       Send text message       ║");
        Serial.println("║  request info      Request slave info      ║");
        Serial.println("║  asa <0-12>        ASA profile request     ║");
        Serial.println("║                                            ║");
        Serial.println("║ CONFIGURATION                              ║");
        Serial.println("║  profile <0-12>    Switch to profile       ║");
        Serial.println("║  profiles          List all profiles       ║");
        Serial.println("║  !lora              Force LoRa mode         ║");
        Serial.println("║  !fsk               Force FSK mode          ║");
        Serial.println("║  !auto              Auto mode selection     ║");
        Serial.println("║                                            ║");
        Serial.println("║ AUTO-ASA (Adaptive Profile Selection)     ║");
        Serial.println("║  autoasa on        Enable auto-ASA         ║");
        Serial.println("║  autoasa off       Disable auto-ASA        ║");
        Serial.println("║  autoasa status    Show auto-ASA info      ║");
        Serial.println("║  autoasa interval <ms>  Set check interval ║");
        Serial.println("║  autoasa hysteresis <dBm> Set hysteresis   ║");
        Serial.println("║                                            ║");
        Serial.println("║ MONITORING                                 ║");
        Serial.println("║  stats             Show statistics         ║");
        Serial.println("║  status            Show system status      ║");
        Serial.println("║  rssi              Show RSSI/SNR/freq      ║");
        Serial.println("║  queue             Show queue status       ║");
        Serial.println("║  clients           Show client info        ║");
        Serial.println("║  log               Show log buffer info    ║");
        Serial.println("║  info              Show device info        ║");
        Serial.println("║                                            ║");
        Serial.println("║ HEARTBEAT                                  ║");
        Serial.println("║  heartbeat on      Enable auto heartbeat   ║");
        Serial.println("║  heartbeat off     Disable auto heartbeat  ║");
        Serial.println("║  heartbeat interval 3000<ms>  Set interval ║");
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
        logf("[%s]Unknown command. Type 'help' for commands.", cmd.c_str());
    }
}

void processIncomingPackets() {
    LoRaPacket pkt;
    while (lora->receive(pkt)) {
        packetsReceived++;
        
        // logf("[RX] From %d, Type='%c', ID=%d, Len=%d",
        //              pkt.getSenderId(),
        //              pkt.packetType,
        //              pkt.packetId,
        //              pkt.payloadLen);
        
        // Handle PONG
        if (pkt.packetType == CMD_PONG) {
            unsigned long rtt = millis() - lastPingTime;
            logf("PONG received! RTT: %lu ms", rtt);
        }
        
        // Handle text messages
        else if (pkt.packetType == CMD_COMMAND_STRING && pkt.payloadLen > 0) {
            String msg = String((char*)pkt.payload, pkt.payloadLen);
            log("Message: " + msg);
        }
    }
}

void setup() {
    Serial.begin(921600);
    delay(1000);
    
    Serial.println("\n\n=== LoRa-Link Master Node ===");
    Serial.println("Mission Control Station");
    Serial.println("Device ID: " + String(MY_DEVICE_ID));
    Serial.println("============================\n");
    
    // Initialize LoRa
    lora = new LoRaCore(MY_DEVICE_ID, TARGET_DEVICE_ID);
    
    if (!lora->begin()) {
        Serial.println("ERROR: Failed to initialize LoRa!");
        while(1) delay(1000);
    }
    
    Serial.println("LoRa initialized successfully");
    Serial.println(lora->getCurrentProfileInfo());
    Serial.println("\nReady! Type 'help' for commands.\n");
    
    lastStatsTime = millis();
}

void loop() {
    // Flush buffered logs periodically
    if (millis() - lastLogFlushTime >= LOG_FLUSH_INTERVAL) {
        flushLogs();
    }
    
    // Process serial commands
    processSerialCommands();
    
    // Process incoming packets
    processIncomingPackets();
    
    // Check bulk ACK timeout
    lora->processBulkAckTimeout(TARGET_DEVICE_ID);
    
    // Process pending ASA profile switch
    //lora->processAsaProfileSwitch();
    
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
            lastHeartbeatPacketId = lora->sendPacketBase(TARGET_DEVICE_ID, &hb, (uint8_t*)&hb.count); // No ACK for heartbeat
            lastHeartbeatTime = millis();
            logf("[HB] Heartbeat sent #%lu", hb.count);
        } else {
            logf("[HB] Skipping heartbeat - previous one still pending (ID: %u)", lastHeartbeatPacketId);
        }
    }
    
    // Periodic statistics
    if (millis() - lastStatsTime > 60000) {
        lastStatsTime = millis();
        logf("[INFO] Uptime: %lu min, TX: %lu, RX: %lu",
             millis() / 60000, packetsSent, packetsReceived);
    }
    
    delay(1);
}
