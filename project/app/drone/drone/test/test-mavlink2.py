import socket
import time
import struct
import math

DEST_IP = "10.8.0.3"
PORT = 14550

class MavTest:
    def __init__(self):
        self.seq = 0
        self.start_time = time.time()
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(('0.0.0.0', 14550))
        self.sock.setblocking(False)
        self.params_sent = False

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
        payload = struct.pack("<IBBBBB", 0, 2, 3, 81, 4, 3)
        self.sock.sendto(self.pack_packet(0, payload, 50), (DEST_IP, PORT))

    # ВІДПОВІДЬ НА ЗАПИТ ПАРАМЕТРІВ
    def send_param(self, param_id, value):
        # PARAM_VALUE (ID 22)
        # param_value (float), param_count (uint16), param_index (uint16), param_id (char[16]), param_type (uint8)
        param_id_bytes = param_id.encode('utf-8').ljust(16, b'\0')
        payload = struct.pack("<fHH16sB", value, 1, 0, param_id_bytes, 9) # 9 = MAV_PARAM_TYPE_REAL32
        pkt = self.pack_packet(22, payload, 158)
        self.sock.sendto(pkt, (DEST_IP, PORT))
        print(f"      >>> Відправлено параметр: {param_id}")

    def send_telemetry(self):
        t = time.time() - self.start_time
        # ATTITUDE
        roll, pitch = 0.2 * math.sin(t), 0.1 * math.cos(t)
        payload_att = struct.pack("<Iffffff", int(t*1000), roll, pitch, 0, 0, 0, 0)
        self.sock.sendto(self.pack_packet(30, payload_att, 39), (DEST_IP, PORT))
        # VFR_HUD
        payload_hud = struct.pack("<ffhHff", 10.5, 10.5, int(t)%360, 50, 100+t, 0)
        self.sock.sendto(self.pack_packet(74, payload_hud, 20), (DEST_IP, PORT))

    def send_ack(self, command_id):
        # COMMAND_ACK (ID 77)
        # command (uint16), result (uint8), progress (uint8), result_param2 (int32), target_system (uint8), target_component (uint8)
        # result: 0 = MAV_RESULT_ACCEPTED
        payload = struct.pack("<HBB i BB", command_id, 0, 0, 0, 255, 0)
        pkt = self.pack_packet(77, payload, 143) # CRC_EXTRA для ID 77 = 143
        self.sock.sendto(pkt, (DEST_IP, PORT))
        print(f"      >>> ВІДПРАВЛЕНО ПІДТВЕРДЖЕННЯ (ACK) для команди {command_id}")

    def receive_commands(self):
        try:
            data, addr = self.sock.recvfrom(1024)
            if len(data) < 6: return
            msg_id = data[5]

            # 1. Запит списку параметрів (ID 21)
            if msg_id == 21:
                print("\n[!] QGC запитав список параметрів (PARAM_REQUEST_LIST)")
                self.send_param("DUMMY_PARAM", 1.0)

            # 2. Запит одного параметра за назвою (ID 20)
            elif msg_id == 20:
                print("\n[!] QGC запитав окремий параметр")
                self.send_param("DUMMY_PARAM", 1.0)

            # 3. Команди (ID 76)
            elif msg_id == 76:
                cmd = struct.unpack("<H", data[34:36])[0]
                print(f"\n[!!!] Отримана команда: {cmd}")

                # Відповідаємо ACK на важливі команди
                if cmd == 511: # SET_MESSAGE_INTERVAL
                    print(f"\n[!] QGC просить змінити інтервал повідомлень (511)")
                    self.send_ack(511)
                else:
                    self.send_ack(cmd)

        except BlockingIOError:
            pass

    def run(self):
        print("Тест з підтримкою параметрів запущено...")
        try:
            while True:
                self.send_heartbeat()
                self.send_telemetry()
                self.receive_commands()
                time.sleep(0.5)
        except KeyboardInterrupt:
            pass

if __name__ == "__main__":
    MavTest().run()