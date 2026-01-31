import socket
import sys
import threading
import time
import json
from lib_syspwm import HPWM
import subprocess

import uuid
import base64

from oled.device import ssd1306
from oled.render import canvas
from PIL import ImageFont, ImageDraw, Image
import PIL.ImageOps

try:
    import RPi.GPIO as GPIO
    isRPi = True
except ImportError:
    isRPi = False


mac = uuid.getnode()
mac_bytes = mac.to_bytes(6, 'big')
mac_b64 = base64.b64encode(mac_bytes).decode('utf-8')
# print(mac_b64)  # Наприклад: 'ABEiM0RVZne='

# Глобальний прапорець для завершення процесу
should_exit = False

arm_active_start_time = None  # Час початку ARM

# Таймаут отримання даних від UDP-сервера
network_data_timeout = 0  # 0 - дані є, >0 - скільки секунд немає даних
last_udp_data_time = time.time()


# Стан. Трохи заплутався, так шо нормалізу все.
# Осі 0 та 1 - це осі керування.
actual_axis_0 = 0.0
actual_axis_1 = 0.0
actual_arm_state = False
actual_lebidka_state = "stop"
actual_aktuator_state = "stop"

#latest_axes0 = 0.0
#latest_axes1 = 0.0
axes_lock = threading.Lock()

def clamp(minimum, x, maximum):
    return max(minimum, min(x, maximum))


if isRPi:
    led_pin = 17
    GPIO.setwarnings(True)
    GPIO.setmode(GPIO.BCM)      # GPIO.BOARD не спрацювало
    GPIO.setup(led_pin, GPIO.OUT)
    servo1 = HPWM(0, 1)
    servo2 = HPWM(0, 2)
else:
    # Luckfox Pico Pro
    servo1 = HPWM(5, 0)
    servo2 = HPWM(6, 0)

    from periphery import GPIO
    # GPIO pin number calculation formula: pin = bank * 32 + number
    # GPIO group number calculation formula: number = group * 8 + X
    # Therefore: pin = bank * 32 + (group * 8 + X)
    # group : 2 (A=0, B=1, C=2, D=3)

    # BEEPER                           | 26 | GPIO2_A2
    Buzzer_Pin = (2 * 32) + (0 * 8) + 2
    Buzzer_GPIO = GPIO(Buzzer_Pin, "out")
    Buzzer_GPIO.write(False) # LOW

    # IO1 (лебідка вгору)              |  4 | GPIO1_C7
    # IO2 (лебідка вниз)               |  5 | GPIO1_C6
    # IO3 ()                           |  6 | GPIO1_C5
    # IO4                              |  7 | GPIO1_C4
    IO1_Pin = (1 * 32) + (2 * 8) + 7
    IO2_Pin = (1 * 32) + (2 * 8) + 6
    IO3_Pin = (1 * 32) + (2 * 8) + 5
    IO4_Pin = (1 * 32) + (2 * 8) + 4
    IO1_GPIO = GPIO(IO1_Pin, "out")
    IO2_GPIO = GPIO(IO2_Pin, "out")
    IO3_GPIO = GPIO(IO3_Pin, "out")
    IO4_GPIO = GPIO(IO4_Pin, "out")
    # Активний рівень - LOW
    IO1_GPIO.write(True)
    IO2_GPIO.write(True)
    IO3_GPIO.write(True)
    IO4_GPIO.write(True)

# Функція для керування лебідкою
# IO1_GPIO=0 - лебідка вгору
# IO2_GPIO=0 - лебідка вниз
# IO1_GPIO=IO2_GPIO=1 - зупинка лебідки
# Комбінація IO1_GPIO=IO2_GPIO=0 заборонена
def lebidka(direction):
    print(f"Lebidka direction: {direction}")
    if direction == "up":
        IO2_GPIO.write(True)
        IO1_GPIO.write(False)
    elif direction == "down":
        IO1_GPIO.write(True)
        IO2_GPIO.write(False)
    else:
        IO1_GPIO.write(True)
        IO2_GPIO.write(True)


