# Як додати файли до /etc/ в прошивку Luckfox Pico (BusyBox)

## Спосіб 1: Через media додаток (рекомендовано)

Файли автоматично додаються до `/etc/` коли ви копіюєте їх в `media/your_app/out/etc/`:

1. **Додайте файли до Makefile:**
   ```makefile
   @mkdir -p $(YOUR_APP_OUT_DIR)/etc
   @cp -af $(CURRENT_DIR)/your_config.conf $(YOUR_APP_OUT_DIR)/etc/
   ```

2. **Файли будуть скопійовані в rootfs:**
   - `media/your_app/out/etc/` → `/etc/` в прошивці

## Спосіб 2: Через project/app/output/root/

Створіть файли в `project/app/output/root/`:

```
project/app/output/root/
└── etc/
    ├── your_config.conf
    ├── init.d/
    │   └── S99your_service    # <-- Важливо: префікс S99 для автозапуску
    └── default/
        └── your_settings
```

## Спосіб 3: Через custom_root

Створіть файли в `custom_root/` в корені проекту:

```
custom_root/
└── etc/
    ├── your_config.conf
    └── init.d/
        └── S99your_service      # <-- Автозапуск
```

## Автозапуск через SysV Init (BusyBox)

**Важливо:** Luckfox Pico використовує BusyBox з SysV init, НЕ SystemD!

1. **Створіть init скрипт з префіксом S99:**
   ```bash
   #!/bin/sh
   # Файл повинен мати назву: S99your_app
   
   case "$1" in
       start)  
           echo "Starting your_app..."
           mkdir -p /var/run /var/log
           /usr/bin/your_app >> /var/log/your_app.log 2>&1 &
           echo $! > /var/run/your_app.pid
           ;;
       stop)
           if [ -f /var/run/your_app.pid ]; then
               kill $(cat /var/run/your_app.pid)
               rm -f /var/run/your_app.pid
           fi
           pkill your_app
           ;;
       restart)
           $0 stop
           sleep 2
           $0 start
           ;;
       status)
           if [ -f /var/run/your_app.pid ]; then
               PID=$(cat /var/run/your_app.pid)
               if kill -0 $PID 2>/dev/null; then
                   echo "your_app is running (PID: $PID)"
               else
                   echo "your_app is not running (stale PID file)"
               fi
           else
               echo "your_app is not running"
           fi
           ;;
       *)
           echo "Usage: $0 {start|stop|restart|status}"
           exit 1
           ;;
   esac
   ```

2. **Скопіюйте через media додаток:**
   ```makefile
   @mkdir -p $(YOUR_APP_OUT_DIR)/etc/init.d
   @cp -af $(CURRENT_DIR)/S99your_app $(YOUR_APP_OUT_DIR)/etc/init.d/
   ```

3. **Префікс S99 означає:**
   - `S` = Start (запустити при завантаженні)
   - `99` = Запустити останнім (після всіх системних сервісів)
   - Автоматично обробляється SysV init

## Важливі моменти для Luckfox Pico

1. **Немає SystemD:** Не використовуйте .service файли
2. **Префікси імен:** Для автозапуску використовуйте `S99`
3. **Права доступу:** Скрипти повинні бути виконуваними (`chmod +x`)
4. **BusyBox:** Обмежений набір команд у порівнянні з повним Linux
5. **Шляхи:** Використовуйте абсолютні шляхи в скриптах

## Конфігураційні файли

Додавайте конфігураційні файли в `/etc/`:
```makefile
@mkdir -p $(YOUR_APP_OUT_DIR)/etc
@cp -af $(CURRENT_DIR)/your_app.conf $(YOUR_APP_OUT_DIR)/etc/
```

## Приклад з додатком Drone

Ваш додаток `drone` вже правильно налаштований:
- ✅ Сервіс: `/etc/init.d/S99drone` (автозапуск)
- ✅ Конфігурація: `/etc/drone.conf`
- ✅ Скрипт налаштування: `/usr/bin/setup_drone.sh`
- ✅ Журнал: `/var/log/drone.log`
- ✅ PID файл: `/var/run/drone.pid`

**Запуск на пристрої:**
```bash
# Автоматично при завантаженні (через S99 префікс)
# Або вручну:
/etc/init.d/S99drone start

# Перевірка статусу:
/etc/init.d/S99drone status

# Перегляд логів:
cat /var/log/drone.log
```

## Перевірка встановлених файлів

Після збірки `./build.sh media`, перевірте:
```bash
find output/out/media_out/etc -type f
```

## Важливі моменти

1. **Права доступу:** Використовуйте `chmod +x` для виконуваних файлів
2. **Шляхи:** Переконайтесь, що шляхи в скриптах абсолютні
3. **Залежності:** Перевіряйте залежності через `ldd /usr/bin/your_app`
4. **Логи:** Створюйте лог-файли в `/var/log/` з необхідними правами

## Приклад з Drone

Ваш додаток `drone` вже правильно інтегрований:
- Бінарник: `/usr/bin/drone`
- Конфігурація: `/etc/drone.conf`
- Сервіс SystemD: `/etc/systemd/system/drone.service`
- Сервіс Init.d: `/etc/init.d/drone`
- Скрипт налаштування: `/usr/bin/setup_drone.sh`

Автоматично активується командою `setup_drone.sh` на пристрої.