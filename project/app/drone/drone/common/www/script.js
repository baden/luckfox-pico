let gamepad = null;
let requestAnimationFrameId = null;
let ws = null;
let reconnectTimeout = null;
let wsConnected = false;
let droneId = localStorage.getItem('droneId') || '';

function setGamepadStatus(text, isConneced) {
    console.log("setGamepadStatus:", text, isConneced);
    const statusDiv = document.getElementById('gamepad-status');
    statusDiv.textContent = text;
    statusDiv.style.color = isConneced ? 'black' : 'red';
    const iconDiv = document.getElementById('gamepad-icon');
    if (isConneced) {
        iconDiv.classList.remove('disconnected');
        iconDiv.classList.add('connected');
    } else {
        iconDiv.classList.remove('connected');
        iconDiv.classList.add('disconnected');
    }
}

function setConnectionStatus(text, isError = false) {
    const statusDiv = document.getElementById('connection-status');
    statusDiv.textContent = text;
    statusDiv.style.color = isError ? 'red' : 'black';
}

function connectWebSocket() {
    setConnectionStatus('Підключення до WebSocket-сервера...');
    ws = new WebSocket("wss://s.navi.cc/ws");

    ws.onopen = () => {
        wsConnected = true;
        setConnectionStatus('WebSocket підключено!');
        ws.send(JSON.stringify({ command: "connectd", id: navigator.userAgent }));
    };

    ws.onmessage = (event) => {
        // console.log("Received:", event.data);
    };

    ws.onclose = (event) => {
        wsConnected = false;
        setConnectionStatus('Втрачено зʼєднання з WebSocket. Перепідключення через 5 секунд...', true);
        if (!reconnectTimeout) {
            reconnectTimeout = setTimeout(() => {
                reconnectTimeout = null;
                connectWebSocket();
            }, 5000);
        }
    };

    ws.onerror = (error) => {
        wsConnected = false;
        setConnectionStatus('Помилка WebSocket! Перепідключення через 5 секунд...', true);
        if (!reconnectTimeout) {
            reconnectTimeout = setTimeout(() => {
                reconnectTimeout = null;
                connectWebSocket();
            }, 5000);
        }
    };
}

connectWebSocket();

// Не даємо пристрою засинати (Android)
if ('wakeLock' in navigator) {
    let wakeLock = null;
    async function requestWakeLock() {
        try {
            wakeLock = await navigator.wakeLock.request('screen');
            wakeLock.addEventListener('release', () => {
                console.log('Wake Lock was released');
            });
            console.log('Wake Lock is active');
        } catch (err) {
            console.error('Wake Lock error:', err);
        }
    }
    document.addEventListener('visibilitychange', () => {
        if (wakeLock !== null && document.visibilityState === 'visible') {
            requestWakeLock();
        }
    });
    requestWakeLock();
}

// Мапінг осей і кнопок Radiomaster TX12
// Осьові індекси можуть відрізнятися в залежності від браузера і ОС
// Нижче наведено типовий мапінг для Radiomaster TX12

// Комбінацію будемо визначати по кількості осей і кнопок

// ---------------------
// Комбінація для [7,24]

// Осьові індекси:
// 0: RH, Правий стик горизонталь
// 1: RV, Правий стик вертикаль
// 2: LV, Лівий стик вертикаль
// 3: LH, Лівий стик горизонталь
// 4: Повторює Лівий стик горизонталь
// 5: S1 (ліве колесо)
// 6: S2 (праве колесо)

// Індекси кнопок:
// 0: E, Лівий дальній перемикач
// 1: A, Ліва кнопка
// 3: B, Ливій ближній перемикач
// 4: D, Права кнопка
// 6: F, Правий дальній перемикач
// 7: С, Правий ближній перемикач
// 8: S1 (дубль, дискретний)
// 9: S2 (дубль, дискретний)

// ---------------------
// Комбінація для [4,17]

// Осьові індекси:
// 0: RH, Правий стик горизонталь
// 1: RV, Правий стик вертикаль
// 2: LV, Лівий стик вертикаль
// 3: LH, Лівий стик горизонталь

