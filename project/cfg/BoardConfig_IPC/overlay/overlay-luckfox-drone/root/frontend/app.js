document.addEventListener('DOMContentLoaded', () => {
    fetchInfo();
    fetchConfig();

    document.getElementById('btn-reboot').addEventListener('click', () => {
        if (confirm('Ви дійсно хочете завершити роботу програми дрона (Graceful Shutdown)?')) {
            rebootDrone();
        }
    });

    document.getElementById('btn-save-config').addEventListener('click', saveConfig);
    document.getElementById('btn-save-network').addEventListener('click', saveNetworkConfig);
    document.getElementById('btn-save-adc').addEventListener('click', saveAdcConfig);
});

async function fetchConfig() {
    try {
        const response = await fetch('/api/v1/config');
        if (!response.ok) throw new Error(`Помилка: ${response.status}`);
        
        const data = await response.json();
        
        if (data.control) {
            document.getElementById('input-curve').value = data.control.curve;
            document.getElementById('input-damping').value = data.control.damping;
        }
        if (data.mavlink) {
            document.getElementById('input-mavlink').value = data.mavlink.connection_string || '';
        }
        if (data.stream) {
            document.getElementById('input-stream').value = data.stream.video_dest || '';
        }
        if (data.adc && data.adc.channels) {
            renderAdcChannels(data.adc.channels);
        }
    } catch (error) {
        console.error('Failed to load config', error);
    }
}

function renderAdcChannels(channels) {
    const container = document.getElementById('adc-channels-container');
    container.innerHTML = '';
    
    if (!channels || channels.length === 0) {
        container.innerHTML = '<div class="text-slate-500 italic text-sm py-2">Немає налаштованих каналів ADC</div>';
        return;
    }

    channels.forEach((ch, idx) => {
        const div = document.createElement('div');
        div.className = 'adc-channel bg-slate-900/50 p-5 rounded-lg border border-slate-700/50 relative overflow-hidden';
        div.innerHTML = `
            <div class="absolute left-0 top-0 bottom-0 w-1 bg-blue-500/30"></div>
            <h4 class="text-white font-medium mb-4 flex items-center gap-2">
                <span class="bg-blue-500/20 text-blue-400 text-xs px-2 py-0.5 rounded">CH</span> 
                Канал ${ch.id}
            </h4>
            <input type="hidden" class="adc-id" value="${ch.id}">
            
            <div class="grid grid-cols-1 sm:grid-cols-3 gap-4">
                <div>
                    <label class="block text-xs font-medium text-slate-400 mb-1.5">Scale (Hardware):</label>
                    <input type="number" class="adc-scale w-full rounded-md px-3 py-2 text-sm focus:outline-none focus:border-blue-500 transition-colors" style="background-color: #1e293b; border: 1px solid #475569; color: white;" step="0.01" min="0" value="${ch.scale}">
                </div>
                <div>
                    <label class="block text-xs font-medium text-slate-400 mb-1.5">Min (0% Volts):</label>
                    <div class="relative">
                        <input type="number" class="adc-min w-full rounded-md pl-3 pr-8 py-2 text-sm focus:outline-none focus:border-blue-500 transition-colors" style="background-color: #1e293b; border: 1px solid #475569; color: white;" step="0.01" value="${ch.min}">
                        <span class="absolute right-3 top-1/2 -translate-y-1/2 text-slate-500 text-xs">V</span>
                    </div>
                </div>
                <div>
                    <label class="block text-xs font-medium text-slate-400 mb-1.5">Max (100% Volts):</label>
                    <div class="relative">
                        <input type="number" class="adc-max w-full rounded-md pl-3 pr-8 py-2 text-sm focus:outline-none focus:border-blue-500 transition-colors" style="background-color: #1e293b; border: 1px solid #475569; color: white;" step="0.01" value="${ch.max}">
                        <span class="absolute right-3 top-1/2 -translate-y-1/2 text-slate-500 text-xs">V</span>
                    </div>
                </div>
            </div>
        `;
        container.appendChild(div);
    });
}

