/* Extracted JS from the inline script in index.html */

/**
 * ==================== CONFIGURATION ====================
 * Update these settings to match your Python backend server
 */
const CONFIG = {
    // WebSocket connection (preferred for real-time)
    WEBSOCKET_URL: 'ws://localhost:8000/ws',
    // HTTP Fallback (polling)
    API_URL: 'http://localhost:8000/sensor-data',
    // ADD THIS LINE BELOW:
    SERIAL_URL: 'http://localhost:8000/serial-config',
    
    // Update frequency
    UPDATE_INTERVAL: 1000, 
    // Use WebSocket (true) or HTTP polling (false)
    USE_WEBSOCKET: true,
};

// ==================== STATE MANAGEMENT ====================
const state = {
    isConnected: false,
    currentState: 'waiting',
    lastUpdate: null,
    minTemp: Infinity,
    maxTemp: -Infinity,
    dataBuffer: [],
    maxSamples: 3600, // max items kept in the client buffer (seconds)
    chartPaused: false,
    chartWindowSize: 600, // seconds, 0 for all
    retryCount: 0,
    maxRetries: 5,
    // Remember last valid measurements (avoid showing invalid negative IAQ, etc.)
    lastValid: {
        temperature: null,
        pressure: null,
        altitude: null,
        iaq: null,
        tvoc: null,
        eco2: null,
        ethanol: null,
    }
};

// ==================== DOM ELEMENTS ====================
const elements = {
    tempValue: document.getElementById('tempValue'),
    pressureValue: document.getElementById('pressureValue'),
    altitudeValue: document.getElementById('altitudeValue'),
    iaqValue: document.getElementById('iaqValue'),
    iaqCircle: document.getElementById('iaqCircle'),
    iaqLabel: document.getElementById('iaqLabel'),
    tvocValue: document.getElementById('tvocValue'),
    eco2Value: document.getElementById('eco2Value'),
    ethanolValue: document.getElementById('ethanolValue'),
    statusDot: document.getElementById('statusDot'),
    statusText: document.getElementById('statusText'),
    lastUpdate: document.getElementById('lastUpdate'),
    tempCard: document.getElementById('tempCard'),
    tempMeta: document.getElementById('tempMeta'),
    pressureMeta: document.getElementById('pressureMeta'),
    connectionStatus: document.getElementById('connectionStatus'),
    // Chart related
    metricSelect: document.getElementById('metricSelect'),
    pauseChartBtn: document.getElementById('pauseChartBtn'),
    resumeChartBtn: document.getElementById('resumeChartBtn'),
    clearChartBtn: document.getElementById('clearChartBtn'),
    exportChartBtn: document.getElementById('exportChartBtn'),
    windowSizeSelect: document.getElementById('windowSizeSelect'),
    // Settings modal elements
    openSettingsBtn: document.getElementById('openSettingsBtn'),
    settingsModal: document.getElementById('settingsModal'),
    closeSettingsBtn: document.getElementById('closeSettingsBtn'),
    applySerialBtn: document.getElementById('applySerialBtn'),
    comPortInput: document.getElementById('comPortInput'),
    baudInput: document.getElementById('baudInput'),
    timeoutInput: document.getElementById('timeoutInput'),
};

// ==================== WEBSOCKET CONNECTION ====================
let ws = null;
let timeseriesChart = null;
const MAX_DISPLAY_POINTS = 600; // max points to render on the chart for performance

function connectWebSocket() {
    try {
        ws = new WebSocket(CONFIG.WEBSOCKET_URL);

        ws.onopen = () => {
            console.log('✓ WebSocket connected');
            state.isConnected = true;
            state.retryCount = 0;
            updateConnectionUI();
            // Request initial data from backend
            ws.send(JSON.stringify({ type: 'request_data' }));
        };

        ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                updateDashboard(data);
            } catch (err) {
                console.error('Error parsing WebSocket message:', err);
            }
        };

        ws.onerror = (error) => {
            console.error('WebSocket error:', error);
            state.isConnected = false;
            updateConnectionUI();
        };

        ws.onclose = () => {
            console.log('✗ WebSocket disconnected');
            state.isConnected = false;
            updateConnectionUI();
            // Attempt reconnection with exponential backoff
            attemptReconnect();
        };

    } catch (err) {
        console.error('Failed to connect WebSocket:', err);
        state.isConnected = false;
        updateConnectionUI();
        attemptReconnect();
    }
}

