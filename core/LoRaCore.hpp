#pragma once
// LoRaCore.hpp - Unified LoRa Communication System
#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>
#include <vector>
#include <functional>
#include "lora_config.h"
#include "lora_helpers.hpp"
#include "lora_packets.hpp"

// Forward declarations for logging functions
void LLog(const char *s);
void LLog(const String &s);

class LoRaCore
{
private:
    LoraAddress_t srcAddress;     // Current source address (can be changed)
    LoraAddress_t dstAddress;     // Default destination address (can be changed)
    Module *_module;
    SX1262 radio;
    QueueHandle_t incomingQueue = nullptr;
    QueueHandle_t outgoingQueue = nullptr;
    SemaphoreHandle_t radioSemaphore = nullptr;
    static TaskHandle_t receiverTaskHandle;
    std::vector<PendingSend> pending;
    SemaphoreHandle_t pendingMutex = nullptr;                                // Мьютекс для pending vector
    std::function<void(PacketId_t, LoraAddress_t, uint8_t)> ackCallback = nullptr; // callback(packetId, senderId, packetType)
    std::vector<String> logBuffer;
    SemaphoreHandle_t logMutex = nullptr;
    static const size_t MAX_LOG_BUFFER_SIZE = 30;

    // Флаг активного приема - блокирует передачу
    volatile bool receivingInProgress = false;
    uint8_t currentMaxRetries = 4;
    uint32_t currentRetryTimeoutMs = 3200;
    unsigned long BULK_ACK_INTERVAL_MS = 600;
    unsigned long BULK_ACK_MAX_WAIT_MS = 250;

    static const uint32_t FAST_TX_THRESHOLD_MS = 100;  // Быстрая передача < 100мс
    static const uint32_t SLOW_TX_THRESHOLD_MS = 1000; // Медленная передача > 1сек
    static const uint32_t QUEUE_FULL_RETRY_MS = 200;   // Повтор при заполненной очереди
    RadioMode _mode{RadioMode::LORA};
    bool _manual{false};
    LoRaProfile _loraLong;
    FSKProfile _fskFast;
    int currentSF{LORA_SF};
    int currentCR{LORA_CODING_RATE};
    float currentBW{LORA_BANDWIDTH};
    float currentFreq{LORA_FREQUENCY};
    int8_t currentTX{LORA_TX_POWER};
    uint32_t currentBitrate{0};
    uint32_t currentDeviation{0};
    uint8_t currentProfileIndex{0};
    static constexpr float RSSI_ENTER_FSK = -85.0f;
    static constexpr float RSSI_LEAVE_FSK = -92.0f;
    static constexpr float SNR_ENTER_FSK = 8.0f;
    PacketId_t nextPacketId{0}; // Централизованный счетчик ID пакетов

    // Система агрегированных ACK
    PacketBulkAck pendingBulkAck;
    unsigned long lastBulkAckTime = 0;

    int _rx_errors = 0;
    int _tx_errors = 0;
    int _duplicated_acks = 0;
    int _ack_received = 0;
    int _last_rssi = -200;
    int _last_snr = -200;
public:
    int getRxErrorCount() const { return _rx_errors; }
    int getDuplicatedAcksCount() const { return _duplicated_acks; }
    int getAckReceivedCount() const { return _ack_received; }
    int getTxErrorCount() const { return _tx_errors; }
    int getLastRssi() const { return _last_rssi; }
    int getLastSnr() const { return _last_snr; }

    void putToLogBuffer(const String &msg);
    bool applyProfileFromSettings(uint8_t profileIndex);
    String getCurrentProfileInfo() const;
    uint8_t getCurrentProfileIndex() const { return currentProfileIndex; }
    uint8_t getCurrentMaxRetries() const { return currentMaxRetries; }
    uint32_t getCurrentRetryTimeout() const { return currentRetryTimeoutMs; }
    bool begin();
    void applySettings(int sf, int cr, float bw);
    void clearManualMode() { _manual = false;}
    bool isManualMode() const { return _manual;}
    RadioMode mode() const { return _mode;}
    SX1262 &getRadio() { return radio;}
    bool removePendingPacket(PacketId_t );  // Метод для принудительного удаления пакета из pending списка (например, при успешном ASA ответе)
    PacketId_t sendPacketBase(LoraAddress_t receiverId, PacketBase &base, const uint8_t *payload, bool waitForAck = true, bool highPriority=false);

    LoRaCore(LoraAddress_t deviceId, LoraAddress_t defaultDestination = 0)
        : srcAddress(deviceId),
          dstAddress(defaultDestination),
          _module(new Module(LORA_SS, LORA_DIO1, LORA_RST, LORA_BUSY)),
          radio(_module)
    {}

