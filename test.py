import serial
import threading
import time


PORT = "COM12"       # твой порт
BAUDRATE = 115200    # по умолчанию у DTU
TIMEOUT = 0.2        # секунды


def reader_thread(ser: serial.Serial):
    """Фоновое чтение из порта и печать на экран."""
    while True:
        try:
            data = ser.read(ser.in_waiting or 1)
            if data:
                try:
                    text = data.decode("utf-8", errors="replace")
                except Exception:
                    text = repr(data)
                print("\n[RX] ", text, end="", flush=True)
        except serial.SerialException:
            print("\n[!] Serial port error (disconnected?)")
            break
        except Exception as e:
            print(f"\n[!] Read error: {e}")
            break


def send_line(ser: serial.Serial, line: str):
    """Отправка строки с \r\n в конец."""
    full = (line + "\r\n").encode("utf-8")
    ser.write(full)
    ser.flush()
    print(f"[TX] {repr(line)}")


def main():
    print(f"Открываю порт {PORT} @ {BAUDRATE}...")
    ser = serial.Serial(PORT, BAUDRATE, timeout=TIMEOUT)

    # небольшая пауза после открытия порта
    time.sleep(1.0)

    print("Порт открыт. Запускаю поток чтения...")
    t = threading.Thread(target=reader_thread, args=(ser,), daemon=True)
    t.start()

    # Попробуем зайти в AT-режим и спросить версию
    print("Пробую войти в AT-режим: отправляю '+++'")
    send_line(ser, "+++")
    time.sleep(0.5)

    print("Отправляю 'AT+VER'")
    send_line(ser, "AT+VER")

    print("\nТеперь интерактивный режим.")
    print("Пиши AT-команды БЕЗ \\r\\n, они добавятся сами.")
    print("Специальные команды:")
    print("  /plus   -> отправить +++")
    print("  /exit   -> выйти из программы\n")

    try:
        while True:
            try:
                line = input("> ")
            except EOFError:
                break

            line = line.strip()

            if not line:
                continue

            if line.lower() == "/exit":
                print("Выход.")
                break

            if line.lower() == "/plus":
                send_line(ser, "+++")
                continue

            # обычная команда
            send_line(ser, line)

    except KeyboardInterrupt:
        print("\n[Ctrl+C] Выход.")
    finally:
        ser.close()
        print("Порт закрыт.")


if __name__ == "__main__":
    main()