function attemptReconnect() {
    if (state.retryCount < state.maxRetries) {
        state.retryCount++;
        const delay = Math.min(1000 * Math.pow(2, state.retryCount), 30000);
        console.log(`Reconnecting in ${delay}ms... (attempt ${state.retryCount}/${state.maxRetries})`);
        setTimeout(connectWebSocket, delay);
    } else {
        console.error('Max reconnection attempts reached, falling back to HTTP polling');
        CONFIG.USE_WEBSOCKET = false;
        startPolling();
    }
}

// ==================== HTTP POLLING (FALLBACK) ====================
let pollInterval = null;

function startPolling() {
    console.log('Starting HTTP polling at ' + CONFIG.UPDATE_INTERVAL + 'ms intervals');
    if (pollInterval) clearInterval(pollInterval);
    pollInterval = setInterval(fetchSensorData, CONFIG.UPDATE_INTERVAL);
    // Fetch immediately
    fetchSensorData();
}

async function fetchSensorData() {
    try {
        const response = await fetch(CONFIG.API_URL);
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        const data = await response.json();
        state.isConnected = true;
        updateDashboard(data);
    } catch (err) {
        console.error('Polling error:', err);
        state.isConnected = false;
        updateConnectionUI();
    }
}

// ==================== DASHBOARD UPDATE ====================
function updateDashboard(data) {
    try {
        // --- NEW LOGIC START ---
        // Handle state mapping and waiting conditions
        if (data.state) {
            state.currentState = data.state;
            // Map backend waiting states to frontend 'waiting'
            if (data.state === 'waiting_for_data' || data.state === 'waiting_for_connection') {
                state.currentState = 'waiting';
            }
        } else {
            // If data arrives without state, assume streaming
            state.currentState = 'streaming';
        }

        updateStateIndicator();

        // If we are waiting or disconnected, don't update UI values yet
        if (state.currentState === 'waiting' || state.currentState === 'disconnected') {
            updateConnectionUI();
            return;
        }
        // --- NEW LOGIC END ---

        // Update timestamp
        state.lastUpdate = data.timestamp || new Date().toLocaleTimeString();
        updateTimestamp();

        // Update communication state (already handled above)

        // Update temperature
        if (data.temperature !== undefined) {
            const temp = parseFloat(data.temperature);
            updateValue(elements.tempValue, temp.toFixed(1), '°C');
            
            // Track min/max
            if (temp < state.minTemp) state.minTemp = temp;
            if (temp > state.maxTemp) state.maxTemp = temp;
            elements.tempMeta.textContent = `Max: ${state.maxTemp.toFixed(1)}°C | Min: ${state.minTemp.toFixed(1)}°C`;
            state.lastValid.temperature = temp;
        }

        // Update pressure
        if (data.pressure !== undefined) {
            const pressure = parseFloat(data.pressure);
            const pressureHpa = (pressure / 100).toFixed(0);
            updateValue(elements.pressureValue, pressure.toFixed(0), 'Pa');
            elements.pressureMeta.textContent = `${pressureHpa} hPa`;
            state.lastValid.pressure = pressure;
        }

        // Update altitude
        if (data.altitude !== undefined) {
            const altitude = parseFloat(data.altitude);
            updateValue(elements.altitudeValue, altitude.toFixed(1), 'm');
            state.lastValid.altitude = altitude;
        }

        // Update IAQ
        if (data.iaq !== undefined) {
            const rawIaq = parseInt(data.iaq);
            // If the backend indicates warming up (negative IAQ), keep showing last known valid value
            if (data.state === 'warming_up' || rawIaq < 0) {
                // Do not update displayed IAQ; use last valid
                if (state.lastValid.iaq !== null) {
                    updateValue(elements.iaqValue, state.lastValid.iaq);
                    elements.iaqValue.classList.add('stale');
                    updateIAQColor(state.lastValid.iaq);
                }
            } else {
                updateValue(elements.iaqValue, rawIaq);
                updateIAQColor(rawIaq);
                state.lastValid.iaq = rawIaq;
                elements.iaqValue.classList.remove('stale');
            }
        }

        // Update TVOC
        if (data.tvoc !== undefined) {
            const tvoc = parseFloat(data.tvoc);
            if (!isNaN(tvoc) && tvoc >= 0) {
                updateValue(elements.tvocValue, tvoc.toFixed(0));
                state.lastValid.tvoc = tvoc;
                elements.tvocValue.classList.remove('stale');
            } else if (state.lastValid.tvoc !== null) {
                updateValue(elements.tvocValue, state.lastValid.tvoc.toFixed(0));
                elements.tvocValue.classList.add('stale');
            }
        }

        // Update eCO2
        if (data.eco2 !== undefined) {
            const eco2 = parseFloat(data.eco2);
            if (!isNaN(eco2) && eco2 >= 0) {
                updateValue(elements.eco2Value, eco2.toFixed(0));
                state.lastValid.eco2 = eco2;
                elements.eco2Value.classList.remove('stale');
            } else if (state.lastValid.eco2 !== null) {
                updateValue(elements.eco2Value, state.lastValid.eco2.toFixed(0));
                elements.eco2Value.classList.add('stale');
            }
        }

        // Update Ethanol
        if (data.ethanol !== undefined) {
            const ethanol = parseFloat(data.ethanol);
            if (!isNaN(ethanol) && ethanol >= 0) {
                updateValue(elements.ethanolValue, ethanol.toFixed(2));
                state.lastValid.ethanol = ethanol;
                elements.ethanolValue.classList.remove('stale');
            } else if (state.lastValid.ethanol !== null) {
                updateValue(elements.ethanolValue, state.lastValid.ethanol.toFixed(2));
                elements.ethanolValue.classList.add('stale');
            }
        }

        updateConnectionUI();
                // Record data for chart
                recordDataPoint(data);
                // Update chart (if exists and not paused)
                if (timeseriesChart && !state.chartPaused) {
                    updateChart(data);
                }

    } catch (err) {
        console.error('Error updating dashboard:', err);
    }
}

