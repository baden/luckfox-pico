# UART Baudrate Fix

## Проблема

Помилка `cfsetispeed/cfsetospeed: Invalid argument` виникає тому що 420000 baud не є стандартною швидкістю для UART в Linux.

## Рішення

Реалізовано 3-рівневу систему fallback:

### 1. Спроба custom baudrate (найкраще)
```c
// Використовує ioctl для встановлення custom divisor
struct serial_struct ser;
ioctl(fd, TIOCGSERIAL, &ser);
ser.flags |= ASYNC_SPD_CUST;
ser.custom_divisor = ser.baud_base / baudrate;
ioctl(fd, TIOCSSERIAL, &ser);
// Встановлюємо B38400 + custom divisor
cfsetispeed(&tty, B38400);
```

### 2. Fallback до стандартної швидкості
Якщо custom не працює, використовуємо найближчу стандартну швидкість.

### 3. Graceful error handling
Якщо все інше не працює, додаток продовжує працювати з повідомленням про помилку.

## Тестування

### Запуск тесту:
```bash
./test_uart.sh
```

### Запуск основного додатку:
```bash
sudo ./build/src/rv1106_ipc/drone
```

## Очікувана поведінка

1. **На підтримуваному обладнанні** - custom baudrate працює
2. **На стандартних системах** - fallback до 38400 або іншої стандартної швидкості
3. **Без UART пристрою** - повідомлення про відсутність пристрою, але додаток продовжує працювати

## CRSF протокол

CRSF зазвичай використовує 420000 baud, але може працювати на інших швидкостях:
- **420000 bps** - оптимальна (високий throughput)
- **38400 bps** - fallback (нижчий throughput)
- **115200 bps** - стандартний fallback

## Відмінності від Python версії

Python використовує бібліотеку `pyserial` яка автоматично обробляє custom baudrates. В C реалізації потрібно вручну працювати з ioctl та serial_struct.

## Моніторинг

Додаток логує:
- "Successfully set custom baudrate: 420000 (divisor: X)"
- "Using standard baudrate: 420000"
- "CRSF disconnected, attempting to reconnect..."

Це дозволяє легко діагностувати проблеми з UART з'єднанням.