import serial
import time

PORT = "COM12"
BAUD = 115200

ESC = bytes([0xFF, 0x55, 0xAA, 0x5A])

with serial.Serial(PORT, BAUD, timeout=1) as ser:
    print(f"Sending bootloader escape sequence on {PORT} @ {BAUD}...")
    ser.reset_input_buffer()
    ser.reset_output_buffer()

    ser.write(ESC)
    ser.flush()

    time.sleep(1)

    data = ser.read(ser.in_waiting or 1)
    print("RX:", data)

print("Done.")
