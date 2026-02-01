import socket
import time
import struct

# Функція розрахунку CRC X.25, яку використовує MAVLink
def crc_accumulate(b, crc):
    accum = b ^ (crc & 0xFF)
    accum ^= (accum << 4) & 0xFF
    return (crc >> 8) ^ (accum << 8) ^ (accum << 3) ^ (accum >> 4)

def calculate_crc(data, extra):
    crc = 0xFFFF
    for byte in data:
        crc = crc_accumulate(byte, crc)
    crc = crc_accumulate(extra, crc)
    return crc

def generate_heartbeat(seq):
    # Payload: custom_mode(0), type(2=quadrotor), autopilot(3=ardupilot),
    # base_mode(81), system_status(4), mavlink_version(3)
    payload = struct.pack("<IBBBBB", 0, 2, 3, 81, 4, 3)

    sys_id = 1
    comp_id = 1
    msg_id = 0 # HEARTBEAT
    length = len(payload)

    # Заголовок MAVLink v1 (6 байт)
    # [STX, LEN, SEQ, SYS, COMP, MSG]
    header = struct.pack("<BBBBBB", 0xFE, length, seq, sys_id, comp_id, msg_id)

    # CRC розраховується на основі всього пакета без STX (0xFE) + CRC_EXTRA
    # Для HEARTBEAT (msg_id 0) CRC_EXTRA = 50
    data_for_crc = header[1:] + payload
    crc = calculate_crc(data_for_crc, 50)

    return header + payload + struct.pack("<H", crc)

def run_test():
    DEST_IP = "10.8.0.3" # Ваш Mac
    PORT = 14550

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    seq = 0

    print(f"Відправка валідних пакетів на {DEST_IP}:{PORT}...")

    try:
        while True:
            packet = generate_heartbeat(seq)
            sock.sendto(packet, (DEST_IP, PORT))
            seq = (seq + 1) % 256 # Sequence має бути 0-255
            print(f"Відправлено пакет #{seq}")
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nЗупинено.")
    finally:
        sock.close()

if __name__ == "__main__":
    run_test()