# Функція керування актуатором
# IO3_GPIO - актуатор вперед
# IO4_GPIO - актуатор назад
# IO3_GPIO=IO4_GPIO=1 - зупинка актуатора
# Комбінація IO3_GPIO=IO4_GPIO=0 заборонена
def aktuator(direction):
    print(f"Aktuator direction: {direction}")
    if direction == "forward":
        IO3_GPIO.write(False)
        IO4_GPIO.write(True)
    elif direction == "backward":
        IO3_GPIO.write(True)
        IO4_GPIO.write(False)
    else:
        IO3_GPIO.write(True)
        IO4_GPIO.write(True)

def sound_buzzer(pattern, delay=0.1):
    """
    pattern: list of bools, True=ON, False=OFF
    delay: seconds for each state
    """
    def buzzer_worker():
        for state in pattern:
            Buzzer_GPIO.write(state)
            time.sleep(delay)
        Buzzer_GPIO.write(False)
    threading.Thread(target=buzzer_worker, daemon=True).start()

# Спробую ssd1306
# from SSD1306 import SSD1306
# oled = SSD1306()
# oled.ClearWhite()



udp_control_last_timestamp = 0
crsf_control_last_timestamp = 0

# Також спробуємо через gpiozero

# from gpiozero import DigitalOutputDevice, PWMOutputDevice, Device
# from gpiozero.pins.lgpio import LGPIOFactory
# # # gpiozero
# # # Вказуємо lgpio як основну фабрику пінів для gpiozero
# factory = LGPIOFactory(chip=0)
# # # Device.pin_factory = LGPioFactory()
# Device.pin_factory = factory

# from rpi_hardware_pwm import HardwarePWM

# servo_pin = 18

#led = LED(17) # Use the BCM pin number for your LED
# 1. Керування цифровими виходами On/Off
# Припустимо, що у нас світлодіод підключено до GPIO 17
# led = DigitalOutputDevice(led_pin)

# 2. Формування PPM (1..2 мсек)
# Для PPM, що зазвичай використовується для сервоприводів, ми використовуємо PWM.
# gpiozero абстрагує це і дозволяє встановити значення від 0 до 1.
# Для сервоприводів, зазвичай, 1 мс відповідає 0,05-0,07, а 2 мс - 0,10-0,12 в залежності від сервоприводу.
# Тут ми будемо емулювати це значеннями duty_cycle.
# Припустимо, сервопривід підключено до GPIO 18
# ШИМ є ще на пінах GPIO12, GPIO13 та GPIO19

# GPIO.setup(servo_pin, GPIO.OUT)
# servo = GPIO.PWM(servo_pin, 50) # Частота 50 Гц
# servo.start(7.5) # Початкове положення (1.5 мс)

# Тут це не пін, а канал
# Нам треба /sys/class/pwm/pwmchip0/pwm2
# if not servo.pwmX_exists():
#     servo.create_pwmX()
servo1.set_frequency(50) # 50 Гц
servo1.set_duty_cycle(1.5)
servo1.polarity(normal=True)
servo1.enable()

servo2.set_frequency(50) # 50 Гц
servo2.set_duty_cycle(1.5)
servo2.polarity(normal=True)
servo2.enable()

# servo = HardwarePWM(pwm_channel=1, hz=50, chip=0)
# servo.start(7.5)

# servo = PWMOutputDevice(servo_pin, frequency=50) # Сервоприводи зазвичай працюють на частоті 50 Гц
# servo.value = 0.5

# SERVER_IP = "127.0.0.1"  # IP сервера
SERVER_IP = "s.navi.cc"  # IP сервера
SERVER_PORT = 8766       # Порт UDP-сервера
SEND_INTERVAL = 15       # Інтервал keep-alive (секунд)
PPM_UPDATE_INTERVAL = 0.02 # 20 мс
OLED_UPDATE_INTERVAL = 0.5 # 0.5 секунда

# def wait_for_internet(timeout=5):
#     while True:
#         try:
#             # Пінгуємо Google DNS
#             subprocess.check_call(["ping", "8.8.8.8", "-c", "1", "-W", str(timeout)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
#             print("Інтернет доступний.")
#             return
#         except subprocess.CalledProcessError:
#             print("Немає інтернету. Очікування...")
#             time.sleep(2)