async function saveConfig() {
    const curve = parseFloat(document.getElementById('input-curve').value);
    const damping = parseFloat(document.getElementById('input-damping').value);
    const statusDiv = document.getElementById('config-status');

    statusDiv.innerHTML = 'Збереження...';
    statusDiv.className = 'text-sm font-medium text-slate-400 mt-2';

    try {
        const response = await fetch('/api/v1/config', {
            method: 'PATCH',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                control: {
                    curve: isNaN(curve) ? null : curve,
                    damping: isNaN(damping) ? null : damping
                }
            })
        });

        if (!response.ok) {
            const errorData = await response.json();
            throw new Error(errorData.error || `HTTP ${response.status}`);
        }

        const data = await response.json();
        statusDiv.innerHTML = 'Налаштування успішно збережено!';
        statusDiv.className = 'text-sm font-medium text-green-400 mt-2';
        
        // Update input fields with confirmed values
        if (data.control) {
            document.getElementById('input-curve').value = data.control.curve;
            document.getElementById('input-damping').value = data.control.damping;
        }
        
        setTimeout(() => { statusDiv.innerHTML = ''; }, 3000);
    } catch (error) {
        statusDiv.innerHTML = `Помилка: ${error.message}`;
        statusDiv.className = 'text-sm font-medium text-red-400 mt-2';
    }
}

async function saveNetworkConfig() {
    const mavlinkStr = document.getElementById('input-mavlink').value;
    const streamDest = document.getElementById('input-stream').value;
    const statusDiv = document.getElementById('network-status');
    const warningDiv = document.getElementById('reboot-warning');

    statusDiv.innerHTML = 'Збереження...';
    statusDiv.className = 'text-sm font-medium text-slate-400 mt-2';

    try {
        const payload = {};
        if (mavlinkStr !== undefined) {
            payload.mavlink = { connection_string: mavlinkStr };
        }
        if (streamDest !== undefined) {
            payload.stream = { video_dest: streamDest };
        }

        const response = await fetch('/api/v1/config', {
            method: 'PATCH',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(payload)
        });

        if (!response.ok) {
            const errorData = await response.json();
            throw new Error(errorData.error || `HTTP ${response.status}`);
        }

        const data = await response.json();
        statusDiv.innerHTML = 'Мережеві налаштування збережено!';
        statusDiv.className = 'text-sm font-medium text-green-400 mt-2';
        
        warningDiv.classList.remove('hidden');
        
        // Update input fields with confirmed values
        if (data.mavlink) {
            document.getElementById('input-mavlink').value = data.mavlink.connection_string || '';
        }
        if (data.stream) {
            document.getElementById('input-stream').value = data.stream.video_dest || '';
        }
        
        setTimeout(() => { statusDiv.innerHTML = ''; }, 3000);
    } catch (error) {
        statusDiv.innerHTML = `Помилка: ${error.message}`;
        statusDiv.className = 'text-sm font-medium text-red-400 mt-2';
    }
}

