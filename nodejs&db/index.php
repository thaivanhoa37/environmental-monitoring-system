<?php
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST');
header('Access-Control-Allow-Headers: Content-Type');

function fetch_initial_data() {
    $servername = "localhost";
    $username = "root";
    $password = "1";
    $dbname = "environmental_monitoring_system";

    $conn = new mysqli($servername, $username, $password, $dbname);
    if ($conn->connect_error) {
        return null;
    }

    $time_range = 24;
    $sql = "SELECT timestamp, temperature, humidity, pressure, co2, dust, aqi 
            FROM sensor_data 
            WHERE timestamp >= NOW() - INTERVAL ? HOUR 
            ORDER BY timestamp ASC";
            
    $stmt = $conn->prepare($sql);
    $stmt->bind_param("i", $time_range);
    $stmt->execute();
    $result = $stmt->get_result();

    $data = [
        'timestamps' => [],
        'temperatures' => [],
        'humidities' => [],
        'pressures' => [],
        'co2_levels' => [],
        'dust_levels' => [],
        'aqi_levels' => []
    ];

    while($row = $result->fetch_assoc()) {
        $data['timestamps'][] = $row['timestamp'];
        $data['temperatures'][] = floatval($row['temperature']);
        $data['humidities'][] = floatval($row['humidity']);
        $data['pressures'][] = floatval($row['pressure']);
        $data['co2_levels'][] = floatval($row['co2']);
        $data['dust_levels'][] = floatval($row['dust']);
        $data['aqi_levels'][] = floatval($row['aqi']);
    }

    $stmt->close();
    $conn->close();
    return $data;
}

if (isset($_GET['action']) && $_GET['action'] === 'fetchData') {
    header('Content-Type: application/json');

    $servername = "localhost";
    $username = "root";
    $password = "1";
    $dbname = "environmental_monitoring_system";

    $conn = new mysqli($servername, $username, $password, $dbname);
    if ($conn->connect_error) {
        die(json_encode(['error' => 'Connection failed: ' . $conn->connect_error]));
    }

    $time_range = isset($_GET['time_range']) ? intval($_GET['time_range']) : 24;
    
    $sql = "SELECT timestamp, temperature, humidity, pressure, co2, dust, aqi 
            FROM sensor_data 
            WHERE timestamp >= NOW() - INTERVAL ? HOUR 
            ORDER BY timestamp ASC";
    $stmt = $conn->prepare($sql);
    $stmt->bind_param("i", $time_range);
    $stmt->execute();
    $result = $stmt->get_result();

    $chart_data = [
        'timestamps' => [],
        'temperatures' => [],
        'humidities' => [],
        'pressures' => [],
        'co2_levels' => [],
        'dust_levels' => [],
        'aqi_levels' => []
    ];

    if ($result->num_rows > 0) {
        while($row = $result->fetch_assoc()) {
            $chart_data['timestamps'][] = $row["timestamp"];
            $chart_data['temperatures'][] = floatval($row["temperature"]);
            $chart_data['humidities'][] = floatval($row["humidity"]);
            $chart_data['pressures'][] = floatval($row["pressure"]);
            $chart_data['co2_levels'][] = floatval($row["co2"]);
            $chart_data['dust_levels'][] = floatval($row["dust"]);
            $chart_data['aqi_levels'][] = floatval($row["aqi"]);
        }
    }

    $stmt->close();

    $sql_latest = "SELECT timestamp, temperature, humidity, pressure, co2, dust, aqi 
                   FROM sensor_data ORDER BY timestamp DESC LIMIT 1";
    $result_latest = $conn->query($sql_latest);
    $latest_data = $result_latest->fetch_assoc();

    $conn->close();

    echo json_encode([
        'chart_data' => $chart_data,
        'latest_data' => $latest_data
    ]);
    exit;
}

$servername = "localhost";
$username = "root";
$password = "1";
$dbname = "environmental_monitoring_system";

$conn = new mysqli($servername, $username, $password, $dbname);
if ($conn->connect_error) {
    die("Connection failed: " . $conn->connect_error);
}

