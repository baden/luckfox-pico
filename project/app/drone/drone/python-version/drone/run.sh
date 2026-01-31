#!/bin/bash

export PATH="/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin:$PATH"

# --- Налаштування ---
# URL до директорії з оновленнями на сервері
SERVER_URL="http://s.navi.cc/"

# Шлях для розміщення проєкту на платі
LOCAL_PROJECT_DIR="/root"

# Назва головного Python-скрипта для запуску
MAIN_SCRIPT="drone.py"

# --- Кінець налаштувань ---


# Функція, яка буде викликана при отриманні сигналу завершення
cleanup() {
    echo "Отримано сигнал завершення. Зупиняю дочірній процес Python..."
    # Якщо змінна PYTHON_PID існує, вбиваємо процес
    if [ -n "$PYTHON_PID" ]; then
        # kill надсилає SIGTERM, що дозволяє Python коректно завершитись
        kill "$PYTHON_PID"
    fi
    # Виходимо зі скрипта
    exit 0
}

# Встановлюємо "пастку" (trap) на сигнали SIGINT (Ctrl+C) та SIGTERM (стандартний для kill/stop)
# Коли скрипт отримає один з цих сигналів, він викличе функцію cleanup
trap 'cleanup' SIGINT SIGTERM

# Створюємо директорію для проєкту, якщо її не існує
# mkdir -p "$LOCAL_PROJECT_DIR"
sleep 2 # Даємо пару секунд на підняття мережі
cd "$LOCAL_PROJECT_DIR" || { echo "Не вдалося перейти до директорії $LOCAL_PROJECT_DIR"; exit 1; }

# Нескінченний цикл, який забезпечує перезапуск
while true; do
    echo "==================================="
    echo "Починаю цикл перевірки та запуску..."

    # 1. ПЕРЕВІРКА ОНОВЛЕНЬ
    echo "[1/3] Перевірка наявності оновлень..."

    # Отримуємо локальну версію. Якщо файлу немає, версія буде "0".
    LOCAL_VERSION_FILE="$LOCAL_PROJECT_DIR/version.txt"
    if [ -f "$LOCAL_VERSION_FILE" ]; then
        LOCAL_VERSION=$(cat "$LOCAL_VERSION_FILE")
    else
        LOCAL_VERSION="0"
    fi

    # Завантажуємо версію з сервера. -q (тихий режим), -O - (вивід в stdout)
    REMOTE_VERSION=$(wget -q -O - "$SERVER_URL/version.txt")

    # Перевіряємо, чи вдалося завантажити версію
    if [ -z "$REMOTE_VERSION" ]; then
        echo "Помилка: не вдалося отримати версію з сервера. Можливо, немає інтернету."
        echo "Пробую запустити наявну локальну версію..."
    # Порівнюємо версії
    elif [ "$REMOTE_VERSION" != "$LOCAL_VERSION" ]; then
        echo "Знайдено нову версію: $REMOTE_VERSION (локальна: $LOCAL_VERSION)."

        # Завантажуємо архів у тимчасову директорію
        ARCHIVE_PATH="/tmp/drone.tar.gz"
        echo "[2/3] Завантаження $SERVER_URL/drone.tar.gz та розпакування $ARCHIVE_PATH..."
        wget -q -O "$ARCHIVE_PATH" "$SERVER_URL/drone.tar.gz"

        # Перевіряємо, чи архів завантажився успішно (код виходу 0)
        if [ $? -eq 0 ]; then
            # Очищуємо стару версію проєкту
            # Конструкція "${LOCAL_PROJECT_DIR:?}"/* захищає від випадкового rm -rf /*
            rm -rf "${LOCAL_PROJECT_DIR:?}"/*

            # Розпаковуємо новий архів у директорію проєкту
            gunzip -c "$ARCHIVE_PATH" | tar xf - -C "$LOCAL_PROJECT_DIR"

            chmod +x "$LOCAL_PROJECT_DIR/run.sh"
            chmod +x /etc/init.d/S99drone_autostart_script

            # Зберігаємо нову версію локально
            echo "$REMOTE_VERSION" > "$LOCAL_VERSION_FILE"
            echo "Оновлення до версії $REMOTE_VERSION завершено успішно."

            # Видаляємо тимчасовий архів
            rm "$ARCHIVE_PATH"
        else
            echo "Помилка завантаження архіву з новою версією. Пропускаю оновлення."
        fi
    else
        echo "Локальна версія ($LOCAL_VERSION) є актуальною."
    fi

    # 2. ЗАПУСК PYTHON-СКРИПТА
    MAIN_SCRIPT_PATH="$LOCAL_PROJECT_DIR/$MAIN_SCRIPT"
    if [ -f "$MAIN_SCRIPT_PATH" ]; then
        echo "[3/3] Запускаю Python-скрипт: $MAIN_SCRIPT_PATH"
        # Запускаємо скрипт. Скрипт-супервізор буде чекати тут, доки Python-програма не завершиться.
        /usr/bin/python3 "$MAIN_SCRIPT_PATH" &

        # Зберігаємо PID щойно запущеного процесу
        PYTHON_PID=$!
        echo "Python-скрипт запущено з PID: $PYTHON_PID"

        # Чекаємо, доки дочірній процес (Python) не завершиться
        wait "$PYTHON_PID"
        EXIT_CODE=$?
        echo "Python-скрипт завершив роботу з кодом виходу: $EXIT_CODE."

        # Очищуємо змінну PID
        PYTHON_PID=""

    else
        echo "Помилка: головний скрипт $MAIN_SCRIPT_PATH не знайдено. Неможливо запустити."
    fi

    # 3. ПАУЗА ПЕРЕД ПОВТОРЕННЯМ
    # Це важливо, щоб уникнути миттєвого перезапуску і навантаження на систему,
    # якщо Python-скрипт одразу "падає" з помилкою.
    echo "Очікування 5 секунд перед наступною ітерацією..."
    sleep 5
done