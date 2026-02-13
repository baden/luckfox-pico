let gamepad = null;
let requestAnimationFrameId = null;
let ws = null;
let reconnectTimeout = null;
let wsConnected = false;
let droneId = localStorage.getItem('droneId') || '';
let currentArmChannel = localStorage.getItem('armChannel') || 'disabled';

// Status Management
function updateSystemStatus(text, isError) {
    const statusBar = document.getElementById('status-bar');
    if (!statusBar) return;

    if (isError) {
        statusBar.textContent = text;
        statusBar.style.display = 'block';
        statusBar.style.backgroundColor = 'rgba(255, 0, 0, 0.8)';
    } else {
        // If not error (e.g. connected), hide after a short delay or immediately
        statusBar.textContent = text;
        statusBar.style.backgroundColor = 'rgba(0, 128, 0, 0.8)';
        setTimeout(() => {
            if (statusBar.textContent === text) { // Only hide if text hasn't changed to an error
                statusBar.style.display = 'none';
            }
        }, 3000);
    }
}

function setGamepadStatus(text, isConnected) {
    console.log("Gamepad Status:", text, isConnected);
    const diagStatus = document.getElementById('gamepad-status');
    if (diagStatus) {
        diagStatus.textContent = text;
        diagStatus.style.color = isConnected ? '#4CAF50' : '#f44336';
    }
    
    // Only show in main status bar if NOT connected
    if (!isConnected) {
        updateSystemStatus("Gamepad Disconnected", true);
    } else {
        updateSystemStatus("Gamepad Connected", false);
    }
}

function setConnectionStatus(text, isError = false) {
    const diagStatus = document.getElementById('connection-status');
    if (diagStatus) {
        diagStatus.textContent = text;
        diagStatus.style.color = isError ? '#f44336' : '#4CAF50';
    }
    
    // Only show in main status bar if error
    if (isError) {
        updateSystemStatus(text, true);
    } else if (text.includes("Connected") || text.includes("підключено")) {
         updateSystemStatus(text, false);
    }
}

// Global object to store current drone settings
let droneSettings = {};

function renderSettings(settings) {
    const listDiv = document.getElementById('settings-list');
    const saveContainer = document.getElementById('save-settings-container');
    
    listDiv.innerHTML = ''; // Clear current list
    droneSettings = settings; // Store for diffing later if needed

    for (const [key, value] of Object.entries(settings)) {
        const itemDiv = document.createElement('div');
        itemDiv.className = 'setting-item';

        const label = document.createElement('label');
        label.innerText = key;
        label.htmlFor = 'setting-' + key;

        const input = document.createElement('input');
        input.id = 'setting-' + key;
        input.dataset.key = key; // Store key for easy retrieval
        
        // Determine input type
        if (typeof value === 'number') {
            input.type = 'number';
            input.step = '0.001'; // Default step, maybe refine based on key
            if (key.includes('port')) input.step = '1';
            
            // Fix precision for floating point display
            // Round to 3 decimal places and remove trailing zeros
            if (!Number.isInteger(value)) {
                input.value = parseFloat(value.toFixed(3));
            } else {
                input.value = value;
            }
        } else {
            input.type = 'text';
            input.value = value;
        }

        itemDiv.appendChild(label);
        itemDiv.appendChild(input);
        listDiv.appendChild(itemDiv);
    }
    
    saveContainer.style.display = 'block';
}

function collectAndSendSettings() {
    const inputs = document.querySelectorAll('#settings-list input');
    const newSettings = {};
    let changed = false;

    inputs.forEach(input => {
        const key = input.dataset.key;
        let value = input.value;
        
        // Type conversion
        if (input.type === 'number') {
            value = parseFloat(value);
        }
        
        // Only send changed or all? Sending subset is safer for bandwidth, 
        // but backend applies all provided. Let's send non-nulls.
        // For CJSON backend: {"settings": {"key": val}}
        // Let's send everything we see in the form to ensure state consistency, 
        // or just changed ones.
        // Let's check diff against droneSettings
        if (droneSettings[key] !== value) {
            newSettings[key] = value;
            changed = true;
        }
    });

    if (changed) {
        if (wsConnected) {
            console.log("Sending settings update:", newSettings);
            ws.send(JSON.stringify({ settings: newSettings }));
            alert("Налаштування відправлено!");
            // Update local cache
            Object.assign(droneSettings, newSettings);
        } else {
            alert("Немає з'єднання з дроном!");
        }
    } else {
        alert("Немає змін для збереження.");
    }
}