// Індекси кнопок:
// 0: E, Лівий дальній перемикач
// 1: A, Ліва кнопка
// 2: B, Ливій ближній перемикач
// 3: D, Права кнопка
// 4: F, Правий дальній перемикач
// 5: С, Правий ближній перемикач
// 6: S1 (дискретний)
// 7: S2 ( дискретний)

function prepareData(gamepad) {
    if (!gamepad) return null;
    const round3 = v => Math.round(v * 1000) / 1000;
    if (gamepad.axes.length === 7 && gamepad.buttons.length === 24) {
        // Комбінація для [7,24]
        return {
            axes: [
                /*RH:*/ round3(gamepad.axes[0]),
                /*RV:*/ round3(gamepad.axes[1]),
                /*LV:*/ round3(gamepad.axes[2]),
                /*LH:*/ round3(gamepad.axes[3]),
                /*S1_wheel:*/ round3(gamepad.axes[5]),
                /*S2_wheel:*/ round3(gamepad.axes[6])
            ],
            buttons: [
                /*E:*/ gamepad.buttons[0].pressed?1:0,
                /*A:*/ gamepad.buttons[1].pressed?1:0,
                /*B:*/ gamepad.buttons[3].pressed?1:0,
                /*D:*/ gamepad.buttons[4].pressed?1:0,
                /*F:*/ gamepad.buttons[6].pressed?1:0,
                /*C:*/ gamepad.buttons[7].pressed?1:0,
                /*S1:*/ gamepad.buttons[8].pressed?1:0,
                /*S2:*/ gamepad.buttons[9].pressed?1:0
            ]
        };
    } else if (gamepad.axes.length === 4 && gamepad.buttons.length === 17) {
        // Комбінація для [4,17]
        return {
            axes: [
                /*RH:*/ round3(gamepad.axes[0]),
                /*RV:*/ round3(gamepad.axes[1]),
                /*LV:*/ round3(gamepad.axes[2]),
                /*LH:*/ round3(gamepad.axes[3]),
                /*S1_wheel:*/ 0.0,
                /*S2_wheel:*/ 0.0
            ],
            buttons: [
                /*E:*/ gamepad.buttons[0].pressed?1:0,
                /*A:*/ gamepad.buttons[1].pressed?1:0,
                /*B:*/ gamepad.buttons[2].pressed?1:0,
                /*D:*/ gamepad.buttons[3].pressed?1:0,
                /*F:*/ gamepad.buttons[4].pressed?1:0,
                /*C:*/ gamepad.buttons[5].pressed?1:0,
                /*S1:*/ gamepad.buttons[6].pressed?1:0,
                /*S2:*/ gamepad.buttons[7].pressed?1:0
            ]
        };
    } else {
        // Невідома комбінація
        return {
            axes: gamepad.axes.map(axis => round3(axis)),
            buttons: gamepad.buttons.map(button => (button.pressed?1:0)) // Надсилаємо 1 або 0 замість true/false
        };
    }

}


// Змінні для керування частотою відправки
let lastAxes = [];
let lastButtons = [];
let lastSendTime = 0;
const sendInterval = 1000; // 1000 мс

function isGamepadChanged() {
    if (!gamepad) return false;
    // Перевірка осей
    for (let i = 0; i < gamepad.axes.length; i++) {
        if (lastAxes[i] === undefined || Math.abs(gamepad.axes[i] - lastAxes[i]) > 0.003) {
            return true;
        }
    }
    // Перевірка кнопок
    for (let i = 0; i < gamepad.buttons.length; i++) {
        if (lastButtons[i] === undefined || gamepad.buttons[i].pressed !== lastButtons[i].pressed || Math.abs(gamepad.buttons[i].value - lastButtons[i].value) > 0.001) {
            return true;
        }
    }
    return false;
}

