# WireGuard + Drone Integration для Luckfox Pico

## 🚀 Швидка інструкція

### 1. Збірка
```bash
./build.sh media
./build.sh all
```

### 2. Перше налаштування VPN
```bash
# Генерація ключів на пристрої
/usr/bin/generate-wg-keys.sh

# Налаштування конфігурації
vi /etc/wireguard/wg0.conf

# Запуск VPN
/etc/init.d/S10wireguard start
```

### 3. Автоматичний запуск
```bash
# Запускає VPN та перевіряє Drone
setup_drone.sh
```

## 📁 Файли у прошивці

```
/etc/init.d/S10wireguard    # VPN (запускається першим)
/etc/init.d/S99drone        # Drone (запускається після VPN)
/etc/wireguard/wg0.conf      # Конфігурація WireGuard
/usr/bin/wireguard-setup.sh # Управління VPN
/usr/bin/generate-wg-keys.sh  # Генерація ключів
/var/log/wireguard.log       # Логи VPN
/var/log/drone.log           # Логи Drone
```

## ⚡ Автозапуск

**Порядок завантаження:**
1. **S10wireguard** → VPN запускається рано
2. **S99drone** → Drone запускається після VPN

**Перевірка автозапуску:**
```bash
# VPN працює?
/etc/init.d/S10wireguard status

# Drone працює?
/etc/init.d/S99drone status

# Обидва сервіси разом
setup_drone.sh
```

## 🔧 Конфігурація WireGuard

```ini
# /etc/wireguard/wg0.conf
[Interface]
Address = 10.8.0.2/24
PrivateKey = ВАШ_ПРИВАТНИЙ_КЛЮЧ
ListenPort = 51820

[Peer]
PublicKey = ПУБЛІЧНИЙ_КЛЮЧ_СЕРВЕРА
AllowedIPs = 10.8.0.0/24
Endpoint = IP_СЕРВЕРА:51820
PersistentKeepalive = 25
```

## 🐛 Проблеми та рішення

### VPN не запускається
```bash
# Перевірка наявності інструментів
which wg
which ip

# Перевірка конфігурації
cat /etc/wireguard/wg0.conf

# Запуск вручну
/usr/bin/wireguard-setup.sh start
```

### Drone не запускається
```bash
# Перевірка статусу VPN
/etc/init.d/S10wireguard status

# Запуск без VPN
/usr/bin/drone &

# Перевірка логів
cat /var/log/drone.log
```

### Мережа не готова
```bash
# Очікування мережі
sleep 10
/etc/init.d/S10wireguard start

# Перевірка інтерфейсів
ip addr show
```

## 📝 Важливі моменти

1. **Без SystemD:** Використовуємо SysV init (BusyBox)
2. **Пріоритети:** S10 → S99 для правильної послідовності
3. **Автоперезапуск:** VPN автоматично відновлює з'єднання
4. **Логування:** Усі події записуються в лог-файли

## 🔄 Діагностика

```bash
# Статус інтерфейсу
ip addr show wg0
wg show wg0

# Тест з'єднання
ping -c 3 10.8.0.1

# Перевірка маршрутів
ip route | grep wg0

# Логи системи
dmesg | grep wireguard
```

Ця інтеграція гарантує, що VPN буде готовий до того, як почне працювати додаток Drone!