// ==================== UTILITY FUNCTIONS ====================
function updateValue(element, value, unit = '') {
    if (element.textContent !== value + (unit ? ' ' + unit : '')) {
        element.classList.remove('updating');
        // Trigger reflow to restart animation
        void element.offsetWidth;
        element.classList.add('updating');
        element.textContent = value + (unit ? ' ' + unit : '');
        setTimeout(() => element.classList.remove('updating'), 400);
    }
}

function updateIAQColor(iaq) {
    const iaqCircle = elements.iaqCircle;
    const iaqLabel = elements.iaqLabel;
    
    iaqCircle.classList.remove('good', 'moderate', 'poor');
    iaqCircle.classList.remove('unknown');
    
    if (iaq === null || iaq === undefined || Number.isNaN(Number(iaq))) {
        iaqCircle.classList.add('unknown');
        iaqLabel.textContent = 'Warming';
        return;
    }

    if (iaq <= 50) {
        iaqCircle.classList.add('good');
        iaqLabel.textContent = 'Good';
    } else if (iaq <= 100) {
        iaqCircle.classList.add('moderate');
        iaqLabel.textContent = 'Moderate';
    } else {
        iaqCircle.classList.add('poor');
        iaqLabel.textContent = 'Poor';
    }
}

function updateStateIndicator() {
    const statusDot = elements.statusDot;
    const statusText = elements.statusText;
    
    // Remove all possible classes
    statusDot.classList.remove('streaming', 'buffering', 'retransmitting', 'warming', 'waiting');
    
    switch (state.currentState) {
        case 'streaming':
            statusDot.classList.add('streaming');
            statusText.textContent = 'Streaming';
            break;
        case 'buffering':
            statusDot.classList.add('buffering');
            statusText.textContent = 'Buffering';
            break;
        case 'retransmitting':
            statusDot.classList.add('retransmitting');
            statusText.textContent = 'Retransmitting';
            break;
        case 'warming_up':
            statusDot.classList.add('warming');
            statusText.textContent = 'Warming Up';
            break;
        case 'waiting':
        case 'disconnected':
        default:
            statusDot.classList.add('waiting'); // Shows Grey Dot
            statusText.textContent = 'Waiting for Connection...';
            break;
    }
}

