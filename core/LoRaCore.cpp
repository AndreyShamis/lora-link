#include "LoRaCore.hpp"

// Static member definitions
TaskHandle_t LoRaCore::receiverTaskHandle = nullptr;
TaskHandle_t LoRaCore::senderTaskHandle = nullptr;

// Helper logging functions (global, not class methods)
void LLog(const char *s)
{
    Serial.println(s);
}

void LLog(const String &s)
{
    LLog(s.c_str());
}

// ═══════════════════════════════════════════════════════════════════════════
// TASK WRAPPERS
// ═══════════════════════════════════════════════════════════════════════════

void LoRaCore::logTaskWrapper(void *param)
{
    static_cast<LoRaCore *>(param)->logTask();
}

void LoRaCore::receiveTaskWrapper(void *param)
{
    static_cast<LoRaCore *>(param)->receiveTask();
}

void LoRaCore::sendTaskWrapper(void *param)
{
    static_cast<LoRaCore *>(param)->sendTask();
}

void LoRaCore::resendTaskWrapper(void *param)
{
    static_cast<LoRaCore *>(param)->resendTask();
}

void LoRaCore::processAsaProfileSwitchWrapper(void *param)
{
    static_cast<LoRaCore *>(param)->processAsaProfileSwitchTask();
}
// ═══════════════════════════════════════════════════════════════════════════
// INITIALIZATION
// ═══════════════════════════════════════════════════════════════════════════

bool LoRaCore::begin()
{
    _loraLong.sf = LORA_SF;
    _loraLong.cr = LORA_CODING_RATE;
    _loraLong.bw = LORA_BANDWIDTH;

    LLog("LoRaCore: Инициализация SPI для LoRa...");
    // Initialize SPI
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
    digitalWrite(LORA_RST, LOW); // Software reset of LoRa module
    delay(5);
    digitalWrite(LORA_RST, HIGH);
    delay(2);

    pinMode(LORA_BUSY, INPUT);
    LLog("LoRaCore: BUSY state before init: " + String(digitalRead(LORA_BUSY)));

    if (!applyLoRa(_loraLong))
    {
        LLog("LoRaCore: Failed to apply LoRa settings");
        return false;
    }

    radio.setDio1Action(onReceive);
    radio.startReceive();

    // Initialize FreeRTOS components
    incomingQueue = xQueueCreate(LORA_INCOMING_QUEUE_SIZE, sizeof(LoRaPacket));
    outgoingQueue = xQueueCreate(LORA_OUTGOING_QUEUE_SIZE, sizeof(LoRaPacket));
    radioSemaphore = xSemaphoreCreateBinary();
    pendingMutex = xSemaphoreCreateMutex();
    asaMutex = xSemaphoreCreateMutex();
    logMutex = xSemaphoreCreateMutex();

    if (!incomingQueue || !outgoingQueue || !radioSemaphore || !pendingMutex || !asaMutex || !logMutex)
    {
        LLog("LoRaCore: Failed to create FreeRTOS objects");
        return false;
    }

    xSemaphoreGive(radioSemaphore);

    // Create tasks
    BaseType_t res1 = xTaskCreatePinnedToCore(receiveTaskWrapper, "LoRaRecv", 6144, this, 3, &receiverTaskHandle, 1);
    BaseType_t res2 = xTaskCreatePinnedToCore(sendTaskWrapper, "LoRaSend", 6144, this, 2, &senderTaskHandle, 1);
    
    BaseType_t res3 = xTaskCreatePinnedToCore(resendTaskWrapper, "LoRaRetry", 4096, this, 1, nullptr, 0);
    BaseType_t res4 = xTaskCreatePinnedToCore(logTaskWrapper, "LoRaLog", 3072, this, 1, nullptr, 0);
    BaseType_t res5 = xTaskCreatePinnedToCore(processAsaProfileSwitchWrapper, "LoRaASA", 4096, this, 1, nullptr, 0);

    if (res1 != pdPASS || res2 != pdPASS || res3 != pdPASS || res4 != pdPASS || res5 != pdPASS)
    {
        LLog("LoRaCore: Failed to create tasks");
        return false;
    }

    LLog("LoRaCore: LoRaCore инициализирован успешно.");
    return true;
}

void LoRaCore::applySettings(int sf, int cr, float bw)
{
    if (radioSemaphore)
        xSemaphoreTake(radioSemaphore, portMAX_DELAY);
    radio.standby();
    radio.setSpreadingFactor(sf);
    radio.setCodingRate(cr);
    radio.setBandwidth(bw);
    currentSF = sf;
    currentCR = cr;
    currentBW = bw;
    updateRetryParameters();
    radio.startReceive();
    if (radioSemaphore)
        xSemaphoreGive(radioSemaphore);

    LLog("LoRaCore: Применены настройки LoRa: SF=" + String(sf) +
         ", CR=" + String(cr) +
         ", BW=" + String(bw, 1) + "kHz");
}

// ═══════════════════════════════════════════════════════════════════════════
// PACKET MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════════


bool LoRaCore::removePendingPacket(PacketId_t packetId)
{
    if (!pendingMutex)
        return false;

    bool removed = false;
    if (xSemaphoreTake(pendingMutex, pdMS_TO_TICKS(200)) == pdTRUE)
    {
        auto it = std::find_if(pending.begin(), pending.end(), [packetId](const PendingSend &p)
                               { return p.pkt.packetId == packetId; });
        if (it != pending.end())
        {
            char s[100];
            snprintf(s, sizeof(s), "🗑️ Manually removed pending packet: id=%u, type=%с", packetId, it->pkt.packetType);
            putToLogBuffer(String(s));

            pending.erase(it);
            removed = true;
        }
        xSemaphoreGive(pendingMutex);
    }
    return removed;
}

int LoRaCore::transmitPacket(const LoRaPacket *txPkt, const size_t len)
{
    xSemaphoreTake(radioSemaphore, portMAX_DELAY);
    radio.standby();
    int result = radio.transmit((uint8_t *)txPkt, len);
    radio.startReceive();
    xSemaphoreGive(radioSemaphore);
    return result;
}

