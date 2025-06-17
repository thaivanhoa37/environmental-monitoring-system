// Firebase configuration
const firebaseConfig = {
    apiKey: "YOUR_API_KEY",
    authDomain: "environmental-monitoring-be427.firebaseapp.com",
    databaseURL: "https://environmental-monitoring-be427-default-rtdb.firebaseio.com",
    projectId: "environmental-monitoring-be427",
    storageBucket: "environmental-monitoring-be427.appspot.com",
    messagingSenderId: "YOUR_MESSAGING_SENDER_ID",
    appId: "YOUR_APP_ID"
};

// Initialize Firebase
firebase.initializeApp(firebaseConfig);
const database = firebase.database();

// WHO guidelines and thresholds
const WHO_GUIDELINES = {
    co2: {
        safe: 1000,
        moderate: 2000,
        danger: 5000,
        unit: 'ppm',
        recommendations: {
            good: 'Chất lượng không khí tốt',
            warning: 'Cần tăng cường thông gió',
            danger: 'Cần thông gió ngay lập tức'
        }
    },
    dust: {
        safe: 100,
        moderate: 150,
        danger: 200,
        unit: 'µg/m³',
        recommendations: {
            good: 'Chất lượng không khí tốt',
            warning: 'Có thể gây kích ứng đường hô hấp',
            danger: 'Nguy hại cho sức khỏe hô hấp'
        }
    },
    aqi: {
        safe: 100,
        moderate: 150,
        danger: 200,
        unit: '',
        recommendations: {
            good: 'Chất lượng không khí tốt',
            warning: 'Hạn chế hoạt động ngoài trời kéo dài',
            danger: 'Không nên hoạt động ngoài trời, đóng cửa sổ'
        }
    }
};

let tempChart, humidityChart, co2Chart, dustChart;
let timeRange = 24; // Default time range in hours

// Function to fetch historical data
async function fetchHistoricalData(hours) {
    const endTime = new Date();
    const startTime = new Date(endTime - hours * 60 * 60 * 1000);

    const historicalRef = database.ref('env_data/history');
    const query = historicalRef
        .orderByChild('timestamp')
        .startAt(startTime.toISOString())
        .endAt(endTime.toISOString());

    return new Promise((resolve, reject) => {
        query.once('value', (snapshot) => {
            const data = {
                timestamps: [],
                temperatures: [],
                humidities: [],
                pressures: [],
                co2_levels: [],
                dust_levels: [],
                aqi_levels: []
            };

            snapshot.forEach((childSnapshot) => {
                const reading = childSnapshot.val();
                const timestamp = new Date(reading.timestamp);

                data.timestamps.push(timestamp);
                data.temperatures.push(reading.temperature);
                data.humidities.push(reading.humidity);
                data.pressures.push(reading.pressure);
                data.co2_levels.push(reading.co2);
                data.dust_levels.push(reading.dust);
                data.aqi_levels.push(reading.aqi);
            });

            resolve(data);
        }, reject);
    });
}

// Function to change time range
async function changeTimeRange(hours) {
    timeRange = parseInt(hours);
    const data = await fetchHistoricalData(timeRange);
    updateCharts(data);
    
    // Update all dropdowns to show the same time range
    document.querySelectorAll('.form-select').forEach(select => {
        select.value = hours;
    });
}

function getAssessment(value, parameter) {
    const guidelines = WHO_GUIDELINES[parameter];
    if (value <= guidelines.safe) return 'good';
    if (value <= guidelines.moderate) return 'warning';
    return 'danger';
}

function updateAlertsSection(data) {
    const container = document.getElementById('alertsContainer');
    container.innerHTML = '';

    const parameters = {
        co2: { label: 'Nồng độ CO₂', value: data.co2 },
        dust: { label: 'Bụi mịn PM2.5', value: data.dust },
        aqi: { label: 'Chỉ số AQI', value: data.aqi }
    };

    for (const [param, info] of Object.entries(parameters)) {
        const guidelines = WHO_GUIDELINES[param];
        const assessment = getAssessment(info.value, param);
        const unit = guidelines.unit ? ` ${guidelines.unit}` : '';

        const alertElement = document.createElement('div');
        alertElement.className = `alert-item ${assessment}`;
        alertElement.innerHTML = `
            <div class="alert-content">
                <div class="alert-title">${info.label}</div>
                <div class="alert-value">Giá trị hiện tại: ${info.value}${unit}</div>
                <div class="who-assessment">
                    <i class="fas fa-info-circle"></i> 
                    ${guidelines.recommendations[assessment]}
                </div>
                <div class="who-limits">
                    Tiêu chuẩn WHO: 
                    An toàn < ${guidelines.safe}${unit} | 
                    Trung bình < ${guidelines.moderate}${unit} | 
                    Nguy hiểm ≥ ${guidelines.danger}${unit}
                </div>
            </div>
        `;
        container.appendChild(alertElement);
    }
}

function createChartOptions(label) {
    return {
        responsive: true,
        maintainAspectRatio: false,
        plugins: {
            legend: {
                position: 'top',
            }
        },
        scales: {
            x: {
                type: 'time',
                time: {
                    unit: 'hour',
                    displayFormats: {
                        hour: 'HH:mm',
                        day: 'MM/DD'
                    }
                },
                title: {
                    display: true,
                    text: 'Thời gian'
                }
            },
            y: {
                title: {
                    display: true,
                    text: label
                }
            }
        }
    };
}

