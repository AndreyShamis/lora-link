# Broadcast Support in LoRa-Link

## Overview

LoRa-Link поддерживает **broadcast сообщения** - пакеты, которые отправляются одновременно всем узлам в сети. Это особенно полезно для:
- **Heartbeat пакетов** - объявление присутствия узла в сети
- **Системных уведомлений** - общие команды или информация для всех
- **Обнаружения узлов** - поиск активных устройств

## Broadcast Address

Broadcast адрес определён константой:
```cpp
#define DEVICE_ID_BROADCAST 0xFF  // Broadcast address
```

Любой пакет с `receiverId = 0xFF` является broadcast сообщением.

## Характеристики Broadcast Пакетов

Broadcast пакеты имеют специальные характеристики:

1. **Не требуют ACK** - `ackRequired = false`
2. **Fire-and-forget** - `noRetry = true`
3. **Не агрегируются** - отправляются немедленно
4. **Не попадают в pending queue** - нет отслеживания доставки
5. **Принимаются всеми узлами** - независимо от их `srcAddress`

## Использование API

### Отправка Broadcast Пакета

```cpp
// Метод 1: Явный метод sendBroadcast (рекомендуется)
PacketHeartbeat hb;
hb.count = 42;
lora->sendBroadcast(&hb, (uint8_t*)&hb.count);

// Метод 2: Ручная настройка пакета
PacketHeartbeat hb;
hb.broadcast = true;  // Пометить как broadcast
hb.count = 42;
lora->sendPacketBase(DEVICE_ID_BROADCAST, &hb, (uint8_t*)&hb.count);
```

### Приём Broadcast Пакетов

Broadcast пакеты автоматически принимаются всеми узлами и помещаются в `incomingQueue`:

```cpp
LoRaPacket pkt = {};
if (xQueueReceive(lora->getIncomingQueue(), &pkt, 100) == pdTRUE) {
    if (pkt.isBroadcast()) {
        // Это broadcast пакет
        Serial.printf("Received broadcast from %u\n", pkt.getSenderId());
    }
}
```

### Проверка Broadcast Пакета

```cpp
// В LoRaPacket
bool isBroadcast() const { return receiverId == DEVICE_ID_BROADCAST; }

// В PacketBase
bool broadcast = false;  // флаг broadcast
```

## Heartbeat как Broadcast

`PacketHeartbeat` теперь по умолчанию является broadcast пакетом:

```cpp
class PacketHeartbeat : public PacketBase
{
public:
    uint32_t count;
    
    PacketHeartbeat() : count(0) {
        packetType      = CMD_HEARTBEAT;
        payloadLen      = sizeof(count);
        ackRequired     = false;    // Broadcast не требует ACK
        highPriority    = false;    
        service         = true;     // Служебный пакет
        noRetry         = true;     // Не ретраить
        broadcast       = true;     // Это broadcast!
    }
};
```

### Использование в приложениях

**Master Node:**
```cpp
PacketHeartbeat hb;
hb.count = heartbeatCounter++;
lora->sendBroadcast(&hb, (uint8_t*)&hb.count);
```

**Slave Node:**
```cpp
PacketHeartbeat hb;
hb.count = heartbeatCounter++;
lora->sendBroadcast(&hb, (uint8_t*)&hb.count);
```

## Логирование

Broadcast пакеты помечаются в логах специальным маркером:

```
[RX]→[2->255📡BC], T=[H], id:42  # Received broadcast
[TX]→[1->255📡BC], T=[H], id:43  # Sent broadcast
```

## Внутренняя Реализация

### Отправка

1. В `sendPacketBase()`:
   - Автоматически определяется broadcast по `receiverId == 0xFF`
   - Устанавливаются флаги `ackRequired=false`, `noRetry=true`
   - Пакет не агрегируется с другими
   - Пакет отправляется немедленно

2. Broadcast пакеты **не попадают** в `pending` список, так как `ackRequired = false`

### Приём

1. В `receiveTask()`:
   - Проверяется `pkt.isBroadcast()` или `pkt.getReceiverId() == srcAddress`
   - Broadcast пакеты принимаются независимо от `srcAddress`
   - Broadcast пакеты НЕ генерируют ACK
   - Помещаются в `incomingQueue` как обычные пакеты

## Пример: Beacon System

```cpp
// Периодическая отправка beacon-сообщений
void sendBeacon() {
    PacketHeartbeat beacon;
    beacon.count = beaconCounter++;
    
    PacketId_t id = lora->sendBroadcast(&beacon, (uint8_t*)&beacon.count);
    Serial.printf("Beacon broadcast #%lu sent (ID: %u)\n", beacon.count, id);
}

// Приём beacon-сообщений
void receiveLoop() {
    LoRaPacket pkt = {};
    if (xQueueReceive(incomingQueue, &pkt, 100) == pdTRUE) {
        if (pkt.packetType == CMD_HEARTBEAT && pkt.isBroadcast()) {
            uint32_t count;
            memcpy(&count, pkt.payload, sizeof(count));
            Serial.printf("Beacon from node %u: #%lu\n", 
                         pkt.getSenderId(), count);
        }
    }
}
```

## Производительность

- **Время отправки**: Такое же как у обычных пакетов
- **Надёжность**: Fire-and-forget, нет гарантий доставки
- **Накладные расходы**: Минимальные (нет ACK, нет retry)
- **Использование эфира**: Эффективно для объявлений многим узлам

## Ограничения

1. **Нет подтверждения доставки** - нет способа узнать, получил ли кто-то пакет
2. **Нет повторных отправок** - пакет отправляется один раз
3. **Размер payload**: Ограничен `MAX_LORA_PAYLOAD` (85 байт)
4. **Не агрегируются** - каждый broadcast занимает полный слот передачи

## Best Practices

1. **Используйте для некритичных данных**: Heartbeat, статус, обнаружение
2. **Не используйте для критичных команд**: Для них нужен ACK
3. **Контролируйте частоту**: Не заливайте эфир broadcast сообщениями
4. **Добавляйте счётчики**: Помогает отслеживать потери (как в `PacketHeartbeat`)

## Будущие Улучшения

- [ ] Multicast groups (отправка подмножеству узлов)
- [ ] Broadcast с подтверждением (NACK от получателей)
- [ ] Time-synchronized broadcast (координированная отправка)
- [ ] Broadcast priority levels

---

**Реализовано**: 27.12.2025  
**Версия**: 1.0  
**Статус**: ✅ Production Ready
