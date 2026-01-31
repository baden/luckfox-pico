import serial

ser1 = serial.Serial(
    port = "/dev/tty.usbmodem0001A00000012", baudrate = 420000,
    bytesize = serial.EIGHTBITS,
    parity = serial.PARITY_NONE,
    stopbits = serial.STOPBITS_ONE,
    timeout = 0.1
)
if not ser1.is_open:
    ser1.open()

data = b"1234567890"

ser1.write(data)
print(f"Sent to CRSF: {data}")

ser1.close()