function updateTimestamp() {
    const date = new Date(state.lastUpdate);
    const timeString = date.toLocaleTimeString('en-US', { 
        hour: '2-digit', 
        minute: '2-digit', 
        second: '2-digit',
        hour12: true 
    });
    const dateString = date.toLocaleDateString('en-US');
    elements.lastUpdate.textContent = `Last update: ${dateString} ${timeString}`;
}

function updateConnectionUI() {
    const status = elements.connectionStatus;
    
    if (state.isConnected) {
        // WebSocket is open, but are we getting data from MCU?
        if (state.currentState === 'waiting') {
            status.classList.remove('connected', 'disconnected');
            status.style.backgroundColor = ''; 
            status.style.color = 'var(--color-text-secondary)';
            status.textContent = '⚠ Server Connected - Waiting for MCU...';
        } else if (state.currentState === 'disconnected') {
            status.classList.remove('connected');
            status.classList.add('disconnected');
            status.textContent = '✗ MCU Disconnected - Retrying...';
        } else {
            // Streaming / Buffering / Warming
            status.classList.remove('disconnected');
            status.classList.add('connected');
            status.textContent = '✓ Connected to MCU via ' + (CONFIG.USE_WEBSOCKET ? 'WebSocket' : 'HTTP Polling');
        }
    } else {
        // WebSocket/Server is down
        status.classList.remove('connected');
        status.classList.add('disconnected');
        status.textContent = '✗ Disconnected from Dashboard Server - Reconnecting...';
    }
}

// ==================== INITIALIZATION ====================
function init() {
    console.log('Initializing Environmental Monitor Dashboard...');
    console.log('Configuration:', CONFIG);
    
    if (CONFIG.USE_WEBSOCKET) {
        connectWebSocket();
    } else {
        startPolling();
    }

    // Update timestamp every second
    setInterval(updateTimestamp, 1000);

    // For development/testing: Log current state every 5 seconds
    console.log('Dashboard ready. Waiting for sensor data...');
    // Initialize Chart
    initChart();
    // Wire chart controls
    setupChartControls();
    // Wire settings modal events
    elements.openSettingsBtn?.addEventListener('click', async () => {
        elements.settingsModal?.classList.remove('hidden');
        try {
            await fetchSerialConfig();
        } catch (e) {
            console.log("Backend not ready yet");
        }
    });
    elements.closeSettingsBtn?.addEventListener('click', () => {
        elements.settingsModal?.classList.add('hidden');
    });
    elements.applySerialBtn?.addEventListener('click', () => {
        applySerialConfig();
    });
    // Rebuild chart from any existing buffer
    rebuildChartFromBuffer(elements.metricSelect?.value || 'temperature');
}

// Start the application when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
} else {
    init();
}

// ==================== CHARTING FUNCTIONS ====================
function initChart() {
    const ctx = document.getElementById('timeseriesChart');
    if (!ctx) return;

    // default metric is temperature
    const config = {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Temperature (°C)',
                backgroundColor: 'rgba(32, 143, 159, 0.2)',
                borderColor: 'rgba(32, 143, 159, 1)',
                data: [],
                fill: true,
                tension: 0.2,
            }]
        },
        options: {
            animation: false,
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                x: {
                    type: 'category',
                    ticks: { autoSkip: true, maxTicksLimit: 12, maxRotation: 0, minRotation: 0 }
                },
                y: { beginAtZero: false }
            },
            plugins: {
                legend: { display: false }
            }
        }
    };
    timeseriesChart = new Chart(ctx, config);
}