def listen(sock):
    global actual_axis_0, actual_axis_1, actual_arm_state
    global actual_lebidka_state, actual_aktuator_state
    global last_udp_data_time, network_data_timeout
    global should_exit
    global udp_control_last_timestamp
    global arm_active_start_time

    while not should_exit:
        try:
            data, addr = sock.recvfrom(4096)
            last_udp_data_time = time.time()
            network_data_timeout = 0
            try:
                packet = json.loads(data.decode())
                command = packet.get("command", "")
                if command == "joy_update":
                    data = packet.get("data", {})
                    axes = data.get("axes", [])
                    buttons = data.get("buttons", [])

                    # Керування по радіо має пріоритет. True якшо пройшло більше 5 секунд після останнього отримання даних
                    lost_radio_control = ((time.time() - crsf_control_last_timestamp) > 5.0)

                    if lost_radio_control and buttons:
                        new_arm_button = buttons[0] == 1
                        # ARM активується якщо кнопка натиснута, навіть після DISARM по таймауту
                        if actual_arm_state != new_arm_button or (not actual_arm_state and new_arm_button):
                            actual_arm_state = new_arm_button
                            if isRPi:
                                GPIO.output(led_pin, GPIO.HIGH if actual_arm_state else GPIO.LOW)
                            else:
                                if actual_arm_state:
                                    sound_buzzer([True, False])
                                else:
                                    sound_buzzer([True, False, True, False])
                        # IO1_GPIO.write(buttons[1] == 0) # "A"
                        # IO2_GPIO.write(buttons[3] == 0) # "D"

                    if lost_radio_control and axes:
                        udp_control_last_timestamp = time.time()
                        with axes_lock:
                            if actual_arm_state:
                                actual_axis_0 = axes[0]
                                actual_axis_1 = axes[1]
                            else:
                                actual_axis_0 = 0.0
                                actual_axis_1 = 0.0

                            # axes[2] < -0.5 - тумблер B вверх.
                            # axes[2] > 0.5 - тумблер B вниз.
                            # інакше - тумблер B в нейтральному положенні.
                            if axes[2] < -0.5:
                                new_lebidka_state = "up"
                            elif axes[2] > 0.5:
                                new_lebidka_state = "down"
                            else:
                                new_lebidka_state = "neutral"

                            # axes[3] < -0.5 - тумблер C вверх.
                            # axes[3] > 0.5 - тумблер C вниз.
                            # інакше - тумблер C в нейтральному положенні.
                            if axes[3] < -0.5:
                                new_aktuator_state = "forward"
                            elif axes[3] > 0.5:
                                new_aktuator_state = "backward"
                            else:
                                new_aktuator_state = "neutral"

                            if actual_lebidka_state != new_lebidka_state:
                                actual_lebidka_state = new_lebidka_state
                                lebidka(new_lebidka_state)

                            if actual_aktuator_state != new_aktuator_state:
                                actual_aktuator_state = new_aktuator_state
                                aktuator(new_aktuator_state)

                    else:
                        print("No axes data", packet)

                if command == "restart":
                    print("Received restart command")
                    should_exit = True

                if actual_arm_state:
                    arm_active_start_time = time.time()
                else:
                    arm_active_start_time = None

            except Exception as e:
                print(f"Received from server: {data.decode()}")
                print(f"Error parsing packet: {e}")
        except socket.timeout:
            continue
        except Exception as e:
            print(f"Listen error: {e}")
            break

def update_servos():
    global actual_axis_0, actual_axis_1
    # Лівий борт: actual_axis_0 + actual_axis_1:
    # Правий борт: actual_axis_1 - actual_axis_0
    left_servo = actual_axis_0 + actual_axis_1
    right_servo = actual_axis_1 - actual_axis_0
    servo1.set_duty_cycle(clamp(0.5, 1.5 + right_servo * 1.0, 2.5))
    servo2.set_duty_cycle(clamp(0.5, 1.5 + left_servo * 1.0, 2.5))

