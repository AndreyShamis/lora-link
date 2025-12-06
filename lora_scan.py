import serial
import time

PORT = "COM12"
BAUDRATES = [115200, 57600, 38400, 19200, 9600]
TIMEOUT = 1.0

def try_baudrate(baud):
    print("\n=====================================")
    print(f"Пробую {PORT} @ {baud}...")
    try:
        ser = serial.Serial(PORT, baudrate=baud, timeout=TIMEOUT)
    except Exception as e:
        print(f"[!] Не удалось открыть порт: {e}")
        return

    # сброс входного буфера
    ser.reset_input_buffer()
    ser.reset_output_buffer()

    # guard-time: 1 сек тишины
    print("  Тишина 1 c перед +++ ...")
    time.sleep(1.0)

    # отправляем "+++" БЕЗ \r\n (классический escape)
    print("  Отправляю '+++' (без CRLF)")
    ser.write(b"+++")
    ser.flush()

    # тишина после +++
    print("  Жду 1 c после +++ ...")
    time.sleep(1.0)

    # читаем что есть
    data = ser.read(ser.in_waiting or 1)
    if data:
        print("  [RX после +++]", repr(data))

    # теперь пробуем AT+VER с CRLF
    cmd = "AT+VER\r\n"
    print(f"  Отправляю {repr(cmd.strip())}")
    ser.write(cmd.encode("utf-8"))
    ser.flush()

    # ждём и читаем ещё пару секунд
    t_end = time.time() + 2.5
    buf = b""
    while time.time() < t_end:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            buf += chunk
        else:
            time.sleep(0.1)

    if buf:
        try:
            text = buf.decode("utf-8", errors="replace")
        except Exception:
            text = repr(buf)
        print("  [RX общий ответ]:")
        print("  -----------------")
        print(text)
        print("  -----------------")
    else:
        print("  [RX] Ничего не получено на этой скорости.")

    ser.close()
    print(f"Закрыл {PORT} @ {baud}.")


def main():
    print(f"Диагностика USB-TO-LoRa на {PORT}")
    for b in BAUDRATES:
        try_baudrate(b)


if __name__ == "__main__":
    main()