// Функція для оновлення статусу джойстика
function updateGamepadStatus() {
    if (gamepad) {
        // setConnectionStatus(`Підключено: ${gamepad.id} (Індекс: ${gamepad.index})`);
        updateAxesDisplay();
        updateButtonsDisplay();

        const currentTime = Date.now();
        let needSend = false;
        let ping = false;
        if (isGamepadChanged()) {
            needSend = true;
            ping = false;
        } else if (currentTime - lastSendTime > sendInterval) {
            needSend = true;
            ping = true;
        }
        if (wsConnected && ws && ws.readyState === WebSocket.OPEN && needSend) {
            const data = {
                command: "joy_update",
                ping: ping,
                id: droneId,
                data: prepareData(gamepad)
            };
            ws.send(JSON.stringify(data));
            lastSendTime = currentTime;
            lastAxes = gamepad.axes.slice();
            lastButtons = gamepad.buttons.map(b => ({ pressed: b.pressed, value: b.value }));
        }
    } else {
        setConnectionStatus('Очікування підключення джойстика...');
    }
}

// Функція для оновлення відображення осей
function updateAxesDisplay() {
    if (!gamepad) return;
    for (let i = 0; i < gamepad.axes.length; i++) {
        let axisDiv = document.getElementById(`axis${i}`);
        if (!axisDiv) {
            axisDiv = document.createElement('div');
            axisDiv.id = `axis${i}`;
            axisDiv.className = 'control-item axis-item';
            document.getElementById('axes-display').appendChild(axisDiv);
        }
        const axisValue = gamepad.axes[i];
        axisDiv.textContent = `${i}: ${axisValue.toFixed(2)}`;
        // Візуалізація заповнення
        const percent = Math.round((axisValue + 1) * 50); // -1 -> 0%, 0 -> 50%, 1 -> 100%
        axisDiv.style.background = `linear-gradient(to right, #4fc3f7 ${percent}%, #f7f7f7 ${percent}%)`;
    }
}

// Функція для оновлення відображення кнопок
function updateButtonsDisplay() {
    if (!gamepad) return;
    for (let i = 0; i < gamepad.buttons.length; i++) {
        let buttonDiv = document.getElementById(`button${i}`);
        if (!buttonDiv) {
            buttonDiv = document.createElement('div');
            buttonDiv.id = `button${i}`;
            buttonDiv.className = 'control-item button-item';
            document.getElementById('buttons-display').appendChild(buttonDiv);
        }
        const button = gamepad.buttons[i];
        buttonDiv.textContent = `${i}: ${button.pressed ? '+' : '-'}`;
        if (button.pressed) {
            buttonDiv.style.background = 'linear-gradient(to right, #4fc3f7 100%, #f7f7f7 0%)';
        } else {
            buttonDiv.style.background = 'linear-gradient(to right, #f7f7f7 0%, #f7f7f7 100%)';
        }
    }
}

// Основний цикл для опитування джойстика
function gameLoop() {
    // console.log("gamepad:", gamepad);
    const gamepads = navigator.getGamepads();
    // let gamepad_ = null;
    for(const gp of gamepads) {
        // console.log("gp:", gp);
        if (gp && (gp.id.includes("Vendor: 1209 Product: 4f54") || gp.id.includes("Radiomaster TX12 Joystick"))) {
            gamepad = gp;
            // console.log("gamepad:", gamepad);
            break;
        }
    }
    // console.log("gamepads:", [gamepads]);
    // if (gamepads.length > 0) {
    //     // Ми припускаємо, що нас цікавить перший підключений джойстик
    //     // Можливо, вам потрібно буде додати логіку для вибору конкретного джойстика за Vendor/Product ID
    //     gamepad = gamepads[0]; // або шукати по gamepad.id, щоб знайти ваш Radiomaster
    // } else {
    //     gamepad = null;
    // }

    updateGamepadStatus();
    requestAnimationFrameId = requestAnimationFrame(gameLoop);
}