def ppm_update():
    global actual_axis_0, actual_axis_1
    global network_data_timeout, last_udp_data_time
    global actual_lebidka_state, actual_aktuator_state
    prev_axes0 = 0.0
    prev_axes1 = 0.0
    global should_exit
    while not should_exit:
        now = time.time()
        # Визначаємо останній час керування (UDP або CRSF)
        last_control_time = max(last_udp_data_time, crsf_control_last_timestamp)
        # Перевіряємо таймаут даних
        if now - last_control_time > 1.5:
            network_data_timeout = now - last_control_time
            # Плавно зменшуємо latest_axes0/latest_axes1 до нуля за 1 секунду
            with axes_lock:
                step = PPM_UPDATE_INTERVAL / 1.0
                if abs(actual_axis_0) > 0.001:
                    if actual_axis_0 > 0:
                        actual_axis_0 = max(0.0, actual_axis_0 - step)
                    else:
                        actual_axis_0 = min(0.0, actual_axis_0 + step)
                if abs(actual_axis_1) > 0.001:
                    if actual_axis_1 > 0:
                        actual_axis_1 = max(0.0, actual_axis_1 - step)
                    else:
                        actual_axis_1 = min(0.0, actual_axis_1 + step)
            # Якщо втрачено будь-яке керування (UDP або CRSF) понад 3 секунди
            udp_timeout = now - last_udp_data_time > 3.0
            crsf_timeout = now - crsf_control_last_timestamp > 3.0
            if udp_timeout or crsf_timeout:
                try:
                    if actual_lebidka_state != "neutral":
                        actual_lebidka_state = "neutral"
                        lebidka(actual_lebidka_state)
                    if actual_aktuator_state != "neutral":
                        actual_aktuator_state = "neutral"
                        aktuator(actual_aktuator_state)
                except Exception as e:
                    print(f"Neutral IO error: {e}")
            # Переводимо в disarm якщо таймаут > 10 секунд
            global actual_arm_state
            if now - last_control_time > (3*60) and actual_arm_state:
                if actual_arm_state:
                    actual_arm_state = False
                    sound_buzzer([True, False, True, False])  # Звук disarm
        else:
            network_data_timeout = 0

        with axes_lock:
            axes0 = actual_axis_0
            axes1 = actual_axis_1
        # Оновлюємо тільки якщо зміна axes0 > 0.01
        # axes0 - horizontal (ліворуч(-1)/праворуч(+1))
        # axes1 - vertical (вперед(+1)/назад(-1))
        if prev_axes0 is None or abs(axes0 - prev_axes0) >= 0.01:
            # duty_cycle = 7.5 + axes0 * 2.5  # -1 -> 5, 0 -> 7.5, 1 -> 10
            # print(f"Updating servo1: axes[0]={axes0}, duty_cycle={duty_cycle}")
            # servo1.set_duty_cycle(1.5 + axes0 * 0.5)
            prev_axes0 = axes0
            update_servos()

        if prev_axes1 is None or abs(axes1 - prev_axes1) >= 0.01:
            # duty_cycle = 7.5 + axes1 * 2.5
            # print(f"Updating servo2: axes[1]={axes1}, duty_cycle={duty_cycle}")
            # servo2.set_duty_cycle(1.5 + axes1 * 0.5)
            prev_axes1 = axes1
            update_servos()

        time.sleep(PPM_UPDATE_INTERVAL)


oled_update_predelay = 5 # Затримка перед першим оновленням OLED 5 cекунд

# Глобальний прапорець для завершення потоку oled_update
should_exit_oled = False

device = ssd1306(port=4, address=0x3C)

def oled_init():
    try:
        device.init()
    except Exception as e:
        print(f"Error initializing OLED: {e}")
        return
    with canvas(device) as draw:
        draw.rectangle((0, 0, device.width, device.height), outline=0, fill=0x00)
        drone_image = Image.open("images/drone.png").convert("1")
        draw.bitmap((0, 0), drone_image, fill=1)
        # draw.text((0, 0), "Init DRONE", font=font, fill=255)

# font = ImageFont.load_default()
font = ImageFont.truetype(r'fonts/C&C Red Alert [INET].ttf', size=22)