PacketId_t LoRaCore::sendPacketBase(LoraAddress_t receiverId, PacketBase &base, const uint8_t *payload)
{
    if (!outgoingQueue) {
        char s2[100];
        snprintf(s2, sizeof(s2), "⚠️ outgoingQueue is null! Cannot send packet id=%u, type=%c", base.packetId, base.packetType);
        putToLogBuffer(String(s2));
        return 0;
    }
    
    base.packetId = ++nextPacketId;
    
    // === AUTOMATIC AGGREGATION LOGIC ===
    // Try to aggregate if:
    // 1. Not high priority
    // 2. Not ACK/BULK_ACK (they need immediate delivery)
    // 3. Queue has packets waiting
    // 4. Payload is small enough (leaving room for headers)
    bool canAggregate = base.highPriority == false && 
                        base.ackRequired == false &&
                        base.payloadLen <= 30 &&
                        uxQueueMessagesWaiting(outgoingQueue) > 0;
    
    if (canAggregate) {
        // Search for existing AGR packet or regular packet for same receiver
        LoRaPacket searchPkt = {};
        LoRaPacket foundPkt = {};
        bool foundAggregated = false;
        bool foundRegular = false;
        int queueSize = uxQueueMessagesWaiting(outgoingQueue);
        
        // Peek through queue (max 10 packets to avoid delays)
        for (int i = 0; i < queueSize && i < 30; i++) {
            if (xQueuePeek(outgoingQueue, &searchPkt, 0) == pdTRUE) {
                // Check if it's for same receiver
                if (searchPkt.getReceiverId() == receiverId) {
                    if (searchPkt.packetType == CMD_AGR) {
                        // Found existing aggregated packet!
                        PacketAggregated agr;
                        if (agr.canFit(base.payloadLen)) {
                            foundAggregated = true;
                            foundPkt = searchPkt;
                            break;
                        }
                    } else if (!foundRegular && 
                               searchPkt.packetType != CMD_ACK && 
                               searchPkt.packetType != CMD_BULK_ACK &&
                               searchPkt.packetType != CMD_REQUEST_ASA &&
                               searchPkt.packetType != CMD_RESPONCE_ASA &&
                               searchPkt.payloadLen <= 30) {
                        // Found regular packet that can be aggregated
                        foundRegular = true;
                        foundPkt = searchPkt;
                    }
                }
                
                // Remove and re-add to rotate queue for peeking next
                if (xQueueReceive(outgoingQueue, &searchPkt, 0) == pdTRUE) {
                    xQueueSendToFront(outgoingQueue, &searchPkt, 0);
                }
            }
        }
        

        if (foundAggregated) {
            // Add to existing AGR packet
            PacketAggregated agr;
            if (agr.deserialize(foundPkt.payload, foundPkt.payloadLen, 
                               [](uint8_t, const uint8_t*, uint8_t){})) {
                
                // Remove old AGR from queue
                LoRaPacket tempPkt;
                bool removed = false;
                int attempts = queueSize;
                while (attempts-- > 0 && xQueueReceive(outgoingQueue, &tempPkt, 0) == pdTRUE) {
                    if (tempPkt.packetId == foundPkt.packetId && 
                        tempPkt.packetType == CMD_AGR) {
                        removed = true;
                        break;
                    }
                    xQueueSendToBack(outgoingQueue, &tempPkt, 10);
                }
                
                if (removed) {
                    // Create new AGR with additional packet
                    uint8_t types[PacketAggregated::MAX_SUB_PACKETS];
                    const uint8_t* payloads[PacketAggregated::MAX_SUB_PACKETS];
                    uint8_t lens[PacketAggregated::MAX_SUB_PACKETS];
                    uint8_t count = 0;
                    
                    // Extract existing sub-packets
                    agr.deserialize(foundPkt.payload, foundPkt.payloadLen,
                                   [&](uint8_t type, const uint8_t* pl, uint8_t len) {
                                       if (count < PacketAggregated::MAX_SUB_PACKETS) {
                                           types[count] = type;
                                           payloads[count] = pl;
                                           lens[count] = len;
                                           count++;
                                       }
                                   });
                    
                    // Add new packet
                    if (count < PacketAggregated::MAX_SUB_PACKETS) {
                        types[count] = base.packetType;
                        payloads[count] = payload;
                        lens[count] = base.payloadLen;
                        count++;
                        
                        // Serialize new AGR
                        PacketAggregated newAgr;
                        uint8_t agrPayload[MAX_LORA_PAYLOAD];
                        uint8_t agrLen = newAgr.serialize(agrPayload, MAX_LORA_PAYLOAD, 
                                                         types, payloads, lens, count);
                        
                        if (agrLen > 0) {
                            newAgr.payloadLen = agrLen;
                            newAgr.packetId = foundPkt.packetId; // Keep original ID
                            
                            LoRaPacket agrFrame = {};
                            packBaseIntoLoRa(agrFrame, srcAddress, receiverId, newAgr, agrPayload);
                            xQueueSendToBack(outgoingQueue, &agrFrame, 20);
                            
                            char s[120];
                            snprintf(s, sizeof(s), "📦➕ Added to AGR: id=%u, type=%c, count=%u→%u, to=%u", 
                                    base.packetId, base.packetType, count-1, count, receiverId);
                            putToLogBuffer(String(s));
                            
                            return foundPkt.packetId; // Return AGR's ID
                        }
                    }
                }
            }
        } else if (foundRegular) {
            // Create new AGR from two packets
            // Remove found packet from queue
            LoRaPacket tempPkt;
            bool removed = false;
            int attempts = queueSize;
            while (attempts-- > 0 && xQueueReceive(outgoingQueue, &tempPkt, 0) == pdTRUE) {
                if (tempPkt.packetId == foundPkt.packetId) {
                    removed = true;
                    break;
                }
                xQueueSendToBack(outgoingQueue, &tempPkt, 10);
            }
            
            if (removed) {
                PacketAggregated newAgr;
                uint8_t types[2] = {foundPkt.packetType, base.packetType};
                const uint8_t* payloads[2] = {foundPkt.payload, payload};
                uint8_t lens[2] = {foundPkt.payloadLen, base.payloadLen};
                
                uint8_t agrPayload[MAX_LORA_PAYLOAD];
                uint8_t agrLen = newAgr.serialize(agrPayload, MAX_LORA_PAYLOAD, types, payloads, lens, 2);
                
                if (agrLen > 0) {
                    newAgr.payloadLen = agrLen;
                    newAgr.packetId = foundPkt.packetId; // Use first packet's ID
                    
                    LoRaPacket agrFrame = {};
                    packBaseIntoLoRa(agrFrame, srcAddress, receiverId, newAgr, agrPayload);
                    xQueueSendToFront(outgoingQueue, &agrFrame, pdMS_TO_TICKS(10));
                    
                    char s[150];
                    snprintf(s, sizeof(s), "📦✨ Created AGR: id=%u, types=[%c,%c], lens=[%u,%u], to=%u", 
                            foundPkt.packetId, foundPkt.packetType, base.packetType, 
                            foundPkt.payloadLen, base.payloadLen, receiverId);
                    putToLogBuffer(String(s));
                    
                    // Add to pending if waitForAck
                    if (base.ackRequired && pendingMutex && xSemaphoreTake(pendingMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        PendingSend pendingItem = {};
                        pendingItem.pkt = agrFrame;
                        pendingItem.timestamp = millis();
                        pendingItem.retries = 0;
                        pending.push_back(pendingItem);
                        xSemaphoreGive(pendingMutex);
                    }
                    
                    return foundPkt.packetId; // Return AGR's ID (first packet's ID)
                }
            }
        }
    }
    
    // === NORMAL SENDING (no aggregation) ===
    LoRaPacket frame = {};
    packBaseIntoLoRa(frame, srcAddress, receiverId, base, payload);
    
    bool ok;
    if (base.highPriority) {
        ok = xQueueSendToFront(outgoingQueue, &frame, pdMS_TO_TICKS(100)) == pdTRUE;
    } else {
        ok = xQueueSendToBack(outgoingQueue, &frame, pdMS_TO_TICKS(200)) == pdTRUE;
    }
    
    if (ok && base.ackRequired) {
        if (pendingMutex && xSemaphoreTake(pendingMutex, pdMS_TO_TICKS(2100)) == pdTRUE) {
            auto existingIt = std::find_if(pending.begin(), pending.end(),
                                          [&frame](const PendingSend &p)
                                          { return p.pkt.packetId == frame.packetId; });

            if (existingIt != pending.end()) {
                char s[100];
                snprintf(s, sizeof(s), "⚠️ Duplicate packet ID detected: id=%u, type=%c, to=%u", 
                        frame.packetId, frame.packetType, frame.getReceiverId());
                putToLogBuffer(String(s));
                existingIt->timestamp = millis();
                existingIt->retries = 0;
            } else {
                PendingSend pendingItem = {};
                pendingItem.pkt = frame;
                pendingItem.timestamp = millis();
                pendingItem.retries = 0;
                pending.push_back(pendingItem);
            }
            xSemaphoreGive(pendingMutex);
        }
    }
    return base.packetId;
}

void LoRaCore::packBaseIntoLoRa(LoRaPacket &out,
                                LoraAddress_t senderId,
                                LoraAddress_t receiverId,
                                const PacketBase &base,
                                const uint8_t *payload)
{
    memset(&out, 0, sizeof(out));
    out.setSenderId(senderId);
    out.setReceiverId(receiverId);
    out.packetType = base.packetType;
    out.packetId   = base.packetId;

    // --- FLAGS MAPPING ---
    out.setAckRequired(base.ackRequired);
    out.setHighPriority(base.highPriority);
    out.setService(base.service);
    out.setNoRetry(base.noRetry);
    out.setEncrypted(base.encrypted);
    out.setCompressed(base.compressed);
    out.setAggregatedFrame(base.aggregated);
    out.setInternalLocalOnly(base.internalLocalOnly);

    // --- PAYLOAD CHECKS ---
    if (base.payloadLen > MAX_LORA_PAYLOAD) {
        out.payloadLen = 0;
        char msg[100];
        snprintf(msg, sizeof(msg),
                 "❌ packBase: payloadLen %u exceeds MAX %u",
                 base.payloadLen, MAX_LORA_PAYLOAD);
        putToLogBuffer(msg);
        return;
    }

    if (base.payloadLen > 0 && payload == nullptr) {
        out.payloadLen = 0;
        char msg[100];
        snprintf(msg, sizeof(msg),
                 "❌ packBase: payload is null, len=%u",
                 base.payloadLen);
        putToLogBuffer(msg);
        return;
    }

    out.payloadLen = base.payloadLen;

    if (out.payloadLen > 0) {
        memcpy(out.payload, payload, out.payloadLen);
    }

    // String hex = "";
    // for (uint8_t i = 0; i < base.payloadLen && i < 32; ++i)
    // {
    //     char byteStr[4];
    //     snprintf(byteStr, sizeof(byteStr), "%02X ", payload[i]);
    //     hex += byteStr;
    // }

    // if (base.payloadLen > 32)
    // {
    //     hex += "...";
    // }

    // putToLogBuffer(String("📦 Packed id[") + String(out.packetId) + " " + String(senderId) + "->" + String(receiverId) + "] Type[" + String((char)out.packetType) + "/" + String(out.packetType) + "], PL[" + hex + "]:" + String(out.payloadLen));

}

// ═══════════════════════════════════════════════════════════════════════════
// ACK HANDLING
// ═══════════════════════════════════════════════════════════════════════════

void LoRaCore::handleAck(const LoRaPacket &pkt)
{
    if (pkt.payloadLen != sizeof(PacketId_t))
    {
        char s[80];
        snprintf(s, sizeof(s), "❌ Invalid ACK payload len: %u (expected %u)", pkt.payloadLen, sizeof(PacketId_t));
        putToLogBuffer(String(s));
        return;
    }
    PacketId_t ackedId;
    memcpy(&ackedId, pkt.payload, sizeof(ackedId));
    char s[80];
    snprintf(s, sizeof(s), "📩 Single ACK received: id=%u from device %u", ackedId, pkt.getSenderId());
    putToLogBuffer(String(s));

    handleSingleAck(ackedId, pkt.getSenderId(), pkt.packetType);
}

void LoRaCore::handleBulkAck(const LoRaPacket &pkt)
{
    if (pkt.payloadLen < sizeof(uint8_t))
    {
        putToLogBuffer(String("❌ BULK ACK: Invalid payload length"));
        return;
    }

    uint8_t count = pkt.payload[0];

    if (count > 10 || pkt.payloadLen != sizeof(uint8_t) + (count * sizeof(PacketId_t)))
    {
        char errorLog[120];
        snprintf(errorLog, sizeof(errorLog), "❌ BULK ACK: Invalid count=%u or length mismatch", count);
        putToLogBuffer(String(errorLog));
        return;
    }

    PacketId_t ackedIds[10];
    memcpy(ackedIds, &pkt.payload[1], count * sizeof(PacketId_t));
    PacketId_t uniqueIds[10];
    uint8_t uniqueCount = 0;

    for (uint8_t i = 0; i < count; i++) {
        bool isDuplicate = false;
        for (uint8_t j = 0; j < uniqueCount; j++) {
            if (uniqueIds[j] == ackedIds[i]) {
                isDuplicate = true;
                break;
            }
        }
        if (!isDuplicate && uniqueCount < 10) {
            uniqueIds[uniqueCount] = ackedIds[i];
            uniqueCount++;
        }
    }

    String idList = "";
    String uniqueIdList = "";
    for (uint8_t i = 0; i < count; i++) {
        if (i > 0)
            idList += ",";
        idList += String(ackedIds[i]);
    }
    for (uint8_t i = 0; i < uniqueCount; i++) {
        if (i > 0)
            uniqueIdList += ",";
        uniqueIdList += String(uniqueIds[i]);
    }

    char successLog[120];
    if (uniqueCount != count) {
        snprintf(successLog, sizeof(successLog), "📩 BULK ACK: %u IDs [%s] → %u unique [%s] from device %u (filtered %u duplicates)", count, idList.c_str(), uniqueCount, uniqueIdList.c_str(), pkt.getSenderId(), count - uniqueCount);
    } else {
        snprintf(successLog, sizeof(successLog), "📩 BULK ACK received: %u IDs [%s] from device %u", count, idList.c_str(), pkt.getSenderId());
    }
    putToLogBuffer(String(successLog));

    for (uint8_t i = 0; i < uniqueCount; i++) {
        handleSingleAck(uniqueIds[i], pkt.getSenderId(), pkt.packetType);
    }
}

void LoRaCore::handleSingleAck(PacketId_t ackedId, LoraAddress_t senderId, uint8_t packetType)
{
    if (pendingMutex && xSemaphoreTake(pendingMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        char s[100];
        auto it = std::find_if(pending.begin(), pending.end(), [ackedId](const PendingSend &p)
                               { return p.pkt.packetId == ackedId; });
        if (it != pending.end())
        {
            uint8_t originalPacketType = it->pkt.packetType;
            pending.erase(it);
            snprintf(s, sizeof(s), "✅ACK confirmed: id=%u, from=%u, type=%c, origType=%c", ackedId, senderId, packetType, originalPacketType);
            putToLogBuffer(String(s));
            
            if (ackCallback)
            {
                ackCallback(ackedId, senderId, originalPacketType);
                _ack_received++;
                char callbackLog[120];
                snprintf(callbackLog, sizeof(callbackLog), "[TO CHECK]🔔 Calling ACK callback: id=%u, sender=%u, origType=%c", ackedId, senderId, originalPacketType);
                putToLogBuffer(String(callbackLog));
            }
        }
        else
        {
            _duplicated_acks++;
            // snprintf(s, sizeof(s), "[Warnings]⚠️ Duplicate ACK ignored: id=%u from device %u (not in pending)", ackedId, senderId);
            // putToLogBuffer(String(s));
        }
        xSemaphoreGive(pendingMutex);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// BULK ACK MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════════

void LoRaCore::addToBulkAck(PacketId_t packetId, uint8_t targetDeviceId)
{
    char s[100];
    if (pendingBulkAck.addAck(packetId))
    {
        // snprintf(s, sizeof(s), "📦 Added ACK for packet %u to bulk (%u/10)", packetId, pendingBulkAck.count);
        // putToLogBuffer(String(s));

        if (pendingBulkAck.isFull() ||
            (millis() - lastBulkAckTime > BULK_ACK_MAX_WAIT_MS && !pendingBulkAck.isEmpty()))
        {
            sendBulkAck(targetDeviceId);
        }
    }
    else
    {
        sendBulkAck(targetDeviceId);
        if (pendingBulkAck.addAck(packetId))
        {
            // snprintf(s, sizeof(s), "📦 Started new bulk ACK with packet %u", packetId);
            // putToLogBuffer(String(s));
        } else
        {
            snprintf(s, sizeof(s), "❌ Failed to add packet %u to new bulk ACK", packetId);
            putToLogBuffer(String(s));
        }
    }
}

void LoRaCore::sendBulkAck(uint8_t targetDeviceId)
{
    if (pendingBulkAck.isEmpty())
        return;

    if (pendingBulkAck.hasDuplicates())
    {
        putToLogBuffer(String("⚠️ WARNING: BULK ACK contains duplicates: ") + pendingBulkAck.getDebugInfo());
    }

    uint8_t payload[1 + 10 * sizeof(PacketId_t)];
    payload[0] = pendingBulkAck.count;
    memcpy(&payload[1], pendingBulkAck.ackedIds, pendingBulkAck.count * sizeof(PacketId_t));

    size_t payloadSize = sizeof(uint8_t) + (pendingBulkAck.count * sizeof(PacketId_t));

    sendPacketBase(targetDeviceId, pendingBulkAck, payload);

    //putToLogBuffer(String("✅ Sent BULK ACK for ") + String(pendingBulkAck.count) + " packets: " + pendingBulkAck.getDebugInfo());

    pendingBulkAck.clear();
    lastBulkAckTime = millis();
}

void LoRaCore::checkBulkAckTimeout(uint8_t targetDeviceId)
{
    if (!pendingBulkAck.isEmpty() &&
        (millis() - lastBulkAckTime > BULK_ACK_INTERVAL_MS))
    {
        sendBulkAck(targetDeviceId);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// RADIO CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════

void LoRaCore::updateRetryParameters()
{
    
    char s[220];
    if (_mode == RadioMode::LORA) {
        float symbolTime = (1 << currentSF) / (currentBW * 1000);
        float preambleTime = (8 + 4.25f) * symbolTime;

        float payloadSymbols = 8.0f + 
                                std::max(
                                    ceil((8.0f * 50 - 4.0f * currentSF + 28 + 16) / (4.0f * (currentSF - 2))), 
                                    0.0f) 
                                    * currentCR;
        float packetTime = (preambleTime + payloadSymbols * symbolTime) * 1000;

        currentRetryTimeoutMs = std::max(8500U, (uint32_t)(packetTime * 3.5f + 1000));
        if (currentSF <= 7) {
            BULK_ACK_INTERVAL_MS = 1800;
            BULK_ACK_MAX_WAIT_MS = 1200;
            currentMaxRetries = 2;
        } else if (currentSF <= 9) {
            BULK_ACK_INTERVAL_MS = 2500;
            BULK_ACK_MAX_WAIT_MS = 1500;
            currentMaxRetries = 3;
        } else {
            BULK_ACK_INTERVAL_MS = 3000;
            BULK_ACK_MAX_WAIT_MS = 1800;
            currentMaxRetries = 4;
        }

        snprintf(s, sizeof(s), "[LoRa] retry: SF%d → timeout=%ums, retries=%u (pkt≈%.1fms)", currentSF, currentRetryTimeoutMs, currentMaxRetries, packetTime);
        putToLogBuffer(String(s));
    }
    else if (_mode == RadioMode::FSK)
    {
        float packetTime = (50.0f * 8.0f * 1000.0f) / currentBitrate;
        currentRetryTimeoutMs = std::max(1500U, (uint32_t)(packetTime * 2.5f + 600));
        if (currentBitrate >= 19200) {
            currentMaxRetries = 2;
        } else {
            currentMaxRetries = 3;
        }
        BULK_ACK_INTERVAL_MS = 600;
        BULK_ACK_MAX_WAIT_MS = 250;

        snprintf(s, sizeof(s), "[FSK] retry: %ukbps → timeout=%ums, retries=%u (pkt≈%.1fms)",
                 currentBitrate / 1000, currentRetryTimeoutMs, currentMaxRetries, packetTime);
        putToLogBuffer(String(s));
    }
    putToLogBuffer(String("Curent profile=") + getCurrentProfileIndex());
    putToLogBuffer(String("Curent Bitrate   :") + currentBitrate + "bps");
    putToLogBuffer(String("Curent Bandwidth :") + currentBW + "kHz");
    putToLogBuffer(String("Curent Coding Rate:") + currentCR);
    putToLogBuffer(String("Curent Spreading Factor:") + currentSF);
    putToLogBuffer(String("Curent Frequency :") + currentFreq + "mHz");
    putToLogBuffer(String("Curent currentRetryTimeoutMs :") + getCurrentRetryTimeout() + "ms");
    putToLogBuffer(String("Curent currentMaxRetries:") + getCurrentMaxRetries());
    putToLogBuffer(String("Curent currentDeviation:") + currentDeviation);
    putToLogBuffer(String("BULK_ACK_INTERVAL_MS=") + BULK_ACK_INTERVAL_MS + "ms");
    putToLogBuffer(String("BULK_ACK_MAX_WAIT_MS=") + BULK_ACK_MAX_WAIT_MS +"ms");
    putToLogBuffer(String("-----------------------------------"));
}

// ═══════════════════════════════════════════════════════════════════════════
// TASK IMPLEMENTATIONS
// ═══════════════════════════════════════════════════════════════════════════

void LoRaCore::logTask()
{
    while (true) {
        if (logMutex && xSemaphoreTake(logMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
            if (!logBuffer.empty()) {
                std::vector<String> logsToProcess = logBuffer;
                logBuffer.clear();
                xSemaphoreGive(logMutex);
                for (const auto &logEntry : logsToProcess) {
                    Serial.println(logEntry);
                }
            } else {
                xSemaphoreGive(logMutex);
            }
        }
        uint32_t randomDelay = 21 + lc_randomRange(0, 28);
        vTaskDelay(pdMS_TO_TICKS(randomDelay));
    }
}

void LoRaCore::receiveTask()
{
    static char s[200];
    static String fullLog = "";

    while (true) {
        LoRaPacket pkt = {};
        fullLog = "";
        receivingInProgress = true;
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (radioSemaphore && xSemaphoreTake(radioSemaphore, 0) == pdTRUE) {

            unsigned long t0 = millis();
            int len = radio.getPacketLength();

            if (len > 0 && len <= sizeof(LoRaPacket)) {
                if (len < offsetof(LoRaPacket, payload)){
                    radio.startReceive();
                    if (radioSemaphore)
                        xSemaphoreGive(radioSemaphore);
                    receivingInProgress = false;
                    char shortLog[80];
                    _rx_errors++;
                    snprintf(shortLog, sizeof(shortLog), "[ERROR] PACKET TOO SHORT: len=%d < header_size=%d", len, (int)offsetof(LoRaPacket, payload));
                    putToLogBuffer(String(shortLog));
                    continue;
                }

                memset(&pkt, 0, sizeof(pkt));
                
                int16_t crcState = radio.readData((uint8_t *)&pkt, len);
                unsigned long t1 = millis();
                radio.startReceive();

                xSemaphoreGive(radioSemaphore);

                if (pkt.getSenderId() == srcAddress || crcState != RADIOLIB_ERR_NONE){
                    receivingInProgress = false;
                    _rx_errors++;
                    continue;
                }

                String payloadHex = "";
                int safePayloadLen = (pkt.payloadLen > MAX_LORA_PAYLOAD) ? 0 : pkt.payloadLen;
                int maxPayloadToShow = std::min(safePayloadLen, 54);
                for (int i = 0; i < maxPayloadToShow; i++){
                    char hexByte[4];
                    snprintf(hexByte, sizeof(hexByte), "%02X ", pkt.payload[i]);
                    payloadHex += hexByte;
                }
                if (pkt.payloadLen > MAX_LORA_PAYLOAD) { payloadHex = "❌CORRUPTED_LEN=" + String(pkt.payloadLen); }
                else if (pkt.payloadLen > 30) { payloadHex += "...";}

                snprintf(s, sizeof(s), "[P:%d][RX on %d][L:%d]%lums→[%u->%u], T=[%d], id:%u,  state:%d, %u", pending.size(), getCurrentProfileIndex(), len, t1 - t0, pkt.getSenderId(), pkt.getReceiverId(), pkt.packetType, pkt.packetId,crcState, pkt.payloadLen);
                fullLog = String(s) + ":[" + payloadHex + "]";
                putToLogBuffer(fullLog);
                if (pkt.packetType == CMD_ACK && !pkt.isAckRequired()) { handleAck(pkt); } 
                else if(pkt.packetType == CMD_BULK_ACK && !pkt.isAckRequired()) { handleBulkAck(pkt); }
                        // Handle ASA response - DON'T apply yet, wait for ACK to be sent first
                else if (pkt.packetType == CMD_RESPONCE_ASA) { handleAsaResponse(&pkt); }
                // Handle ASA request - respond with ASA response (DON'T switch yet!)
                else if (pkt.packetType == CMD_REQUEST_ASA) { handleAsaRequest(&pkt); }
                else {
                    if (pkt.isAckRequired()) {
                        addAckToBulk(pkt.packetId, pkt.getSenderId());
                        if(pkt.isHighPriority()){ flushBulkAck(pkt.getSenderId()); }
                    }
                    if(pkt.isHighPriority()){ xQueueSendToFront(incomingQueue, &pkt, 10); } 
                    else { xQueueSendToBack(incomingQueue, &pkt, 500); }
                }
            } else {
                _rx_errors++;
                radio.startReceive();
                if (radioSemaphore) { 
                    xSemaphoreGive(radioSemaphore); 
            }

                receivingInProgress = false;
            }
        }
    }
}

void LoRaCore::sendTask()
{
    static char s[200];
    unsigned int send_in_row = 0;
    while (true)
    {
        LoRaPacket pkt = {};
        if (xQueueReceive(outgoingQueue, &pkt, pdMS_TO_TICKS(500)) == pdTRUE){

            LoRaPacket txPkt = pkt;
            ssize_t len = offsetof(LoRaPacket, payload) + pkt.payloadLen;
            unsigned long t0 = millis();
            int result = transmitPacket(&txPkt, len);
            unsigned long txDuration = millis() - t0;

            String payloadHex = "";
            int safePayloadLen = (pkt.payloadLen > MAX_LORA_PAYLOAD) ? 0 : pkt.payloadLen;
            for (int i = 0; i < safePayloadLen; i++){
                char hexByte[4];
                snprintf(hexByte, sizeof(hexByte), "%02X ", pkt.payload[i]);
                payloadHex += hexByte;
            }
            if (pkt.payloadLen > MAX_LORA_PAYLOAD) { payloadHex = "❌CORRUPTED_LEN=" + String(pkt.payloadLen); }

            snprintf(s, sizeof(s), "[TxRxQ:%d/%d P:%d][TX:%d][L:%d]%lums→[%u->%u], T=[%c/%d], id=%u, %u:[", getOutgoingQueueCount(), getIncomingQueueCount(), pending.size(), getCurrentProfileIndex(), (int)len, txDuration, pkt.getSenderId(), pkt.getReceiverId(), pkt.packetType, pkt.packetType, pkt.packetId, pkt.payloadLen);
            putToLogBuffer( String(s) + payloadHex + "]");

            if (result != RADIOLIB_ERR_NONE){
                snprintf(s, sizeof(s),  "[ERROR] TX Error: code=%d, id=%u, len=%d, duration=%lums", result, pkt.packetId, (int)len, txDuration);
                putToLogBuffer(String(s));
                _tx_errors++;
            }

            if (pkt.payloadLen != txPkt.payloadLen){
                char corruptionBuf[50];
                snprintf(s, sizeof(corruptionBuf), "[ERROR] TX Error: CORRUPTION: orig_len=%u, tx_len=%u", pkt.payloadLen, txPkt.payloadLen);
                putToLogBuffer(String(corruptionBuf));
            }
            if (txDuration > 900){
                vTaskDelay(pdMS_TO_TICKS(txDuration * 3.5));
            } else if (txDuration > 600){
                vTaskDelay(pdMS_TO_TICKS(txDuration * 3));
            } else if (txDuration > 300){
                vTaskDelay(pdMS_TO_TICKS(txDuration * 2));
            } else {
                vTaskDelay(pdMS_TO_TICKS((txDuration+2)/2));
            }
            send_in_row++;
            if (send_in_row >= 9) {
                send_in_row = 0;
                vTaskDelay(pdMS_TO_TICKS(15 + lc_randomRange(0, 20)));
            }
            
        } else {
            send_in_row = 0;
            uint32_t randomDelay = lc_randomRange(10, 50);
            if (currentProfileIndex < 4){
                randomDelay += lc_randomRange(10, 20);
                if (currentProfileIndex < 2){
                    randomDelay += lc_randomRange(20, 49);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(randomDelay));
        }
    }
}

void LoRaCore::resendTask()
{
    static char s[120];
    while (true)
    {
        uint32_t now = millis();
        if (pendingMutex && xSemaphoreTake(pendingMutex, pdMS_TO_TICKS(1500)) == pdTRUE){
            for (auto it = pending.begin(); it != pending.end();) {
                if (now - it->timestamp > currentRetryTimeoutMs) {
                    if (it->retries < currentMaxRetries) {
                        if (xQueueSendToBack(outgoingQueue, &it->pkt, pdMS_TO_TICKS(1500)) == pdTRUE) {
                            it->timestamp = now;
                            it->retries++;
                            if (it->retries >= currentMaxRetries - 1) {
                                snprintf(s, sizeof(s), "🔄Retry: id=%u #%u, T=%c, to=%u", it->pkt.packetId, it->retries, it->pkt.packetType, it->pkt.getReceiverId());
                                putToLogBuffer(String(s));
                            }
                            ++it;
                        } else {
                            ++it;
                        }
                    } else {
                        snprintf(s, sizeof(s), "❌Drop: id=%u, T=%c, to=%u (max retries)", it->pkt.packetId, it->pkt.packetType, it->pkt.getReceiverId());
                        putToLogBuffer(String(s));
                        it = pending.erase(it);
                    }
                } else {
                    ++it;
                }
            }
            xSemaphoreGive(pendingMutex);
        }
        uint32_t randomDelay = 211 + lc_randomRange(0, 99);
        vTaskDelay(pdMS_TO_TICKS(randomDelay));
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// PROFILE MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════════

bool LoRaCore::applyProfileFromSettings(uint8_t profileIndex)
{
    if (profileIndex >= LORA_PROFILE_COUNT) {
        LLog("LoRaCore: Недопустимый индекс профиля: " + String(profileIndex));
        return false;
    }

    const auto &profile = loraProfiles[profileIndex];

    if (radioSemaphore && xSemaphoreTake(radioSemaphore, pdMS_TO_TICKS(3000)) == pdTRUE) {
        radio.standby();

        bool stat = false;

        if (profile.mode == RadioProfileMode::LORA) {
            stat =
                radio.setModem(RADIOLIB_MODEM_LORA) == RADIOLIB_ERR_NONE &&
                radio.setFrequency(currentFreq) == RADIOLIB_ERR_NONE &&
                radio.setSpreadingFactor(profile.spreadingFactor) == RADIOLIB_ERR_NONE &&
                radio.setCodingRate(profile.codingRate) == RADIOLIB_ERR_NONE &&
                radio.setBandwidth(profile.bandwidth) == RADIOLIB_ERR_NONE &&
                radio.setPreambleLength(LORA_PREAMBLE_LEN) == RADIOLIB_ERR_NONE &&
                radio.setCRC(true) == RADIOLIB_ERR_NONE &&
                radio.setOutputPower(currentTX) == RADIOLIB_ERR_NONE &&
                radio.setSyncWord(LORA_SYNC_WORD) == RADIOLIB_ERR_NONE;

            if (stat) {
                _mode = RadioMode::LORA;
                currentSF = profile.spreadingFactor;
                currentCR = profile.codingRate;
                currentBW = profile.bandwidth;
                currentProfileIndex = profileIndex;
                updateRetryParameters();
            }
        } else if (profile.mode == RadioProfileMode::FSK) {
            int result;
            radio.standby();
            vTaskDelay(pdMS_TO_TICKS(400));
            
            result = radio.setModem(RADIOLIB_MODEM_FSK);
            if (result != RADIOLIB_ERR_NONE) {
                LLog("LoRaCore: GFSK setModem error: " + String(result));
                xSemaphoreGive(radioSemaphore);
                return false;
            }
            result = radio.setFrequency(currentFreq);
            if (result != RADIOLIB_ERR_NONE) {
                LLog("LoRaCore: GFSK setFrequency error: " + String(result));
                xSemaphoreGive(radioSemaphore);
                return false;
            }
            
            result = radio.setBitRate(profile.bitrate / 1000.0f);
            if (result == RADIOLIB_ERR_NONE) {
                LLog("LoRaCore: GFSK setBitRate успешно, используем классический подход");

                result = radio.setFrequencyDeviation(profile.deviation / 1000.0f);
                if (result != RADIOLIB_ERR_NONE) {
                    LLog("LoRaCore: GFSK setFrequencyDeviation error: " + String(result));
                    xSemaphoreGive(radioSemaphore);
                    return false;
                }

                result = radio.setRxBandwidth(profile.bandwidth);
                if (result != RADIOLIB_ERR_NONE) {
                    LLog("LoRaCore: GFSK setRxBandwidth error: " + String(result) +
                         " (trying to set " + String(profile.bandwidth, 1) + "kHz)");
                    xSemaphoreGive(radioSemaphore);
                    return false;
                }
            } else {
                LLog("LoRaCore: setBitRate не поддерживается, пробуем beginFSK...");

                if (profile.bitrate < 4800) {
                    LLog("LoRaCore: GFSK bitrate " + String(profile.bitrate) + " below SX1262 minimum (4800)");
                    xSemaphoreGive(radioSemaphore);
                    return false;
                }
                
                result = radio.beginFSK(profile.bitrate / 1000.0f, profile.deviation / 1000.0f,
                                        profile.bandwidth, 32, 10.0f, false);
                if (result != RADIOLIB_ERR_NONE) {
                    LLog("LoRaCore: GFSK beginFSK error: " + String(result) +
                         " (bitrate=" + String(profile.bitrate / 1000.0f, 1) + "kbps" +
                         ", dev=" + String(profile.deviation / 1000.0f, 1) + "kHz" +
                         ", rxBw=" + String(profile.bandwidth, 1) + "kHz)");
                    xSemaphoreGive(radioSemaphore);
                    return false;
                }
            }
            
            result = radio.setCRC(true);
            if (result != RADIOLIB_ERR_NONE) {
                LLog("LoRaCore: GFSK setCRC error: " + String(result));
                xSemaphoreGive(radioSemaphore);
                return false;
            } else {
                LLog("LoRaCore: GFSK setCRC успешно");
            }

            stat = true;
            _mode = RadioMode::FSK;
            currentBitrate = profile.bitrate;
            currentDeviation = profile.deviation;
            currentBW = profile.bandwidth;
            currentProfileIndex = profileIndex;
            updateRetryParameters();
        }

        if (stat) { radio.startReceive(); }
        else { LLog("LoRaCore: Ошибка применения профиля " + String(profileIndex)); }

        xSemaphoreGive(radioSemaphore);
        return stat;
    }

    LLog("LoRaCore:failed to get semaphore for profile " + String(profileIndex));
    return false;
}

String LoRaCore::getCurrentProfileInfo() const {
    if (_mode == RadioMode::LORA) {
        return "LoRa #" + String(currentProfileIndex) + ": SF=" + String(currentSF) + ", CR=" + String(currentCR) + ", BW=" + String(currentBW, 1) + "kHz";
    } else {
        return "FSK #" + String(currentProfileIndex) + ": " + String(currentBitrate / 1000.0f, 1) + "kb/s" + ", dev=" + String(currentDeviation / 1000.0f, 1) + "k" + ", bw=" + String(currentBW, 1) + "k";
    }
}


bool LoRaCore::applyLoRa(const LoRaProfile &p) {
    bool success = radio.setModem(RADIOLIB_MODEM_LORA) == RADIOLIB_ERR_NONE &&
                   radio.setFrequency(currentFreq) == RADIOLIB_ERR_NONE &&
                   radio.setSpreadingFactor(p.sf) == RADIOLIB_ERR_NONE &&
                   radio.setCodingRate(p.cr) == RADIOLIB_ERR_NONE &&
                   radio.setBandwidth(p.bw) == RADIOLIB_ERR_NONE &&
                   radio.setPreambleLength(8) == RADIOLIB_ERR_NONE &&
                   radio.setCRC(true) == RADIOLIB_ERR_NONE &&
                   radio.setOutputPower(currentTX) == RADIOLIB_ERR_NONE;

    if (success) {
        currentSF = p.sf;
        currentCR = p.cr;
        currentBW = p.bw;
        updateRetryParameters();
    }
    return success;
}

bool LoRaCore::applyFSK(const FSKProfile &p) {
    int result = radio.beginFSK(p.bitrate / 1000.0f, p.deviation / 1000.0f, p.rxBw / 1000.0f, 4);
    if (result != RADIOLIB_ERR_NONE) {
        return false;
    }
    bool success = radio.setCRC(true) == RADIOLIB_ERR_NONE;
    if (success) {
        currentBitrate = p.bitrate;
        currentDeviation = p.deviation;
        currentBW = p.rxBw;
        updateRetryParameters();
    }
    return success;
}

void LoRaCore::putToLogBuffer(const String &msg) {
    if (logMutex && xSemaphoreTake(logMutex, pdMS_TO_TICKS(2)) == pdTRUE) {
        unsigned long uptime = millis();
        unsigned long seconds = uptime / 1000;
        unsigned long milliseconds = uptime % 1000;
        unsigned long minutes = seconds / 60;
        unsigned long hours = minutes / 60;
        seconds = seconds % 60;
        minutes = minutes % 60;

        char timestamp[32];
        snprintf(timestamp, sizeof(timestamp), "[%02lu:%02lu:%02lu.%03lu] ",  hours, minutes, seconds, milliseconds);
        logBuffer.push_back(String(timestamp) + msg);
        if (logBuffer.size() > MAX_LOG_BUFFER_SIZE) {
            logBuffer.erase(logBuffer.begin());
        }
        xSemaphoreGive(logMutex);
    }
}

bool LoRaCore::switchTo(RadioMode m) {
    if (m == _mode)
        return true;
    bool ok = (m == RadioMode::LORA) ? applyLoRa(_loraLong) : applyFSK(_fskFast);
    if (ok) {
        _mode = m;
        LLog(String("[Warning] LoRaCore: Switched to ") + (m == RadioMode::LORA ? "[LoRa]" : "[GFSK]"));
        radio.startReceive();
    }
    return ok;
}

// -----------------------------------------------------------------------------
PacketId_t LoRaCore::sendAsaRequest(uint8_t profileIndex, LoraAddress_t receiver) {
    PacketAsaExchange pkt(CMD_REQUEST_ASA);
    pkt.setProfile(profileIndex);

    // Create proper payload buffer instead of relying on memory layout
    uint8_t payload[1];
    payload[0] = pkt.profileIndex;

    return sendPacketBase(receiver, pkt, payload);
}

PacketId_t LoRaCore::sendAsaResponse(uint8_t profileIndex, LoraAddress_t receiver) {
    PacketAsaExchange pkt(CMD_RESPONCE_ASA);
    pkt.setProfile(profileIndex);

    // Create proper payload buffer instead of relying on memory layout
    uint8_t payload[1];
    payload[0] = pkt.profileIndex;

    return sendPacketBase(receiver, pkt, payload);
}

// -----------------------------------------------------------------------------
bool LoRaCore::handleAsaRequest(const LoRaPacket *pkt) {
    if (pkt->packetType != CMD_REQUEST_ASA || pkt->payloadLen != 1) {
        return false;
    }
    
    uint8_t requestedProfile = pkt->payload[0];
    LLog("[ASA]request received: profile " + String(requestedProfile) + " from device " + String(pkt->getSenderId()));
    
    if (requestedProfile == getCurrentProfileIndex()) {
        LLog("[ASA]✓ Requested profile " + String(requestedProfile) + " is already active. No switch needed.");
        return true;
    } 
    else if (requestedProfile < LORA_PROFILE_COUNT) {
        // Send ASA response on CURRENT profile
        LLog("[ASA]Sending ASA response for profile " + String(requestedProfile) + " (staying on current profile for now)...");
        sendAsaResponse(requestedProfile, pkt->getSenderId());
        
        // Schedule profile switch after delay (with mutex protection)
        if (asaMutex && xSemaphoreTake(asaMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            pendingAsaProfile = requestedProfile;
            asaResponseSentTime = millis();
            xSemaphoreGive(asaMutex);
            LLog("[ASA]⏳ Will switch to profile " + String(requestedProfile) + " in " + String(ASA_SWITCH_DELAY) + " ms");
            return true;
        } else {
            LLog("[ASA]✗ Failed to acquire asaMutex");
            return false;
        }
    } else {
        LLog("[ASA]✗ Invalid profile index: " + String(requestedProfile) + " (max: " + String(LORA_PROFILE_COUNT-1) + ")");
        return false;
    }
}

// -----------------------------------------------------------------------------
bool LoRaCore::handleAsaResponse(const LoRaPacket* pkt) {
    if (pkt->packetType != CMD_RESPONCE_ASA || pkt->payloadLen != 1) {
        return false;
    }
    
    uint8_t responseProfile = pkt->payload[0];
    LLog("[ASA]response received: profile " + String(responseProfile) + " from device " + String(pkt->getSenderId()));
    
    // Schedule profile switch after delay (to allow ACK to be sent) - with mutex protection
    if (asaMutex && xSemaphoreTake(asaMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        pendingAsaProfile = responseProfile;
        asaResponseReceivedTime = millis();
        xSemaphoreGive(asaMutex);
        LLog("[ASA]⏳ Will switch to profile " + String(responseProfile) + " in " + String(ASA_SWITCH_DELAY) + " ms");
        return true;
    } else {
        LLog("[ASA]✗ Failed to acquire asaMutex");
        return false;
    }
}

// -----------------------------------------------------------------------------
bool LoRaCore::processAsaProfileSwitch() {
    if (!asaMutex) {
        return false;
    }
    
    // Check and read state atomically
    int profileToApply = -1;
    unsigned long currentTime = millis();
    unsigned long lastAsaTime = 0;
    
    if (xSemaphoreTake(asaMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (pendingAsaProfile < 0) {
            xSemaphoreGive(asaMutex);
            return false; // No pending profile switch
        }
        
        // Validate profile index
        if (pendingAsaProfile >= LORA_PROFILE_COUNT) {
            LLog("[ASA]✗ Invalid pending profile index: " + String(pendingAsaProfile) + " (max: " + String(LORA_PROFILE_COUNT-1) + ")");
            pendingAsaProfile = -1; // Clear invalid state
            xSemaphoreGive(asaMutex);
            return false;
        }
        
        // Check if enough time has passed
        lastAsaTime = max(asaResponseSentTime, asaResponseReceivedTime);
        if (currentTime - lastAsaTime <= ASA_SWITCH_DELAY) {
            xSemaphoreGive(asaMutex);
            return false; // Not ready yet
        }
        
        // Ready to switch - save profile and clear state
        profileToApply = pendingAsaProfile;
        pendingAsaProfile = -1;
        asaResponseSentTime = 0;
        asaResponseReceivedTime = 0;
        xSemaphoreGive(asaMutex);
    } else {
        return false; // Could not acquire mutex
    }
    
    // Apply profile switch outside of critical section
    LLog("[ASA]⚡ Switching to ASA profile " + String(profileToApply) + " now...");
    bool success = applyProfileFromSettings(profileToApply);
    
    if (success) {
        LLog("[ASA]✓ ASA profile applied successfully");
        LLog("[ASA]" + getCurrentProfileInfo());
    } else {
        LLog("[ASA]✗ Failed to apply ASA profile");
    }
    
    return success;
}

void LoRaCore::processAsaProfileSwitchTask() {
    while (true){
        vTaskDelay(pdMS_TO_TICKS(200));
        processAsaProfileSwitch();
    }
}