function connectWebSocket() {
    setConnectionStatus('Підключення до WebSocket-сервера...');
    // Connect to the drone's IP on port 80 (implied), using current hostname
    const host = window.location.hostname || '10.8.3.2'; // Fallback if local file
    ws = new WebSocket(`ws://${host}`);

    ws.onopen = () => {
        wsConnected = true;
        setConnectionStatus('WebSocket підключено!');
        // No initial handshake command needed for the C server
    };

    ws.onmessage = (event) => {
        try {
            const msg = JSON.parse(event.data);
            
            // Check if it's a settings response (contains specific keys or simple heuristic)
            // Backend sends flat JSON. Telemetry has 'armed', 'r', 'p'. 
            // Settings has 'udp_host', 'steering_damping', etc.
            
            if (msg.udp_host !== undefined || msg.steering_damping !== undefined) {
                console.log("Received Settings:", msg);
                renderSettings(msg);
                return;
            }

            // console.log("Telem:", msg);
            if (msg.armed !== undefined) {
                // Update UI with telemetry if elements exist
                const statusText = `ARM: ${msg.armed ? 'ON' : 'OFF'} | R: ${msg.r.toFixed(2)} | P: ${msg.p.toFixed(2)}`;
                // You might want to display this somewhere
                // setConnectionStatus(statusText); // Optional: override status or add new element
            }
        } catch (e) {
            console.error("Error parsing telemetry", e);
        }
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
    // console.log("gamepad.axes", gamepad.axes);
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
                /*E:*/ (gamepad.axes[4]>0.5)?1:0, //gamepad.buttons[0].pressed?1:0,
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
            const processedData = prepareData(gamepad);

            // Determine ARM state based on configuration (using raw gamepad input)
            let armState = false;
            if (currentArmChannel === 'disabled') {
                armState = false;
            } else if (currentArmChannel.startsWith('btn-')) {
                const btnIndex = parseInt(currentArmChannel.split('-')[1]);
                if (gamepad.buttons[btnIndex]) {
                    armState = gamepad.buttons[btnIndex].pressed;
                }
            } else if (currentArmChannel.startsWith('axis-')) {
                const axisIndex = parseInt(currentArmChannel.split('-')[1]);
                if (gamepad.axes[axisIndex] !== undefined) {
                    armState = gamepad.axes[axisIndex] > 0.5;
                }
            }

            const data = {
                arm: armState,
                // Legacy fields (still used by backend?)
                a0: processedData.axes[0],
                a1: processedData.axes[1],
                a2: processedData.axes[2],
                a3: processedData.axes[3],
                
                // New fields as requested
                axes: processedData.axes, 
                buttons: processedData.buttons
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

// Functions for Axis/Button UI Updates
function updateAxesDisplay() {
    if (!gamepad) return;
    const container = document.getElementById('axes-display');
    if (!container) return; // Container might be hidden/lazy loaded

    for (let i = 0; i < gamepad.axes.length; i++) {
        let axisDiv = document.getElementById(`axis${i}`);
        if (!axisDiv) {
            axisDiv = document.createElement('div');
            axisDiv.id = `axis${i}`;
            axisDiv.className = 'control-item axis-item';
            container.appendChild(axisDiv);
        }
        const axisValue = gamepad.axes[i];
        axisDiv.textContent = `A${i}: ${axisValue.toFixed(2)}`;
        
        // Visual indicator (border color or background gradient)
        const percent = Math.round((axisValue + 1) * 50); 
        axisDiv.style.background = `linear-gradient(to right, rgba(76, 175, 80, 0.4) ${percent}%, rgba(255,255,255,0.1) ${percent}%)`;
    }
}

function updateButtonsDisplay() {
    if (!gamepad) return;
    const container = document.getElementById('buttons-display');
    if (!container) return;

    for (let i = 0; i < gamepad.buttons.length; i++) {
        let buttonDiv = document.getElementById(`button${i}`);
        if (!buttonDiv) {
            buttonDiv = document.createElement('div');
            buttonDiv.id = `button${i}`;
            buttonDiv.className = 'control-item button-item';
            container.appendChild(buttonDiv);
        }
        const button = gamepad.buttons[i];
        buttonDiv.textContent = `B${i}`;
        
        if (button.pressed) {
            buttonDiv.style.background = '#4CAF50';
            buttonDiv.style.color = 'white';
        } else {
            buttonDiv.style.background = 'rgba(255,255,255,0.1)';
            buttonDiv.style.color = '#aaa';
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
    const armChannelSelect = document.getElementById('arm-channel');

    // Populate Arm Channel Select
    if (armChannelSelect) {
        // Buttons 0-15
        for (let i = 0; i < 16; i++) {
            const opt = document.createElement('option');
            opt.value = `btn-${i}`;
            opt.textContent = `Button ${i}`;
            armChannelSelect.appendChild(opt);
        }
        // Axes 0-7
        for (let i = 0; i < 8; i++) {
            const opt = document.createElement('option');
            opt.value = `axis-${i}`;
            opt.textContent = `Axis ${i} (>0.5)`;
            armChannelSelect.appendChild(opt);
        }
        
        // Load saved value
        armChannelSelect.value = currentArmChannel;

        // Save on change
        armChannelSelect.addEventListener('change', () => {
            currentArmChannel = armChannelSelect.value;
            localStorage.setItem('armChannel', currentArmChannel);
            console.log("ARM Channel set to:", currentArmChannel);
        });
    }

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

    // Request Settings Button
    document.getElementById('refresh-settings').addEventListener('click', () => {
        if (wsConnected) {
            console.log("Requesting settings...");
            ws.send(JSON.stringify({ get_settings: true }));
        } else {
            alert("Немає з'єднання з дроном!");
        }
    });

    // Save Settings Button
    document.getElementById('save-settings').addEventListener('click', () => {
        collectAndSendSettings();
    });

});


// ws.onopen, ws.onmessage, ws.onclose, ws.onerror тепер у connectWebSocket()
