import socket
import sys
import time
import threading

# --- Налаштування ---
BROKER_IP = "46.225.8.20" 
BROKER_PORT = 9999
LOCAL_PORT = 51821 
MAGIC_PHRASE = "P2P_VERIFIED_DATA"
# --------------------

if len(sys.argv) != 2:
    print("Usage: python3 p2p_test.py [node_name]")
    sys.exit(1)

my_id = sys.argv[1]
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

try:
    sock.bind(('0.0.0.0', LOCAL_PORT))
except list(OSError):
    print(f"Error: Port {LOCAL_PORT} is busy. If you test on 1 PC, use different ports.")
    sys.exit(1)

peer_addr = None

def listen_loop():
    while True:
        try:
            data, addr = sock.recvfrom(1024)
            msg = data.decode(errors='ignore')
            
            # ІГНОРУЄМО ПАКЕТИ ВІД БРОКЕРА
            if addr[0] == BROKER_IP:
                continue
                
            # ПЕРЕВІРЯЄМО МАГІЧНУ ФРАЗУ
            if MAGIC_PHRASE in msg:
                print(f"\n[!!! SUCCESS !!!] Direct P2P packet from {addr}")
                print(f"Content: {msg}")
            else:
                print(f"\n[STRAY PACKET] Received something from {addr}, but not P2P magic.")
        except:
            break

# 1. Реєстрація
print(f"Registering as '{my_id}' on broker {BROKER_IP}...")
sock.sendto(my_id.encode(), (BROKER_IP, BROKER_PORT))

# 2. Очікування даних піра
print("Waiting for peer info...")
while True:
    data, addr = sock.recvfrom(1024)
    if addr[0] == BROKER_IP:
        decoded_data = data.decode()
        p_ip, p_port = decoded_data.split(':')
        peer_addr = (p_ip, int(p_port))
        print(f"Confirmed Peer: {peer_addr}")
        break

# 3. Запуск слухача
threading.Thread(target=listen_loop, daemon=True).start()

# 4. Пробивання (Hole Punching)
print(f"Starting Punching to {peer_addr}. Sending 30 pings...")
for i in range(30):
    # Надсилаємо магічну фразу, щоб інша сторона нас впізнала
    msg = f"{MAGIC_PHRASE} from {my_id} #{i}"
    sock.sendto(msg.encode(), peer_addr)
    time.sleep(1)

print("\nTest finished.")

