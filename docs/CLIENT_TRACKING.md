# Client Information Tracking

## Обзор

Система отслеживания клиентов автоматически собирает информацию о каждом узле в сети LoRa, включая:
- Время последнего контакта
- Количество принятых/отправленных пакетов
- Фильтрованное значение RSSI (без скачков)
- Последние значения RSSI и SNR

## Структура ClientInfo

```cpp
struct ClientInfo {
    LoraAddress_t address;           // Адрес клиента
    unsigned long lastSeenMs;        // Время последнего контакта (millis)
    uint32_t packetsReceived;        // Количество полученных пакетов
    uint32_t packetsSent;            // Количество отправленных пакетов
    RssiFilter rssiFilter;           // Фильтр для RSSI
    float lastRawRssi;               // Последнее сырое значение RSSI
    float lastSnr;                   // Последнее значение SNR
    bool hasReceivedPackets;         // Флаг: получали ли хоть раз пакет от клиента
};
```

**Важно**: 
- `lastSeenMs` обновляется **только** при получении пакета от клиента (RX)
- RSSI/SNR доступны **только** если `hasReceivedPackets == true`
- Если вы только отправляете пакеты клиенту (TX), но не получаете ответы - RSSI/SNR будут N/A

## Фильтр RSSI

Для сглаживания значений RSSI используется **Exponential Moving Average (EMA)** фильтр:

```cpp
filtered = alpha * new_value + (1 - alpha) * filtered
```

- **alpha = 0.3**: Коэффициент сглаживания (больше = быстрее реакция на изменения)
- Устраняет скачки и шум в измерениях RSSI
- Дает более стабильное представление качества связи

### Пример работы фильтра

```
Сырые значения RSSI: -78, -82, -79, -80, -85, -81
Отфильтрованные:     -78, -79.2, -79.14, -79.4, -81.08, -81.06
```

## API методы

### Обновление информации

```cpp
// Автоматически вызывается при получении пакета
void updateClientOnReceive(LoraAddress_t address, float rssi, float snr);

// Автоматически вызывается при отправке пакета
void updateClientOnSend(LoraAddress_t address);
```

### Получение информации

```cpp
// Получить информацию о конкретном клиенте
bool getClientInfo(LoraAddress_t address, ClientInfo &info);

// Получить количество известных клиентов
size_t getClientsCount() const;

// Получить список всех клиентов
std::vector<ClientInfo> getAllClients();

// Очистить неактивных клиентов (не видели дольше timeoutMs)
void cleanupInactiveClients(unsigned long timeoutMs = 60000);
```

### Методы ClientInfo

```cpp
// Получить отфильтрованное значение RSSI
float getFilteredRssi() const;

// Время с последнего контакта (мс)
unsigned long getTimeSinceLastSeen() const;

// Проверка активности клиента
bool isActive(unsigned long timeoutMs = 30000) const;
```

## Использование в приложении

### Пример: Отображение всех клиентов

```cpp
void showClients() {
    Serial.printf("Total clients: %d\n", lora->getClientsCount());
    
    auto clients = lora->getAllClients();
    for (const auto& client : clients) {
        Serial.printf("Client %u: RSSI=%.1f dBm (filtered=%.1f), "
                     "SNR=%.1f, RX=%u, TX=%u, LastSeen=%lu ms ago\n",
                     client.address,
                     client.lastRawRssi,
                     client.getFilteredRssi(),
                     client.lastSnr,
                     client.packetsReceived,
                     client.packetsSent,
                     client.getTimeSinceLastSeen());
    }
}
```

### Пример: Проверка качества связи

```cpp
void checkLinkQuality(LoraAddress_t targetId) {
    ClientInfo info;
    if (lora->getClientInfo(targetId, info)) {
        float rssi = info.getFilteredRssi();
        
        if (rssi > -70) {
            Serial.println("Отличный сигнал!");
        } else if (rssi > -85) {
            Serial.println("Хороший сигнал");
        } else if (rssi > -95) {
            Serial.println("Слабый сигнал");
        } else {
            Serial.println("Очень слабый сигнал");
        }
        
        if (!info.isActive(10000)) {
            Serial.println("Внимание: клиент давно не отвечает!");
        }
    }
}
```

### Пример: Периодическая очистка

```cpp
void loop() {
    static unsigned long lastCleanup = 0;
    
    // Очищать неактивных клиентов каждые 60 секунд
    if (millis() - lastCleanup > 60000) {
        lora->cleanupInactiveClients(60000); // Удалить клиентов, которых не видели 60+ секунд
        lastCleanup = millis();
    }
}
```

## Команда "clients" в Master Node

В приложении `master_node` добавлена команда `clients` для просмотра информации:

```
> clients

=== Connected Clients ===
Total clients: 2

Addr | LastSeen  | RX | TX | RSSI(flt) | SNR   | Raw RSSI | Status
-----|-----------|----|----|-----------|-------|----------|--------
  10 |      2s   | 156|  89|   -78.5   |  9.2  |   -79.0  | Active
   2 |   Never   |   0| 56 |    N/A    |  N/A  |   N/A    | TX only
======================================================================
```

**Пояснение статусов**:
- **Active**: Клиент активен (получали пакет в последние 30 сек)
- **Idle**: Клиент неактивен (давно не отвечает)
- **TX only**: Только отправка к клиенту, ответов не получено (нет RSSI/SNR данных)
- **Never**: Никогда не получали пакетов от этого клиента

## Потокобезопасность

Все методы работы с клиентами защищены мьютексом `clientsMutex`:
- Безопасно вызывать из любых задач FreeRTOS
- Автоматическая синхронизация доступа
- Timeout 100ms для захвата мьютекса

## Требования к памяти

- **ClientInfo**: ~40 байт на клиента
- **std::map overhead**: ~32 байта на запись
- **Итого**: ~72 байта на каждого отслеживаемого клиента

Для 10 клиентов: ~720 байт RAM

## Рекомендации

1. **Периодически вызывайте `cleanupInactiveClients()`** для освобождения памяти
2. **Используйте отфильтрованное RSSI** (`getFilteredRssi()`) для принятия решений о качестве связи
3. **Проверяйте `isActive()`** перед отправкой важных данных
4. **Настройте alpha фильтра** (в конструкторе RssiFilter) если нужна другая скорость реакции:
   - 0.1-0.2: Медленная реакция, максимальное сглаживание
   - 0.3-0.5: Баланс (рекомендуется)
   - 0.6-0.8: Быстрая реакция, меньше сглаживания