// Обробник події підключення джойстика
window.addEventListener("gamepadconnected", (event) => {
    console.log("Gamepad", [event.gamepad]);
    console.log("Gamepad connected at index %d: %s. %d buttons, %d axes.",
        event.gamepad.index, event.gamepad.id,
        event.gamepad.buttons.length, event.gamepad.axes.length);

    // Перевірка на Vendor/Product ID
    // Radiomaster TX12 Joystick: Vendor: 1209 Product: 4f54
    // Деякі браузери можуть надавати ідентифікатор у форматі "Vendor_XXXX_Product_YYYY"
    // або просто текстовий опис. Вам потрібно буде перевірити формат у вашому браузері.
        if (event.gamepad.id.includes("Vendor: 1209 Product: 4f54") || event.gamepad.id.includes("Radiomaster TX12 Joystick")) { // Або інший спосіб перевірки
            console.log("Radiomaster TX12 підключено.", event.gamepad);
            gamepad = event.gamepad;
            // Обрізаємо все в дужках разом з дужками
            const shortName = gamepad.id.replace(/\s*\([^)]*\)/g, "").trim();
            setGamepadStatus(`Підключено: ${shortName}`, true);
            if (!requestAnimationFrameId) { // Запускаємо цикл, якщо він ще не запущений
                requestAnimationFrameId = requestAnimationFrame(gameLoop);
            }
    } else {
        console.log("Інший джойстик підключено, але не Radiomaster TX12.");
    }
});

// Обробник події відключення джойстика
window.addEventListener("gamepaddisconnected", (event) => {
    setGamepadStatus('Джойстик відключено!', false);
    // console.log("Gamepad disconnected from index %d: %s",
    //     event.gamepad.index, event.gamepad.id);
    if (gamepad && gamepad.index === event.gamepad.index) {
        gamepad = null;
        cancelAnimationFrame(requestAnimationFrameId);
        requestAnimationFrameId = null;
        updateGamepadStatus();
    }
});

// Початковий запуск перевірки джойстика, якщо він вже підключений при завантаженні сторінки
// Це може бути не потрібно, якщо ви покладаєтеся тільки на gamepadconnected
const initialGamepads = navigator.getGamepads();
if (initialGamepads.length > 0) {
    // Перевірте, чи є серед них ваш Radiomaster
    // console.log("gpamepads:", [initialGamepads]);
    for (const gp of initialGamepads) {
        if (gp && (gp.id.includes("Vendor_1209_Product_4f54") || gp.id.includes("RadioMaster TX12 Joystick"))) {
            gamepad = gp;
            break;
        }
    }
    if (gamepad && !requestAnimationFrameId) {
        requestAnimationFrameId = requestAnimationFrame(gameLoop);
    }
} else {
    updateGamepadStatus();
}


// Відкриття/закриття модального вікна налаштувань + LocalStorage для назви дрона
document.addEventListener('DOMContentLoaded', function() {
    const settingsBtn = document.getElementById('settings-btn');
    const settingsModal = document.getElementById('settings-modal');
    const closeSettings = document.getElementById('close-settings');
    const droneIdInput = document.getElementById('drone-id');

    // Завантажити назву дрона з LocalStorage при старті
    if (droneIdInput) {
        droneIdInput.value = droneId;
    }

    settingsBtn.addEventListener('click', function() {
        settingsModal.style.display = 'block';
        // Під час відкриття модального — оновити поле з LocalStorage
        if (droneIdInput) {
            droneIdInput.value = localStorage.getItem('droneId') || '';
        }
    });
    closeSettings.addEventListener('click', function() {
        settingsModal.style.display = 'none';
    });
    window.addEventListener('click', function(event) {
        if (event.target === settingsModal) {
            settingsModal.style.display = 'none';
        }
    });

    // Зберігати назву дрона при зміні та оновлювати глобальну змінну
    if (droneIdInput) {
        droneIdInput.addEventListener('input', function() {
            localStorage.setItem('droneId', droneIdInput.value);
            droneId = droneIdInput.value;
        });
    }


    // Обробник кнопки перезапуску дрона
    document.getElementById('restart-drone').addEventListener('click', () => {
        if (wsConnected && ws && ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({ command: "restart" }));
            // alert("Команда перезапуску дрона відправлена.");
        } else {
            // alert("WebSocket не підключено. Неможливо відправити команду.");
        }
    });

});


// ws.onopen, ws.onmessage, ws.onclose, ws.onerror тепер у connectWebSocket()