async function fetchInfo() {
    const infoDiv = document.getElementById('drone-info');
    try {
        const response = await fetch('/api/v1/info');
        if (!response.ok) throw new Error(`Помилка: ${response.status}`);
        
        const data = await response.json();
        
        let streamsHtml = '';
        if (data.streams && data.streams.length > 0) {
            streamsHtml = '<ul>' + data.streams.map(s => `<li>Стрім ${s.id}: ${s.name}${s.has_eco ? ' (eco)' : ''}</li>`).join('') + '</ul>';
        } else {
            streamsHtml = '<p>Немає налаштованих стрімів</p>';
        }

        let sslHashHtml = '';
        if (data.ssl_cert_hash) {
            sslHashHtml = `
                <div class="mt-3 pt-3 border-t border-slate-700/30">
                    <p class="text-sm text-slate-400 mb-1">SSL Certificate Hash:</p>
                    <div class="flex items-center gap-2">
                        <code class="text-xs bg-slate-900 px-2 py-1 rounded text-blue-400 font-mono select-all">${data.ssl_cert_hash}</code>
                        <button onclick="copyToClipboard('${data.ssl_cert_hash}', this)" class="p-1.5 bg-slate-700 hover:bg-slate-600 rounded transition-colors" title="Копіювати">
                            <svg class="w-4 h-4 text-slate-300" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                                <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M8 16H6a2 2 0 01-2-2V6a2 2 0 012-2h8a2 2 0 012 2v2m-6 12h8a2 2 0 002-2v-8a2 2 0 00-2-2h-8a2 2 0 00-2 2v8a2 2 0 002 2z"></path>
                            </svg>
                        </button>
                        <span class="copy-feedback text-xs text-green-400 hidden">Скопійовано!</span>
                    </div>
                </div>
            `;
        }
        
        infoDiv.innerHTML = `
            <p><strong>Drone ID:</strong> ${data.drone_id}</p>
            <p><strong>Версія:</strong> ${data.version}</p>
            <p><strong>Доступні камери:</strong></p>
            ${streamsHtml}
            ${sslHashHtml}
        `;
    } catch (error) {
        infoDiv.innerHTML = `<p style="color: red;">Не вдалося завантажити інформацію: ${error.message}</p>`;
    }
}

async function copyToClipboard(text, button) {
    try {
        if (navigator.clipboard && navigator.clipboard.writeText) {
            await navigator.clipboard.writeText(text);
        } else {
            const textArea = document.createElement('textarea');
            textArea.value = text;
            textArea.style.position = 'fixed';
            textArea.style.left = '-9999px';
            document.body.appendChild(textArea);
            textArea.select();
            document.execCommand('copy');
            document.body.removeChild(textArea);
        }
        
        const feedback = button.nextElementSibling;
        if (feedback) {
            feedback.classList.remove('hidden');
            setTimeout(() => feedback.classList.add('hidden'), 2000);
        }
    } catch (err) {
        console.error('Failed to copy:', err);
    }
}

async function rebootDrone() {
    try {
        const response = await fetch('/api/v1/system/exit', {
            method: 'POST'
        });
        
        if (response.ok) {
            alert('Команда на завершення відправлена. Додаток зупиняється.');
        } else {
            alert('Помилка при спробі завершити роботу.');
        }
    } catch (error) {
        alert(`Помилка: ${error.message}`);
    }
}

async function saveAdcConfig() {
    const statusDiv = document.getElementById('adc-status');
    const channels = [];
    document.querySelectorAll('.adc-channel').forEach(el => {
        const id = parseInt(el.querySelector('.adc-id').value);
        const scale = parseFloat(el.querySelector('.adc-scale').value);
        const min = parseFloat(el.querySelector('.adc-min').value);
        const max = parseFloat(el.querySelector('.adc-max').value);
        channels.push({
            id: isNaN(id) ? 0 : id,
            scale: isNaN(scale) ? 48.52 : scale,
            min: isNaN(min) ? 0.0 : min,
            max: isNaN(max) ? 16.8 : max
        });
    });

    statusDiv.innerHTML = 'Збереження...';
    statusDiv.className = 'text-sm font-medium text-slate-400 mt-2';

    try {
        const response = await fetch('/api/v1/config', {
            method: 'PATCH',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                adc: {
                    channels: channels
                }
            })
        });

        if (!response.ok) {
            const errorData = await response.json();
            throw new Error(errorData.error || `HTTP ${response.status}`);
        }

        const data = await response.json();
        statusDiv.innerHTML = 'Налаштування ADC успішно збережено!';
        statusDiv.className = 'text-sm font-medium text-green-400 mt-2';
        
        if (data.adc && data.adc.channels) {
            renderAdcChannels(data.adc.channels);
        }
        
        setTimeout(() => { statusDiv.innerHTML = ''; }, 3000);
    } catch (error) {
        statusDiv.innerHTML = `Помилка: ${error.message}`;
        statusDiv.className = 'text-sm font-medium text-red-400 mt-2';
    }
}