#pragma once

enum class RadioMode : uint8_t
{
    LORA,
    FSK
};

struct LoRaProfile
{
    uint8_t sf{LORA_SF};          // 7‑12
    uint8_t cr{LORA_CODING_RATE}; // 5‑8  (CR4/5..CR4/8)
    float bw{LORA_BANDWIDTH};     // kHz (125/250/500)
};

struct FSKProfile
{
    uint32_t bitrate{38400};   // bit/s
    uint32_t deviation{25000}; // Hz
    uint32_t rxBw{50000};      // Hz (Rx filter BW)
};

// Exponential Moving Average фильтр для RSSI
class RssiFilter {
private:
    float alpha;           // Коэффициент сглаживания (0.0 - 1.0)
    float filteredValue;   // Текущее отфильтрованное значение
    bool initialized;      // Флаг инициализации

public:
    RssiFilter(float smoothingFactor = 0.3f) 
        : alpha(smoothingFactor), filteredValue(0.0f), initialized(false) {}
    
    // Добавить новое значение RSSI и получить отфильтрованное
    float update(float newRssi) {
        if (!initialized) {
            filteredValue = newRssi;
            initialized = true;
        } else {
            // EMA: filtered = alpha * new + (1 - alpha) * filtered
            filteredValue = alpha * newRssi + (1.0f - alpha) * filteredValue;
        }
        return filteredValue;
    }
    
    // Получить текущее отфильтрованное значение
    float get() const { return filteredValue; }
    
    // Проверка инициализации
    bool isInitialized() const { return initialized; }
    
    // Сброс фильтра
    void reset() { initialized = false; }
};

// Информация о клиенте/узле
struct ClientInfo {
    LoraAddress_t address;           // Адрес клиента
    unsigned long lastSeenMs;        // Время последнего контакта (millis)
    uint32_t packetsReceived;        // Количество полученных пакетов
    uint32_t packetsSent;            // Количество отправленных пакетов
    RssiFilter rssiFilter;           // Фильтр для RSSI
    float lastRawRssi;               // Последнее сырое значение RSSI
    float lastSnr;                   // Последнее значение SNR
    bool hasReceivedPackets;         // Флаг: получали ли хоть раз пакет от клиента
    
    ClientInfo() 
        : address(0), lastSeenMs(0), packetsReceived(0), 
          packetsSent(0), rssiFilter(0.3f), lastRawRssi(-200.0f), lastSnr(-200.0f), hasReceivedPackets(false) {}
    
    ClientInfo(LoraAddress_t addr) 
        : address(addr), lastSeenMs(0), packetsReceived(0), 
          packetsSent(0), rssiFilter(0.3f), lastRawRssi(-200.0f), lastSnr(-200.0f), hasReceivedPackets(false) {}
    
    // Обновить информацию о клиенте при получении пакета
    void updateOnReceive(float rssi, float snr) {
        lastSeenMs = millis();
        packetsReceived++;
        lastRawRssi = rssi;
        rssiFilter.update(rssi);
        lastSnr = snr;
        hasReceivedPackets = true;
    }
    
    // Обновить информацию при отправке пакета клиенту
    void updateOnSend() {
        packetsSent++;
    }
    
    // Получить отфильтрованное значение RSSI
    float getFilteredRssi() const { return rssiFilter.get(); }
    
    // Проверка, как давно видели клиента (в миллисекундах)
    unsigned long getTimeSinceLastSeen() const { 
        return millis() - lastSeenMs; 
    }
    
    // Проверка активности (например, видели за последние 30 секунд)
    bool isActive(unsigned long timeoutMs = 30000) const {
        return getTimeSinceLastSeen() < timeoutMs;
    }
};


