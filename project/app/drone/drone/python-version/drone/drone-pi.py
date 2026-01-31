import socket
import time

SERVER_IP = "127.0.0.1" # Публічна IP-адреса VPS
SERVER_PORT = 12345

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
# Зв'язуємо сокет з певним портом, якщо потрібно (наприклад, для NAT-traversal, але тут не критично)
# sock.bind(('0.0.0.0', 12346))

print("Raspberry Pi Client started...")
try:
    while True:
        message = f"RPi_command_{time.time()}"
        sock.sendto(message.encode('utf-8'), (SERVER_IP, SERVER_PORT))
        print(f"Sent: {message}")

        # Спробуємо прийняти відповідь (неблокуюче, щоб не чекати вічно)
        sock.settimeout(1.0) # Чекати відповідь 1 секунду
        try:
            data, addr = sock.recvfrom(4096)
            print(f"Received from relay: {data.decode('utf-8')}")
        except socket.timeout:
            pass # No data received within the timeout

        time.sleep(2)
except KeyboardInterrupt:
    print("\nRPi Client shutting down.")
finally:
    sock.close()