# device.SendCommand(0xb0 + i)
# device.command(0xB0, 0x02, 0x10)
# device.data([0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F])


armed_image = Image.open("images/armed.png").convert("1")
armed_image = PIL.ImageOps.invert(armed_image)
disarmed_image = Image.open("images/disarmed.png").convert("1")
disarmed_image = PIL.ImageOps.invert(disarmed_image)

def oled_loop_task(counter):
    global actual_axis_0, actual_axis_1, actual_arm_state
    global crsf_control_last_timestamp
    global udp_control_last_timestamp

    with canvas(device) as draw:
        draw.rectangle((0, 0, device.width, device.height), outline=0, fill=0x00)
        draw.text((0, 0), f"{actual_axis_0:.1f}:{actual_axis_1:.1f}", font=font, fill=255)
        draw.text((0, 16), f"ID: {mac_b64}", font=font, fill=255)

        if actual_arm_state:
            # draw.bitmap((90, 0), armed_image, fill=1)
            draw.text((90, 0), f"ARM", font=font, fill=255)
        else:
            #draw.bitmap((90, 0), disarmed_image, fill=1)
            draw.text((90, 0), f"DIS", font=font, fill=255)

        if time.time() - crsf_control_last_timestamp < 5.0:
            draw.text((74, 0), f"R", font=font, fill=255)
        elif time.time() - udp_control_last_timestamp < 5.0:
            draw.text((74, 0), f"S", font=font, fill=255)
        else:
            draw.text((74, 0), f"-", font=font, fill=255)

def oled_update():
    global should_exit_oled
    counter = 0
    oled_init()
    time.sleep(oled_update_predelay)
    global should_exit
    while not should_exit_oled and not should_exit:
        try:
            oled_loop_task(counter)
            counter += 1
            time.sleep(OLED_UPDATE_INTERVAL)
        except Exception as e:
            print(f"oled_update error: {e}")
            time.sleep(5)
            oled_init()

def keep_alive(sock):
    global should_exit
    global arm_active_start_time, actual_arm_state

    while not should_exit:
        packet = f'{{"command": "keep_alive", "id": "{mac_b64}"}}'
        try:
            sock.sendto(packet.encode('utf-8'), (SERVER_IP, SERVER_PORT))
            print("Keep-alive packet sent.")
            time.sleep(SEND_INTERVAL)
        except OSError as e:
            print(f"Keep-alive error: {e}")
            # Не завершуємо потік, а чекаємо і пробуємо знову
            time.sleep(5)
        if arm_active_start_time is not None:
            if time.time() - arm_active_start_time > (20 * 60):
                print("ARM timeout exceeded. Exiting.")
                should_exit = True

from crsf import CRSF

latest_crsf_channels = []
latest_crsf_timestamp = 0

def crsf_read(crsf):
    global latest_crsf_channels, latest_crsf_timestamp
    global actual_arm_state
    global actual_axis_0, actual_axis_1
    global actual_lebidka_state, actual_aktuator_state
    global crsf_control_last_timestamp
    global arm_active_start_time

    global should_exit
    while not should_exit:
        crsf.process()
        if crsf.channels:
            # Обробка отриманих каналів

            latest_crsf_channels = crsf.channels
            crsf.channels = None
            crsf_control_last_timestamp = time.time()

            # if time.time() - latest_crsf_timestamp > 1.0:
            #     print(f"Latest CRSF Channels: {latest_crsf_channels}")
            latest_crsf_timestamp = time.time()

            if latest_crsf_channels:
                # Треба показати на OLED шо керування по радіо
                # канал № - ARM/DISARM (>0.5)
                new_arm_button = latest_crsf_channels[4] > 0.5

                if new_arm_button:
                    actual_axis_0 = latest_crsf_channels[0]
                    actual_axis_1 = latest_crsf_channels[1]
                else:
                    actual_axis_0 = 0.0
                    actual_axis_1 = 0.0

                update_servos()

                if actual_arm_state:
                    arm_active_start_time = time.time()
                else:
                    arm_active_start_time = None


                # ARM активується якщо канал > 0.5, навіть після DISARM по таймауту
                if actual_arm_state != new_arm_button or (not actual_arm_state and new_arm_button):
                    actual_arm_state = new_arm_button
                    if isRPi:
                        GPIO.output(led_pin, GPIO.HIGH if actual_arm_state else GPIO.LOW)
                    else:
                        if actual_arm_state:
                            sound_buzzer([True, False])
                        else:
                            sound_buzzer([True, False, True, False])

                # канал latest_crsf_channels[2] - лебідка
                if latest_crsf_channels[2] < -0.5:
                    new_lebidka_state = "up"
                elif latest_crsf_channels[2] > 0.5:
                    new_lebidka_state = "down"
                else:
                    new_lebidka_state = "neutral"

                # канал latest_crsf_channels[3] - актуатор
                if latest_crsf_channels[3] < -0.5:
                    new_aktuator_state = "forward"
                elif latest_crsf_channels[3] > 0.5:
                    new_aktuator_state = "backward"
                else:
                    new_aktuator_state = "neutral"

                if actual_lebidka_state != new_lebidka_state:
                    actual_lebidka_state = new_lebidka_state
                    lebidka(new_lebidka_state)

                if actual_aktuator_state != new_aktuator_state:
                    actual_aktuator_state = new_aktuator_state
                    aktuator(new_aktuator_state)

        time.sleep(0.02)


