import socket
import time
import struct
import math

# Конфігурація
DEST_IP = "10.8.0.3"
PORT = 14550

# Словник відомих команд для наочності
MAV_COMMANDS = {
    22:  "NAV_TAKEOFF",
    400: "COMPONENT_ARM_DISARM",
    176: "DO_SET_MODE",
    511: "SET_MESSAGE_INTERVAL",
    520: "REQUEST_AUTOPILOT_CAPABILITIES",
    521: "REQUEST_PROTOCOL_VERSION"
}

class MavTest:
    def __init__(self):
        self.seq = 0
        self.start_time = time.time()
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        # Дозволяємо сокету слухати відповіді
        self.sock.bind(('0.0.0.0', 14550))
        self.sock.setblocking(False)

    def crc_accumulate(self, b, crc):
        accum = b ^ (crc & 0xFF)
        accum ^= (accum << 4) & 0xFF
        return (crc >> 8) ^ (accum << 8) ^ (accum << 3) ^ (accum >> 4)

    def calculate_crc(self, data, extra):
        crc = 0xFFFF
        for byte in data:
            crc = self.crc_accumulate(byte, crc)
        crc = self.crc_accumulate(extra, crc)
        return crc

    def pack_packet(self, msg_id, payload, crc_extra):
        sys_id = 1
        comp_id = 1
        length = len(payload)
        header = struct.pack("<BBBBBB", 0xFE, length, self.seq, sys_id, comp_id, msg_id)
        self.seq = (self.seq + 1) % 256

        data_for_crc = header[1:] + payload
        crc = self.calculate_crc(data_for_crc, crc_extra)
        return header + payload + struct.pack("<H", crc)

    def send_heartbeat(self):
        # type=2, autopilot=3, base_mode=81, custom=0, status=4, mav_v=3
        payload = struct.pack("<IBBBBB", 0, 2, 3, 81, 4, 3)
        pkt = self.pack_packet(0, payload, 50)
        self.sock.sendto(pkt, (DEST_IP, PORT))

    def send_telemetry(self):
        t = time.time() - self.start_time

        # 1. ATTITUDE (ID 30) - Авіагоризонт
        # roll, pitch, yaw, rollspeed, pitchspeed, yawspeed (в радіанах)
        roll = 0.2 * math.sin(t)  # Хитання вліво-вправо
        pitch = 0.1 * math.cos(t) # Вперед-назад
        yaw = (t * 0.1) % (2 * math.pi)
        payload_att = struct.pack("<Iffffff", int(t*1000), roll, pitch, yaw, 0, 0, 0)
        self.sock.sendto(self.pack_packet(30, payload_att, 39), (DEST_IP, PORT))

        # 2. VFR_HUD (ID 74) - Швидкість та компас
        # airspeed, groundspeed, heading, throttle, alt, climb
        heading = int(math.degrees(yaw)) % 360
        payload_hud = struct.pack("<ffhHff", 10.5, 10.5, heading, 50, 100 + t, 0.5)
        self.sock.sendto(self.pack_packet(74, payload_hud, 20), (DEST_IP, PORT))

    def receive_commands(self):
        try:
            # Читаємо пакет
            data, addr = self.sock.recvfrom(1024)
            if len(data) < 6:
                return

            # MAVLink v1: STX(0), LEN(1), SEQ(2), SYSID(3), COMPID(4), MSGID(5)
            msg_id = data[5]

            # 1. Фільтруємо шум: ігноруємо Heartbeat (0) та Request Stream (66)
            if msg_id in [0, 66]:
                return

            # 2. Обробка COMMAND_LONG (ID 76)
            if msg_id == 76:
                # В MAVLink v1 COMMAND_LONG має payload 33 байти.
                # Структура: param1-7 (4 байти кожна), потім command (2 байти)
                # Команда знаходиться на зміщенні: заголовок(6) + параметри(28) = 34
                if len(data) >= 36:
                    command_id = struct.unpack("<H", data[34:36])[0]
                    command_name = MAV_COMMANDS.get(command_id, f"UNKNOWN_{command_id}")

                    # Витягуємо перший параметр (часто це Arm=1/Disarm=0)
                    param1 = struct.unpack("<f", data[6:10])[0]

                    print(f"\n[!!!] КОМАНДА ВІД ОПЕРАТОРА: {command_name} ({command_id})")
                    print(f"      Параметр 1: {param1}")

                    # Спеціальний вивід для Arm/Disarm
                    if command_id == 400:
                        state = "ARM" if param1 == 1.0 else "DISARM"
                        print(f"      >>> СТАТУС: {state}")
                else:
                    print(f"\n[?] Отримана коротка команда ID 76, довжина: {len(data)}")

            # 3. Обробка інших важливих повідомлень
            else:
                print(f"\n[*] Отримано повідомлення ID: {msg_id}")

        except BlockingIOError:
            pass
        except Exception as e:
            print(f"\n[!] Помилка розбору: {e}")

    def run(self):
        print("Тест телеметрії запущено. Дивіться на авіагоризонт у QGC.")
        try:
            while True:
                self.send_heartbeat()
                self.send_telemetry()
                self.receive_commands()
                time.sleep(0.5)
        except KeyboardInterrupt:
            print("\nЗупинено.")

if __name__ == "__main__":
    test = MavTest()
    test.run()