function recordDataPoint(data) {
    try {
        const entry = {
            timestamp: data.timestamp || new Date().toISOString(),
            pressure: (data.pressure !== undefined && !isNaN(parseFloat(data.pressure)) && parseFloat(data.pressure) >= 0) ? parseFloat(data.pressure) : state.lastValid.pressure,
            temperature: (data.temperature !== undefined && !isNaN(parseFloat(data.temperature))) ? parseFloat(data.temperature) : state.lastValid.temperature,
            altitude: (data.altitude !== undefined && !isNaN(parseFloat(data.altitude))) ? parseFloat(data.altitude) : state.lastValid.altitude,
            iaq: (data.iaq !== undefined && !isNaN(parseInt(data.iaq)) && parseInt(data.iaq) >= 0) ? parseInt(data.iaq) : state.lastValid.iaq,
            tvoc: (data.tvoc !== undefined && !isNaN(parseFloat(data.tvoc)) && parseFloat(data.tvoc) >= 0) ? parseFloat(data.tvoc) : state.lastValid.tvoc,
            eco2: (data.eco2 !== undefined && !isNaN(parseFloat(data.eco2)) && parseFloat(data.eco2) >= 0) ? parseFloat(data.eco2) : state.lastValid.eco2,
            ethanol: (data.ethanol !== undefined && !isNaN(parseFloat(data.ethanol)) && parseFloat(data.ethanol) >= 0) ? parseFloat(data.ethanol) : state.lastValid.ethanol,
        };
        state.dataBuffer.push(entry);
        // Cap buffer to last 3 hours (or configurable)
        const maxSamples = state.maxSamples || 3600; // limit samples in the buffer
        if (state.dataBuffer.length > maxSamples) {
            state.dataBuffer.shift();
        }
        // Trim according to chart window size if selected and non-zero
        if (state.chartWindowSize > 0) {
            const cutoff = Date.now() - state.chartWindowSize * 1000;
            while (state.dataBuffer.length && new Date(state.dataBuffer[0].timestamp).getTime() < cutoff) {
                state.dataBuffer.shift();
            }
        }
    } catch (err) {
        console.warn('Failed to record data point', err);
    }
}

function updateChart(data) {
    if (!timeseriesChart) return;
    const metric = elements.metricSelect?.value || 'temperature';
    const label = new Date((data.timestamp || new Date()).toString().replace(' ', 'T')).toLocaleTimeString();
    const value = Number(data[metric]);
    if (Number.isNaN(value)) return; // skip invalid points
    // push new label and value
    timeseriesChart.data.labels.push(label);
    timeseriesChart.data.datasets[0].data.push(value);
    // trim the rendered points to maximum allowed for performance
    const maxDisplay = Math.min(MAX_DISPLAY_POINTS, state.chartWindowSize > 0 ? state.chartWindowSize : MAX_DISPLAY_POINTS);
    while (timeseriesChart.data.labels.length > maxDisplay) {
        timeseriesChart.data.labels.shift();
        timeseriesChart.data.datasets[0].data.shift();
    }
    // Trim dataset to size if window selected
    if (state.chartWindowSize > 0) {
        const cutoff = Date.now() - state.chartWindowSize * 1000;
        while (timeseriesChart.data.labels.length && new Date(state.dataBuffer[0].timestamp).getTime() < cutoff) {
            timeseriesChart.data.labels.shift();
            timeseriesChart.data.datasets[0].data.shift();
        }
    }
    timeseriesChart.update('none');
}