function createChart(canvasId, label, color, options) {
    return new Chart(document.getElementById(canvasId).getContext('2d'), {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: label,
                data: [],
                borderColor: color,
                backgroundColor: 'transparent',
                tension: 0.4
            }]
        },
        options: options
    });
}

function initCharts() {
    tempChart = createChart('tempChart', 'Nhiệt độ (°C)', '#4e73df', createChartOptions('Nhiệt độ (°C)'));
    humidityChart = createChart('humidityChart', 'Độ ẩm (%)', '#36b9cc', createChartOptions('Độ ẩm (%)'));
    
    co2Chart = new Chart(document.getElementById('co2Chart').getContext('2d'), {
        type: 'line',
        data: {
            labels: [],
            datasets: [
                {
                    label: 'CO2 (ppm)',
                    data: [],
                    borderColor: '#f6c23e',
                    backgroundColor: 'transparent',
                    tension: 0.4
                },
                {
                    label: 'AQI',
                    data: [],
                    borderColor: '#858796',
                    backgroundColor: 'transparent',
                    tension: 0.4
                }
            ]
        },
        options: createChartOptions('Mức độ')
    });

    dustChart = createChart('dustChart', 'Bụi mịn (µg/m³)', '#e74a3b', createChartOptions('Bụi mịn (µg/m³)'));
}

function updateCharts(data) {
    // Sort data by timestamp
    const indices = data.timestamps.map((_, i) => i);
    indices.sort((a, b) => data.timestamps[a] - data.timestamps[b]);

    const sortedData = {
        timestamps: indices.map(i => data.timestamps[i]),
        temperatures: indices.map(i => data.temperatures[i]),
        humidities: indices.map(i => data.humidities[i]),
        co2_levels: indices.map(i => data.co2_levels[i]),
        dust_levels: indices.map(i => data.dust_levels[i]),
        aqi_levels: indices.map(i => data.aqi_levels[i])
    };

    // Update temperature chart
    tempChart.data.labels = sortedData.timestamps;
    tempChart.data.datasets[0].data = sortedData.temperatures;
    tempChart.update();

    // Update humidity chart
    humidityChart.data.labels = sortedData.timestamps;
    humidityChart.data.datasets[0].data = sortedData.humidities;
    humidityChart.update();

    // Update CO2 & AQI chart
    co2Chart.data.labels = sortedData.timestamps;
    co2Chart.data.datasets[0].data = sortedData.co2_levels;
    co2Chart.data.datasets[1].data = sortedData.aqi_levels;
    co2Chart.update();

    // Update dust chart
    dustChart.data.labels = sortedData.timestamps;
    dustChart.data.datasets[0].data = sortedData.dust_levels;
    dustChart.update();
}

function updateSensorValues(data) {
    // Update current values
    document.getElementById('currentTemp').textContent = data.temperature.toFixed(1);
    document.getElementById('currentHumidity').textContent = data.humidity.toFixed(1);
    document.getElementById('currentPressure').textContent = data.pressure.toFixed(1);
    document.getElementById('currentCO2').textContent = data.co2.toFixed(1);
    document.getElementById('currentDust').textContent = data.dust.toFixed(1);
    document.getElementById('currentAQI').textContent = data.aqi.toFixed(1);

    // Update assessment text
    function updateAssessmentElement(elementId, value, parameter) {
        const assessment = getAssessment(value, parameter);
        const element = document.getElementById(elementId);
        const levelText = {
            'good': 'An toàn',
            'warning': 'Trung bình',
            'danger': 'Nguy hiểm'
        };
        
        element.className = `assessment-text ${assessment}`;
        element.innerHTML = `
            <strong>${levelText[assessment]}</strong><br>
            ${WHO_GUIDELINES[parameter].recommendations[assessment]}
        `;
    }

    updateAssessmentElement('co2Assessment', data.co2, 'co2');
    updateAssessmentElement('dustAssessment', data.dust, 'dust');
    updateAssessmentElement('aqiAssessment', data.aqi, 'aqi');

    // Update WHO assessment section
    updateAlertsSection({
        co2: data.co2,
        dust: data.dust,
        aqi: data.aqi
    });
    
    document.getElementById('lastUpdate').textContent = moment().format('HH:mm:ss');
}

// Initialize application
document.addEventListener('DOMContentLoaded', async function() {
    initCharts();

    // Initial historical data load
    const historicalData = await fetchHistoricalData(timeRange);
    updateCharts(historicalData);

    // Listen to Firebase real-time updates for current values
    const sensorsRef = database.ref('env_data/sensors');
    sensorsRef.on('value', (snapshot) => {
        const data = snapshot.val();
        if (data) {
            updateSensorValues(data);
            
            // Store historical data
            const historyRef = database.ref('env_data/history').push();
            historyRef.set({
                ...data,
                timestamp: new Date().toISOString()
            });
        }
    });

    // Set up automatic refresh of historical data
    setInterval(async () => {
        const data = await fetchHistoricalData(timeRange);
        updateCharts(data);
    }, 60000); // Refresh every minute
});