$sql = "SELECT timestamp, temperature, humidity, pressure, co2, dust, aqi 
        FROM sensor_data ORDER BY timestamp DESC LIMIT 1";
$result = $conn->query($sql);
$latest = $result->fetch_assoc();

if (!$latest) {
    $latest = [
        'timestamp' => date('Y-m-d H:i:s'),
        'temperature' => 0,
        'humidity' => 0,
        'pressure' => 0,
        'co2' => 0,
        'dust' => 0,
        'aqi' => 0
    ];
}

$conn->close();
?>
<!DOCTYPE html>
<html lang="vi">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Environmental Monitoring System</title>
    <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.1.3/dist/css/bootstrap.min.css" rel="stylesheet">
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css">
    <style>
        :root {
            --primary: #4e73df;
            --success: #1cc88a;
            --warning: #f6c23e;
            --danger: #e74a3b;
            --info: #36b9cc;
            --dark: #5a5c69;
        }

        body {
            background-color: #f8f9fc;
            font-family: 'Segoe UI', sans-serif;
        }

        .topbar {
            background: linear-gradient(to right, var(--primary), #224abe);
            padding: 1rem;
            color: white;
            margin-bottom: 2rem;
            border-radius: 0 0 15px 15px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }

        .status-bar {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-top: 0.5rem;
            font-size: 0.9rem;
            opacity: 0.9;
        }

        .sensor-card {
            color: white;
            padding: 1.5rem;
            border-radius: 15px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
            margin-bottom: 1.5rem;
            transition: transform 0.2s;
        }

        .sensor-card:hover {
            transform: translateY(-5px);
        }

        .sensor-card.temperature { background: linear-gradient(45deg, #4e73df, #224abe); }
        .sensor-card.humidity { background: linear-gradient(45deg, #36b9cc, #1a8eaa); }
        .sensor-card.pressure { background: linear-gradient(45deg, #1cc88a, #13855c); }
        .sensor-card.co2 { background: linear-gradient(45deg, #f6c23e, #dda20a); }
        .sensor-card.dust { background: linear-gradient(45deg, #e74a3b, #be2617); }
        .sensor-card.aqi { background: linear-gradient(45deg, #858796, #60616f); }

        .sensor-value {
            font-size: 2.5rem;
            font-weight: bold;
            margin: 1rem 0;
        }

        .sensor-icon {
            font-size: 2rem;
            opacity: 0.8;
        }

        .assessment-text {
            font-size: 0.85rem;
            padding: 8px 15px;
            border-radius: 6px;
            background: white;
            display: block;
            margin-top: -10px;
            margin-bottom: 20px;
            font-weight: 500;
            box-shadow: 0 2px 8px rgba(0,0,0,0.1);
            text-align: center;
            border-bottom: 3px solid transparent;
        }

        .assessment-text.good {
            color: var(--success);
            border-bottom-color: var(--success);
        }

        .assessment-text.warning {
            color: var(--warning);
            border-bottom-color: var(--warning);
        }

        .assessment-text.danger {
            color: var(--danger);
            border-bottom-color: var(--danger);
        }

        .alerts-section {
            background: white;
            padding: 1.5rem;
            border-radius: 15px;
            margin-bottom: 2rem;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }

        .alert-item {
            padding: 0.75rem;
            margin-bottom: 1rem;
            border-radius: 0.5rem;
            display: flex;
            justify-content: space-between;
            align-items: flex-start;
        }

        .alert-item.warning {
            background-color: rgba(246, 194, 62, 0.1);
            border-left: 4px solid var(--warning);
        }

        .alert-item.danger {
            background-color: rgba(231, 74, 59, 0.1);
            border-left: 4px solid var(--danger);
        }

        .alert-item.good {
            background-color: rgba(28, 200, 138, 0.1);
            border-left: 4px solid var(--success);
        }

        .alert-content {
            flex: 1;
        }

        .alert-title {
            font-weight: bold;
            color: var(--dark);
            margin-bottom: 0.25rem;
            font-size: 1.1rem;
        }

        .alert-value {
            font-size: 1rem;
            color: var(--dark);
            margin-bottom: 0.5rem;
        }

        .who-assessment {
            color: var(--dark);
            font-size: 0.95rem;
            margin-top: 0.5rem;
            padding: 0.5rem;
            background: rgba(0,0,0,0.05);
            border-radius: 0.25rem;
        }

        .who-limits {
            font-size: 0.85rem;
            color: var(--dark);
            opacity: 0.8;
            margin-top: 0.5rem;
            border-top: 1px solid rgba(0,0,0,0.1);
            padding-top: 0.5rem;
        }

        .chart-container {
            background: white;
            padding: 1rem;
            border-radius: 15px;
            height: 400px;
            margin-bottom: 1.5rem;
        }
    </style>
</head>
<body>
    <div class="topbar">
        <div class="container">
            <h1 class="h3 mb-0">Environmental Monitoring System</h1>
            <div class="status-bar">
                <div class="last-update">
                    Last update: <span id="lastUpdate"><?php echo date('H:i:s', strtotime($latest['timestamp'])); ?></span>
                </div>
            </div>
        </div>
    </div>

    <div class="container">

        <!-- Sensor Cards -->
        <div class="row">
            <div class="col-md-4">
                <div class="sensor-card temperature">
                    <div class="d-flex justify-content-between align-items-center">
                        <div>
                            <h4>Temperature</h4>
                            <div class="sensor-value">
                                <span id="currentTemp"><?php echo number_format($latest['temperature'], 1); ?></span>°C
                            </div>
                        </div>
                        <i class="fas fa-temperature-high sensor-icon"></i>
                    </div>
                </div>
            </div>
            <div class="col-md-4">
                <div class="sensor-card humidity">
                    <div class="d-flex justify-content-between align-items-center">
                        <div>
                            <h4>Humidity</h4>
                            <div class="sensor-value">
                                <span id="currentHumidity"><?php echo number_format($latest['humidity'], 1); ?></span>%
                            </div>
                        </div>
                        <i class="fas fa-tint sensor-icon"></i>
                    </div>
                </div>
            </div>
            <div class="col-md-4">
                <div class="sensor-card pressure">
                    <div class="d-flex justify-content-between align-items-center">
                        <div>
                            <h4>Pressure</h4>
                            <div class="sensor-value">
                                <span id="currentPressure"><?php echo number_format($latest['pressure'], 1); ?></span>hPa
                            </div>
                        </div>
                        <i class="fas fa-tachometer-alt sensor-icon"></i>
                    </div>
                </div>
            </div>
        </div>

        <div class="row">
            <div class="col-md-4">
                <div class="sensor-card co2">
                    <div class="d-flex justify-content-between align-items-center">
                        <div>
                            <h4>CO2</h4>
                            <div class="sensor-value">
                                <span id="currentCO2"><?php echo number_format($latest['co2'], 1); ?></span>ppm
                            </div>
                        </div>
                        <i class="fas fa-cloud sensor-icon"></i>
                    </div>
                </div>
                <div id="co2Assessment" class="assessment-text"></div>
            </div>
            <div class="col-md-4">
                <div class="sensor-card dust">
                    <div class="d-flex justify-content-between align-items-center">
                        <div>
                            <h4>Dust (PM2.5)</h4>
                            <div class="sensor-value">
                                <span id="currentDust"><?php echo number_format($latest['dust'], 1); ?></span>µg/m³
                            </div>
                        </div>
                        <i class="fas fa-smog sensor-icon"></i>
                    </div>
                </div>
                <div id="dustAssessment" class="assessment-text"></div>
            </div>
            <div class="col-md-4">
                <div class="sensor-card aqi">
                    <div class="d-flex justify-content-between align-items-center">
                        <div>
                            <h4>AQI</h4>
                            <div class="sensor-value">
                                <span id="currentAQI"><?php echo number_format($latest['aqi'], 1); ?></span>
                            </div>
                        </div>
                        <i class="fas fa-wind sensor-icon"></i>
                    </div>
                </div>
                <div id="aqiAssessment" class="assessment-text"></div>
            </div>
        </div>
        <!-- Air Quality Assessment Section -->
        <div class="alerts-section">
            <h4 class="mb-4">Đánh giá chất lượng không khí theo WHO</h4>
            <div id="alertsContainer">
                <!-- Alerts will be dynamically inserted here -->
            </div>
        </div>
        <!-- Charts -->
        <div class="row">
            <div class="col-md-6">
                <div class="card">
                    <div class="card-header d-flex justify-content-between align-items-center">
                        <h6 class="font-weight-bold text-primary mb-0">Temperature History</h6>
                        <select class="form-select form-select-sm w-auto" onchange="changeTimeRange(this.value)">
                            <option value="1">1 hour</option>
                            <option value="6">6 hours</option>
                            <option value="12">12 hours</option>
                            <option value="24" selected>24 hours</option>
                            <option value="48">48 hours</option>
                        </select>
                    </div>
                    <div class="card-body chart-container">
                        <canvas id="tempChart"></canvas>
                    </div>
                </div>
            </div>
            <div class="col-md-6">
                <div class="card">
                    <div class="card-header d-flex justify-content-between align-items-center">
                        <h6 class="font-weight-bold text-primary mb-0">Humidity History</h6>
                        <select class="form-select form-select-sm w-auto" onchange="changeTimeRange(this.value)">
                            <option value="1">1 hour</option>
                            <option value="6">6 hours</option>
                            <option value="12">12 hours</option>
                            <option value="24" selected>24 hours</option>
                            <option value="48">48 hours</option>
                        </select>
                    </div>
                    <div class="card-body chart-container">
                        <canvas id="humidityChart"></canvas>
                    </div>
                </div>
            </div>
        </div>

        <div class="row">
            <div class="col-md-6">
                <div class="card">
                    <div class="card-header d-flex justify-content-between align-items-center">
                        <h6 class="font-weight-bold text-primary mb-0">CO2 & AQI History</h6>
                        <select class="form-select form-select-sm w-auto" onchange="changeTimeRange(this.value)">
                            <option value="1">1 hour</option>
                            <option value="6">6 hours</option>
                            <option value="12">12 hours</option>
                            <option value="24" selected>24 hours</option>
                            <option value="48">48 hours</option>
                        </select>
                    </div>
                    <div class="card-body chart-container">
                        <canvas id="co2Chart"></canvas>
                    </div>
                </div>
            </div>
            <div class="col-md-6">
                <div class="card">
                    <div class="card-header d-flex justify-content-between align-items-center">
                        <h6 class="font-weight-bold text-primary mb-0">Dust Level History</h6>
                        <select class="form-select form-select-sm w-auto" onchange="changeTimeRange(this.value)">
                            <option value="1">1 hour</option>
                            <option value="6">6 hours</option>
                            <option value="12">12 hours</option>
                            <option value="24" selected>24 hours</option>
                            <option value="48">48 hours</option>
                        </select>
                    </div>
                    <div class="card-body chart-container">
                        <canvas id="dustChart"></canvas>
                    </div>
                </div>
            </div>
        </div>
    </div>

    <script src="https://cdn.jsdelivr.net/npm/moment@2.29.1/moment.min.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/chart.js@3.7.0/dist/chart.min.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/chartjs-adapter-moment@1.0.0/dist/chartjs-adapter-moment.min.js"></script>

    <script>
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
                co2: { label: 'Nồng độ CO₂', value: data.co2.value },
                dust: { label: 'Bụi mịn PM2.5', value: data.dust.value },
                aqi: { label: 'Chỉ số AQI', value: data.aqi.value }
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

        let tempChart, humidityChart, co2Chart, dustChart;
        let timeRange = 24;

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
                                hour: 'HH:mm'
                            }
                        },
                        title: {
                            display: true,
                            text: 'Time'
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
            tempChart = createChart('tempChart', 'Temperature (°C)', '#4e73df', createChartOptions('Temperature (°C)'));
            humidityChart = createChart('humidityChart', 'Humidity (%)', '#36b9cc', createChartOptions('Humidity (%)'));
            co2Chart = createChart('co2Chart', 'CO2 (ppm) & AQI', '#f6c23e', {
                ...createChartOptions('Level'),
                scales: {
                    ...createChartOptions('Level').scales,
                    y: {
                        title: {
                            display: true,
                            text: 'Level'
                        }
                    }
                }
            });
            dustChart = createChart('dustChart', 'Dust (µg/m³)', '#e74a3b', createChartOptions('Dust (µg/m³)'));
        }

        function updateCharts(data) {
            if (!data || !data.chart_data) return;

            const chartData = data.chart_data;
            const timestamps = chartData.timestamps;

            // Update temperature chart
            tempChart.data.labels = timestamps;
            tempChart.data.datasets[0].data = chartData.temperatures;
            tempChart.update();

            // Update humidity chart
            humidityChart.data.labels = timestamps;
            humidityChart.data.datasets[0].data = chartData.humidities;
            humidityChart.update();

            // Update CO2 & AQI chart
            co2Chart.data.labels = timestamps;
            co2Chart.data.datasets = [
                {
                    label: 'CO2 (ppm)',
                    data: chartData.co2_levels,
                    borderColor: '#f6c23e',
                    backgroundColor: 'transparent',
                    tension: 0.4
                },
                {
                    label: 'AQI',
                    data: chartData.aqi_levels,
                    borderColor: '#858796',
                    backgroundColor: 'transparent',
                    tension: 0.4
                }
            ];
            co2Chart.update();

            // Update dust chart
            dustChart.data.labels = timestamps;
            dustChart.data.datasets[0].data = chartData.dust_levels;
            dustChart.update();
        }

        function updateSensorValues(data) {
            if (!data || !data.latest_data) return;

            const latest = data.latest_data;
            // Update values
            document.getElementById('currentTemp').textContent = parseFloat(latest.temperature).toFixed(1);
            document.getElementById('currentHumidity').textContent = parseFloat(latest.humidity).toFixed(1);
            document.getElementById('currentPressure').textContent = parseFloat(latest.pressure).toFixed(1);
            document.getElementById('currentCO2').textContent = parseFloat(latest.co2).toFixed(1);
            document.getElementById('currentDust').textContent = parseFloat(latest.dust).toFixed(1);
            document.getElementById('currentAQI').textContent = parseFloat(latest.aqi).toFixed(1);

            // Update assessment text for each sensor card
            const co2Value = parseFloat(latest.co2);
            const dustValue = parseFloat(latest.dust);
            const aqiValue = parseFloat(latest.aqi);

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

            updateAssessmentElement('co2Assessment', co2Value, 'co2');
            updateAssessmentElement('dustAssessment', dustValue, 'dust');
            updateAssessmentElement('aqiAssessment', aqiValue, 'aqi');

            // Update WHO assessment section
            updateAlertsSection({
                co2: { value: parseFloat(latest.co2) },
                dust: { value: parseFloat(latest.dust) },
                aqi: { value: parseFloat(latest.aqi) }
            });
            
            document.getElementById('lastUpdate').textContent = moment(latest.timestamp).format('HH:mm:ss');
        }

        function changeTimeRange(hours) {
            timeRange = parseInt(hours);
            fetchData();
        }

        function fetchData() {
            fetch(`?action=fetchData&time_range=${timeRange}`)
                .then(response => response.json())
                .then(data => {
                    updateCharts(data);
                    updateSensorValues(data);
                })
                .catch(error => console.error('Error:', error));
        }

        // Initialize charts and start periodic updates
        document.addEventListener('DOMContentLoaded', function() {
            initCharts();
            fetchData();
            setInterval(fetchData, 5000); // Update every 5 seconds
        });
    </script>
</body>
</html>