    ~LoRaCore()
    {
        if (incomingQueue){
            vQueueDelete(incomingQueue);
        }
        if (outgoingQueue){
            vQueueDelete(outgoingQueue);
        }
        if (radioSemaphore){
            vSemaphoreDelete(radioSemaphore);
        }
        if (pendingMutex){
            vSemaphoreDelete(pendingMutex);
        }
        if (logMutex){
            vSemaphoreDelete(logMutex);
        }
        radio.clearDio1Action();
        delete _module;
    }

    static void onReceive()
    {
        // STATIC INTERRUPT HANDLER
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(receiverTaskHandle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }

    void forceMode(RadioMode m) {
        _manual = true;
        switchTo(m);
    }

    void setAckCallback(std::function<void(PacketId_t, LoraAddress_t, uint8_t)> &&callback){
        ackCallback = std::move(callback);
    }

    void clearAckCallback()
    {
        ackCallback = nullptr;
    }


    // Низкоуровневая отправка без ACK и повторов (для служебных пакетов)
    bool send(const LoRaPacket &pkt)
    {
        if (!outgoingQueue)
            return false;
        return xQueueSendToBack(outgoingQueue, &pkt, 0) == pdTRUE;
    }

    bool receive(LoRaPacket &pkt)
    {
        if (!incomingQueue)
            return false;
        return xQueueReceive(incomingQueue, &pkt, 0) == pdTRUE;
    }

    // DIAGNOSTIC AND MAINTENANCE METHODS

    // Получить текущий ID (без инкремента) - только для диагностики
    PacketId_t getCurrentPacketId() const
    {
        return nextPacketId;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // BULK ACK MANAGEMENT
    // ═══════════════════════════════════════════════════════════════════════════

    // Добавить ACK в bulk пакет (публичный интерфейс)
    void addAckToBulk(PacketId_t packetId, uint8_t targetDeviceId)
    {
        addToBulkAck(packetId, targetDeviceId);
    }

    // Принудительно отправить накопленные ACK
    void flushBulkAck(uint8_t targetDeviceId)
    {
        sendBulkAck(targetDeviceId);
    }

    // Проверить таймаут bulk ACK (вызывать в главном цикле)
    void processBulkAckTimeout(uint8_t targetDeviceId)
    {
        checkBulkAckTimeout(targetDeviceId);
    }

    size_t getPendingCount() const
    {
        if (!pendingMutex)
            return 0;
        size_t count = 0;
        if (xSemaphoreTake(pendingMutex, pdMS_TO_TICKS(5)) == pdTRUE)
        {
            count = pending.size();
            xSemaphoreGive(pendingMutex);
        }
        return count;
    }

    bool isPacketPending(PacketId_t packetId) const
    {
        if (!pendingMutex)
            return false;
        bool isPending = false;
        if (xSemaphoreTake(pendingMutex, pdMS_TO_TICKS(5)) == pdTRUE)
        {
            for (const auto& p : pending)
            {
                if (p.pkt.packetId == packetId)
                {
                    isPending = true;
                    break;
                }
            }
            xSemaphoreGive(pendingMutex);
        }
        return isPending;
    }

    void clearPending()
    {
        if (pendingMutex && xSemaphoreTake(pendingMutex, pdMS_TO_TICKS(1100)) == pdTRUE)
        {
            pending.clear();
            xSemaphoreGive(pendingMutex);
        }
    }

    size_t getLogBufferSize() const
    {
        if (!logMutex)
            return 0;
        size_t count = 0;
        if (xSemaphoreTake(logMutex, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            count = logBuffer.size();
            xSemaphoreGive(logMutex);
        }
        return count;
    }

    void clearLogBuffer()
    {
        if (logMutex && xSemaphoreTake(logMutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            logBuffer.clear();
            xSemaphoreGive(logMutex);
        }
    }

    bool isHealthy() const
    {
        return incomingQueue != nullptr && outgoingQueue != nullptr && radioSemaphore != nullptr && pendingMutex != nullptr;
    }

    // Диагностические методы для мониторинга очередей
    size_t getIncomingQueueCount() const
    {
        return incomingQueue ? uxQueueMessagesWaiting(incomingQueue) : 0;
    }

    size_t getOutgoingQueueCount() const
    {
        return outgoingQueue ? uxQueueMessagesWaiting(outgoingQueue) : 0;
    }

    size_t getIncomingQueueFree() const
    {
        return incomingQueue ? uxQueueSpacesAvailable(incomingQueue) : 0;
    }

    size_t getOutgoingQueueFree() const
    {
        return outgoingQueue ? uxQueueSpacesAvailable(outgoingQueue) : 0;
    }

    String getQueueStatus() const
    {
        return "TX:" + String(getOutgoingQueueCount()) + "/" + String(getOutgoingQueueCount() + getOutgoingQueueFree()) + ", RX:" + String(getIncomingQueueCount()) + "/" + String(getIncomingQueueCount() + getIncomingQueueFree()) + ", Pending:" + String(getPendingCount());
    }



    String getAdaptiveRetryInfo() const
    {
        return "Retries:" + String(currentMaxRetries) + ", Timeout:" + String(currentRetryTimeoutMs) + "ms" + " (" + ((_mode == RadioMode::LORA) ? "LoRa" : "FSK") + ")";
    }

    // Диагностический метод для отображения текущих pending пакетов
    String getPendingPacketsInfo() const
    {
        if (!pendingMutex)
            return "No pending mutex";

        String result = "";
        if (xSemaphoreTake(pendingMutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            result = "Pending(" + String(pending.size()) + "): ";
            for (size_t i = 0; i < pending.size(); i++)
            {
                if (i > 0)
                    result += ", ";
                result += String(pending[i].pkt.packetId) + "#" + String(pending[i].retries);
            }
            if (pending.empty())
            {
                result += "empty";
            }
            xSemaphoreGive(pendingMutex);
        }
        else
        {
            result = "Pending: mutex timeout";
        }
        return result;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // ADDRESS MANAGEMENT
    // ═══════════════════════════════════════════════════════════════════════════
    
    // Get current source address
    uint8_t getSrcAddress() const { return srcAddress; }
    
    // Get default destination address
    uint8_t getDstAddress() const { return dstAddress; }
    
    // Set source address (who we are)
    void setSrcAddress(LoraAddress_t addr) { 
        srcAddress = addr;
        LLog("Source address set to: " + String(addr));
    }
    
    // Set default destination address (who we talk to by default)
    void setDstAddress(LoraAddress_t addr) { 
        dstAddress = addr;
        LLog("Destination address set to: " + String(addr));
    }
    
    // Send with default destination
    PacketId_t sendPacket(PacketBase &base, const uint8_t *payload, bool waitForAck = true) {
        return sendPacketBase(dstAddress, base, payload, waitForAck);
    }



private:
    // Task wrappers - implemented in .cpp
    static void logTaskWrapper(void *param);
    static void receiveTaskWrapper(void *param);
    static void sendTaskWrapper(void *param);
    static void resendTaskWrapper(void *param);

    void updateRetryParameters();
    bool applyLoRa(const LoRaProfile &p);
    bool applyFSK(const FSKProfile &p);
    bool switchTo(RadioMode m);
    // ACK handling methods
    void handleAck(const LoRaPacket &pkt);
    void handleBulkAck(const LoRaPacket &pkt);
    void handleSingleAck(PacketId_t ackedId, LoraAddress_t senderId, uint8_t packetType);
    
    // Bulk ACK methods
    void addToBulkAck(PacketId_t packetId, uint8_t targetDeviceId);
    void sendBulkAck(uint8_t targetDeviceId);
    void checkBulkAckTimeout(uint8_t targetDeviceId);

    // Packet packing
    void packBaseIntoLoRa(LoRaPacket &out, LoraAddress_t senderId, LoraAddress_t receiverId, const PacketBase &base, const uint8_t *payload);
    
    // Task implementations
    void logTask();
    void receiveTask();
    void sendTask();
    void resendTask();
};

// ═══════════════════════════════════════════════════════════════════════════
// INLINE METHODS FOR PROFILE MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════════

// -----------------------------------------------------------------------------
// Утилиты ASA: отправка через экземпляр LoRaCore
// -----------------------------------------------------------------------------

inline PacketId_t sendAsaRequest(LoRaCore *loraCore, uint8_t profileIndex, LoraAddress_t receiver)
{
    if (!loraCore)
        return 0;
    PacketAsaExchange pkt(CMD_REQUEST_ASA);
    pkt.setProfile(profileIndex);

    // Create proper payload buffer instead of relying on memory layout
    uint8_t payload[1];
    payload[0] = pkt.profileIndex;

    return loraCore->sendPacketBase(receiver, pkt, payload, true, true);
}

inline PacketId_t sendAsaResponse(LoRaCore *loraCore, uint8_t profileIndex, LoraAddress_t receiver)
{
    if (!loraCore)
        return 0;
    PacketAsaExchange pkt(CMD_REPOSNCE_ASA);
    pkt.setProfile(profileIndex);

    // Create proper payload buffer instead of relying on memory layout
    uint8_t payload[1];
    payload[0] = pkt.profileIndex;

    return loraCore->sendPacketBase(receiver, pkt, payload, true, true);
}

// // Пример разбора входящего ASA пакета
// inline bool parseAsaPacket(const PacketBase &hdr, const uint8_t *buf, uint8_t &outProfileIndex)
// {
//     if (hdr.payloadLen != sizeof(uint8_t))
//         return false;
//     outProfileIndex = buf[0];
//     return true;
// }
