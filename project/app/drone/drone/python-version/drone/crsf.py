import serial
import time

# Конфігурація послідовного порту
PORT = '/dev/ttyS3'
BAUDRATE = 420000
TIMEOUT = 0.1 # Таймаут для читання з порта

# CRSF пакетні байти (для довідки)
CRSF_SYNC_BYTE = 0xC8
CRSF_EXT_SYNC_BYTE = 0xEE # Для розширених пакетів, якщо вони використовуються

# Типи CRSF пакетів (деякі найпоширеніші)
CRSF_FRAMETYPE_GPS = 0x02
CRSF_FRAMETYPE_BATTERY_SENSOR = 0x08
CRSF_FRAMETYPE_LINK_STATISTICS = 0x14
CRSF_FRAMETYPE_RC_CHANNELS_PACKET = 0x16
CRSF_FRAMETYPE_ATTITUDE = 0x1E
CRSF_FRAMETYPE_DEVICE_PING = 0x28
CRSF_FRAMETYPE_DEVICE_INFO = 0x29
CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY = 0x2B
CRSF_FRAMETYPE_COMMAND = 0x32
CRSF_FRAMETYPE_FLIGHT_MODE = 0x21 # ELRS custom flight mode

class CRSF(object):
    def __init__(self, port=PORT, baudrate=BAUDRATE):
        self.ser = serial.Serial(
            port = port, baudrate = baudrate,
            bytesize = serial.EIGHTBITS,
            parity = serial.PARITY_NONE,
            stopbits = serial.STOPBITS_ONE,
            timeout = TIMEOUT
        )
        if not self.ser.is_open:
            self.ser.open()

        self.buffer = bytearray()
        self.debounce = 5
        self.channels = None
        self.last_timestamp = 0

    def process(self):
        if self.ser.in_waiting > 0:
            data = self.ser.read(self.ser.in_waiting)
            # print(f"Received from CRSF: {data}")
            if data:
                self.buffer.extend(data)
            self.parse()

    def parse(self):
        while True:
            # Шукаємо початковий байт CRSF пакету
            sync_index = self.buffer.find(CRSF_SYNC_BYTE)
            if sync_index == -1:
                # Якщо синхро-байта немає, очищаємо буфер і чекаємо далі
                self.buffer.clear()
                break

            # Видаляємо всі байти до синхро-байта
            if sync_index > 0:
                self.buffer = self.buffer[sync_index:]
                sync_index = 0 # Тепер синхро-байт знаходиться на початку буфера

            # Перевіряємо, чи є достатньо байтів для заголовка (Sync, Length, Type)
            if len(self.buffer) < 3:
                # Недостатньо даних для повного заголовка, чекаємо далі
                break

            # Довжина пакета (включаючи Type і CRC, але не Sync байт)
            # CRSF_LENGTH_BYTE вказує на кількість байтів після CRSF_SYNC_BYTE.
            # Це Length (1 байт) + Type (1 байт) + Payload (N байтів) + CRC (1 байт).
            # Таким чином, повна довжина пакета = Length_Byte + 2 (для Sync і Length байтів)
            packet_length = self.buffer[1]
            total_packet_size = packet_length + 2 # + Sync byte, + Length byte

            # Перевіряємо, чи весь пакет вже в буфері
            if len(self.buffer) < total_packet_size:
                # Пакет ще не повністю отримано, чекаємо далі
                break

            # Отримуємо повний пакет
            packet = self.buffer[:total_packet_size]

            # Видаляємо оброблений пакет з буфера
            self.buffer = self.buffer[total_packet_size:]

            # Парсинг пакета
            sync_byte = packet[0]
            length_byte = packet[1] # Довжина Payload + Type + CRC
            frame_type = packet[2]
            payload = packet[3:-1] # Payload знаходиться між Type та CRC
            crc = packet[-1]

            # Проста перевірка CRC (можна додати складнішу логіку для CRC8)
            # CRC в CRSF розраховується від Length до кінця Payload
            # crc_calculated = calculate_crsf_crc8(packet[1:-1])
            # if crc_calculated == crc:
            #     print("CRC OK")
            # else:
            #     print(f"CRC ERROR! Expected {crc_calculated}, got {crc}")

            # print(f"Отримано CRSF пакет: ")
            # print(f"  Sync: 0x{sync_byte:02X}")
            # print(f"  Length (payload+type+crc): {length_byte} bytes")
            # print(f"  Total packet size: {total_packet_size} bytes")
            # print(f"  Frame Type: 0x{frame_type:02X} (")

            # Визначення типу фрейму
            if frame_type == CRSF_FRAMETYPE_GPS:
                print("    GPS Data")
            elif frame_type == CRSF_FRAMETYPE_BATTERY_SENSOR:
                print("    Battery Sensor Data")
            elif frame_type == CRSF_FRAMETYPE_LINK_STATISTICS:
                pass
                # print("    Link Statistics")
                # Приклад парсингу Link Statistics
                # RSSI Ant 1 (dBm), RSSI Ant 2 (dBm), Link Quality (%), SNR (dB),
                # Active Antenna, RF Mode, Transmit Power, RC Gets, Failsafe
                # if len(payload) >= 10:
                #     print(f"    RSSI Ant 1: {-payload[0]} dBm")
                #     print(f"    RSSI Ant 2: {-payload[1]} dBm")
                #     print(f"    Link Quality: {payload[2]} %")
                #     print(f"    SNR: {(payload[3] - 128) / 2.0} dB") # SNR може бути від -128 до 127, з кроком 0.5
                #     print(f"    Active Antenna: {payload[4]}")
                #     print(f"    RF Mode: {payload[5]}")
                #     print(f"    Transmit Power: {payload[6]}")
                #     print(f"    RC Gets: {payload[7]}")
                #     print(f"    Failsafe: {payload[8]}")

            elif frame_type == CRSF_FRAMETYPE_RC_CHANNELS_PACKET:
                # print("    RC Channels Data")
                # CRSF RC Channels Packet містить 16 каналів, кожен по 11 біт.
                # Це 22 байти payload (16 каналів * 11 біт = 176 біт = 22 байти).
                if len(payload) >= 22:
                    # Для парсингу каналів потрібно працювати на бітовому рівні.
                    # Приклад для перших кількох каналів:
                    # Канал 1: payload[0] & 0x7FF
                    # Канал 2: (payload[0] >> 3) & 0x7FF ... це складно без повної логіки
                    # Простіше використовувати функцію для розпакування 11-бітних значень
                    channels = []
                    data_bits = int.from_bytes(payload, 'little')
                    for i in range(16):
                        channel_value = (data_bits >> (i * 11)) & 0x07FF
                        # Значення 174 (мінімум)..992(середина)..1811 (максимум)
                        # Оттранслируем в значення -1.0 ... 1.0 (ровно)
                        # channels.append(channel_value)
                        channels.append((channel_value - 992) / 818)
                    self.channels = channels
                    self.last_timestamp = time.time()
                    self.debounce -= 1
                    if self.debounce == 0:
                        # print(f"    Channels: {channels}")
                        self.debounce = 5
                else:
                    print(f"    RC Channels payload too short: {len(payload)} bytes")
            elif frame_type == CRSF_FRAMETYPE_ATTITUDE:
                print("    Attitude Data")
            elif frame_type == CRSF_FRAMETYPE_DEVICE_INFO:
                print("    Device Info")
            else:
                print("    Unknown/Other Type")
            # print(f"  Payload: {payload.hex()}")
            # print(f"  CRC: 0x{crc:02X}")
            # print("-" * 30)
            pass

    # def send(self, data: bytes):