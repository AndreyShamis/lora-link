import serial
import time

PORT = "COM12"
BAUD = 115200

with serial.Serial(PORT, BAUD, timeout=1) as ser:
    print("Ожидаю 2 секунды полной тишины...")
    time.sleep(2)

    print("Отправляю + по одному байту...")
    ser.write(b'+')
    ser.flush()
    time.sleep(0.2)

    ser.write(b'+')
    ser.flush()
    time.sleep(0.2)

    ser.write(b'+')
    ser.flush()

    print("Жду 2 секунды после +++...")
    time.sleep(2)

    print("Читаю ответ...")
    data = ser.read(ser.in_waiting or 1)
    print("RX:", data)

print("Готово.")