function setupChartControls() {
    if (!elements.metricSelect) return;
    elements.metricSelect.addEventListener('change', (e) => {
        const metric = e.target.value;
        // Update dataset label & color per metric
        const ds = timeseriesChart.data.datasets[0];
        switch (metric) {
            case 'temperature': ds.label = 'Temperature (°C)'; ds.borderColor = 'rgba(32, 143, 159, 1)'; break;
            case 'pressure': ds.label = 'Pressure (Pa)'; ds.borderColor = 'rgba(52, 152, 219, 1)'; break;
            case 'altitude': ds.label = 'Altitude (m)'; ds.borderColor = 'rgba(155, 89, 182, 1)'; break;
            case 'iaq': ds.label = 'IAQ'; ds.borderColor = 'rgba(39, 174, 96, 1)'; break;
        }
        // Rebuild the chart from the buffer — prefer an instantaneous rebuild
        rebuildChartFromBuffer(metric);
    });

    elements.pauseChartBtn?.addEventListener('click', () => {
        state.chartPaused = true;
        elements.pauseChartBtn.classList.add('hidden');
        elements.resumeChartBtn.classList.remove('hidden');
    });
    elements.resumeChartBtn?.addEventListener('click', () => {
        state.chartPaused = false;
        elements.resumeChartBtn.classList.add('hidden');
        elements.pauseChartBtn.classList.remove('hidden');
    });

    elements.clearChartBtn?.addEventListener('click', () => {
        timeseriesChart.data.labels = [];
        timeseriesChart.data.datasets[0].data = [];
        state.dataBuffer = [];
        timeseriesChart.update();
    });

    elements.exportChartBtn?.addEventListener('click', () => {
        exportBufferCSV();
    });

    elements.windowSizeSelect?.addEventListener('change', (e) => {
        const v = parseInt(e.target.value, 10) || 0;
        state.chartWindowSize = v;
        rebuildChartFromBuffer(elements.metricSelect?.value || 'temperature');
    });
}

// ==================== SERIAL SETTINGS UI ====================
async function fetchSerialConfig() {
    try {
        // === CHANGE THIS LINE ===
        const res = await fetch(CONFIG.SERIAL_URL);
        // ========================

        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const cfg = await res.json();
        elements.comPortInput.value = cfg.port || '';
        elements.baudInput.value = cfg.baud || 9600;
        elements.timeoutInput.value = cfg.timeout || 2;
    } catch (err) {
        console.warn('Unable to fetch serial config:', err);
        // Alert removed so it doesn't annoy you if backend is off
    }
}

async function applySerialConfig() {
    try {
        const body = {
            port: elements.comPortInput.value,
            baud: parseInt(elements.baudInput.value, 10) || 9600,
            timeout: parseFloat(elements.timeoutInput.value) || 2,
            enabled: true
        };
        elements.applySerialBtn.disabled = true;

        // === CHANGE THIS LINE ===
        const res = await fetch(CONFIG.SERIAL_URL, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(body)
        });
        // ========================

        elements.applySerialBtn.disabled = false;
        if (!res.ok) {
            const t = await res.text();
            throw new Error(t || `HTTP ${res.status}`);
        }
        const result = await res.json();
        alert('Connected!');
        elements.settingsModal?.classList.add('hidden');
    } catch (err) {
        console.error('Failed to apply serial config:', err);
        elements.applySerialBtn.disabled = false;
        alert('Failed: ' + err.message);
    }
}

function rebuildChartFromBuffer(metric) {
    if (!timeseriesChart) return;
    timeseriesChart.data.labels = [];
    timeseriesChart.data.datasets[0].data = [];
    const list = state.dataBuffer || [];
    // render the most recent points only (cap to MAX_DISPLAY_POINTS)
    const startIndex = Math.max(0, list.length - MAX_DISPLAY_POINTS);
    for (let i = startIndex; i < list.length; i++) {
        const entry = list[i];
        const val = Number(entry[metric]);
        if (Number.isNaN(val)) continue;
        timeseriesChart.data.labels.push(new Date(entry.timestamp).toLocaleTimeString());
        timeseriesChart.data.datasets[0].data.push(val);
    }
    timeseriesChart.update();
}

function exportBufferCSV() {
    const buf = state.dataBuffer;
    if (!buf || !buf.length) return alert('No data to export');
    const headers = Object.keys(buf[0]);
    const rows = buf.map(r => headers.map(h => r[h]).join(','));
    const csv = [headers.join(','), ...rows].join('\n');
    const blob = new Blob([csv], { type: 'text/csv' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'sensor_data.csv';
    document.body.appendChild(a);
    a.click();
    a.remove();
    URL.revokeObjectURL(url);
}