def main():
    # Стартовий піск
    sound_buzzer([True, False])

    crsf = CRSF(port="/dev/ttyS3", baudrate=420000)

    # wait_for_internet()
    sock = None
    udp_threads_started = False
    global should_exit
    while True:
        try:
            # Спроба створити сокет, якщо ще не створено і не стартували потоки

            if sock is None and not udp_threads_started:
                try:
                    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                    sock.settimeout(1.0)
                    sock.bind(("", 0))
                    # Відправляємо перший реєстраційний пакет
                    packet = f'{{"command": "register", "id": "{mac_b64}"}}'
                    sock.sendto(packet.encode('utf-8'), (SERVER_IP, SERVER_PORT))
                    print("Registration packet sent.")

                    # Потік для прослуховування відповідей
                    thread_listen = threading.Thread(target=listen, args=(sock,), daemon=True)
                    thread_listen.start()

                    # Потік для періодичних keep-alive
                    thread_keepalive = threading.Thread(target=keep_alive, args=(sock,), daemon=True)
                    thread_keepalive.start()

                    udp_threads_started = True
                except (socket.gaierror, OSError) as e:
                    print(f"UDP socket error: {e}. Керування тільки через CRSF.")
                    sock = None

            # PPM update thread always starts once, regardless of UDP/network
            if not hasattr(main, "ppm_thread_started"):
                thread_ppm = threading.Thread(target=ppm_update, daemon=True)
                thread_ppm.start()
                main.ppm_thread_started = True

            # OLED thread always starts once, regardless of UDP/network
            if not hasattr(main, "oled_thread_started"):
                thread_oled = threading.Thread(target=oled_update, daemon=True)
                thread_oled.start()
                main.oled_thread_started = True

            # Потік для читання з CRSF завжди стартує
            if not hasattr(main, "crsf_thread_started"):
                thread_crsf = threading.Thread(target=crsf_read, args=(crsf,))
                thread_crsf.start()
                main.crsf_thread_started = True

            if should_exit:
                print("Exiting by restart command")
                should_exit_oled = True
                # Wait a bit for the OLED thread to finish
                time.sleep(0.3)
                try:
                    with canvas(device) as draw:
                        draw.rectangle((0, 0, device.width, device.height), outline=0, fill=0x00)
                        draw.text((10, 10), "Rebooting...", font=font, fill=255)
                except Exception as e:
                    print(f"OLED error: {e}")
                if sock:
                    sock.close()
                break
            # Періодично пробуємо створити сокет, якщо інтернет з'явився
            if sock is None and not udp_threads_started:
                time.sleep(5)
            else:
                time.sleep(1)
        except KeyboardInterrupt:
            print("Client stopped.")
            if sock:
                sock.close()
            break
    import sys
    sys.exit(0)

if __name__ == "__main__":
    main()