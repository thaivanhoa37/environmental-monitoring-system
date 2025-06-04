<?php
if (isset($_GET['action']) && $_GET['action'] === 'fetchData') {
    header('Content-Type: application/json');

    $servername = "localhost";
    $username = "admin";
    $password = "admin";
    $dbname = "sensor_data";

    $conn = new mysqli($servername, $username, $password, $dbname);
    if ($conn->connect_error) {
        die(json_encode(['error' => 'Kết nối thất bại: ' . $conn->connect_error]));
    }

    $time_range = isset($_GET['time_range']) ? intval($_GET['time_range']) : 24;

    $sql = "SELECT * FROM sensor_datanode2 WHERE timestamp >= NOW() - INTERVAL ? HOUR ORDER BY timestamp ASC";
    $stmt = $conn->prepare($sql);
    $stmt->bind_param("i", $time_range);
    $stmt->execute();
    $result = $stmt->get_result();

    $chart_data = [
        'timestamps' => [],
        'temperatures' => [],
        'humidities' => [],
        'dust_densities' => [],
        'aqis' => []
    ];

    if ($result->num_rows > 0) {
        while($row = $result->fetch_assoc()) {
            $chart_data['timestamps'][] = $row["timestamp"];
            $chart_data['temperatures'][] = $row["temperature"];
            $chart_data['humidities'][] = $row["humidity"];
            $chart_data['dust_densities'][] = $row["dust_density"];
            $chart_data['aqis'][] = $row["aqi"];
        }
    }

    $stmt->close();

    $sql_latest = "SELECT * FROM sensor_datanode2 ORDER BY timestamp DESC LIMIT 1";
    $result_latest = $conn->query($sql_latest);

    $latest_data = [];
    if ($result_latest->num_rows > 0) {
        $row = $result_latest->fetch_assoc();
        $latest_data = [
            'temperature' => $row["temperature"],
            'humidity' => $row["humidity"],
            'dust_density' => $row["dust_density"],
            'aqi' => $row["aqi"],
            'timestamp' => $row["timestamp"]
        ];
    }

    $conn->close();

    echo json_encode([
        'chart_data' => $chart_data,
        'latest_data' => $latest_data,
        'time_range' => $time_range
    ]);
    exit;
}
?>

<!DOCTYPE html>
<html lang="vi">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Dữ liệu Cảm biến Node 2</title>
    <style>
        /* Reset mặc định và thiết lập font */
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: 'Segoe UI', Arial, sans-serif;
            background-color: #f4f7fa;
            color: #333;
            line-height: 1.6;
            padding: 20px;
        }

        /* Container chính */
        h2 {
            color: #2c3e50;
            margin-bottom: 20px;
            font-size: 28px;
            text-align: center;
        }

        /* Form lọc thời gian */
        .filter-form {
            background-color: #fff;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
            margin-bottom: 30px;
            display: flex;
            flex-wrap: wrap;
            gap: 15px;
            align-items: center;
        }

        .filter-form label {
            font-weight: 500;
            color: #34495e;
        }

        .filter-form select,
        .filter-form button {
            padding: 10px 15px;
            border: 1px solid #ddd;
            border-radius: 5px;
            font-size: 16px;
            transition: all 0.3s ease;
        }

        .filter-form select:focus,
        .filter-form button:focus {
            outline: none;
            border-color: #3498db;
            box-shadow: 0 0 5px rgba(52, 152, 219, 0.3);
        }

        .filter-form button {
            background-color: #3498db;
            color: #fff;
            border: none;
            cursor: pointer;
        }

        .filter-form button:hover {
            background-color: #2980b9;
        }

        /* Nút điều khiển thiết bị */
        .device-control {
            background-color: #fff;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
            margin-bottom: 30px;
            text-align: center;
        }

        .device-control h2 {
            font-size: 24px;
            margin-bottom: 15px;
        }

        .device-control .button-group {
            display: flex;
            justify-content: center;
            gap: 15px;
            flex-wrap: wrap;
        }

        .device-control button {
            padding: 10px 20px;
            border: none;
            border-radius: 5px;
            font-size: 16px;
            transition: all 0.3s ease;
            cursor: pointer;
        }

        .device-control button[id$="Button"] {
            background-color: #2ecc71;
            color: #fff;
        }

        .device-control button[id$="Button"]:hover {
            background-color: #27ae60;
        }

        .device-control button[data-state="OFF"] {
            background-color: #e74c3c;
        }

        .device-control button[data-state="OFF"]:hover {
            background-color: #c0392b;
        }

        /* Phần cập nhật ngưỡng */
        .threshold-control {
            background-color: #fff;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
            margin-bottom: 30px;
            text-align: center;
        }

        .threshold-control h2 {
            font-size: 24px;
            margin-bottom: 15px;
        }

        .threshold-control .input-group {
            display: flex;
            justify-content: center;
            gap: 15px;
            flex-wrap: wrap;
            align-items: center;
        }

        .threshold-control label {
            font-weight: 500;
            color: #34495e;
        }

        .threshold-control input {
            padding: 10px 15px;
            border: 1px solid #ddd;
            border-radius: 5px;
            font-size: 16px;
            width: 100px;
            transition: all 0.3s ease;
        }

        .threshold-control input:focus {
            outline: none;
            border-color: #3498db;
            box-shadow: 0 0 5px rgba(52, 152, 219, 0.3);
        }

        .threshold-control button {
            padding: 10px 20px;
            border: none;
            border-radius: 5px;
            font-size: 16px;
            background-color: #3498db;
            color: #fff;
            transition: all 0.3s ease;
            cursor: pointer;
        }

        .threshold-control button:hover {
            background-color: #2980b9;
        }

        /* Bảng giá trị mới nhất */
        table {
            width: 100%;
            border-collapse: collapse;
            background-color: #fff;
            border-radius: 8px;
            overflow: hidden;
            box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
            margin-bottom: 30px;
        }

        th, td {
            padding: 12px 15px;
            text-align: center;
            border-bottom: 1px solid #eee;
        }

        th {
            background-color: #3498db;
            color: #fff;
            font-weight: 600;
        }

        td {
            color: #34495e;
        }

        /* Dự báo chất lượng không khí */
        .forecast-container {
            margin-bottom: 40px;
            overflow-x: auto;
            padding: 10px;
        }

        .forecast-table {
            display: flex;
            gap: 10px;
        }

        .forecast-column {
            background-color: #fff;
            border-radius: 8px;
            box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
            padding: 15px;
            text-align: center;
            width: 140px;
            transition: transform 0.3s ease;
        }

        .forecast-column:hover {
            transform: translateY(-5px);
        }

        .forecast-column .hour {
            font-weight: 600;
            color: #2c3e50;
            margin-bottom: 10px;
        }

        .forecast-column .temperature,
        .forecast-column .aqi,
        .forecast-column .dust-density,
        .forecast-column .humidity {
            display: flex;
            align-items: center;
            justify-content: center;
            margin-bottom: 10px;
            font-size: 14px;
            color: #34495e;
        }

        .forecast-column .temperature {
            font-size: 16px;
            font-weight: 500;
            color: #e74c3c;
        }

        .forecast-column .aqi {
            font-size: 16px;
            font-weight: 600;
            color: #3498db;
        }

        .forecast-column .icon {
            margin-right: 5px;
            font-size: 18px;
        }

        /* Container biểu đồ */
        .chart-container {
            background-color: #fff;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
            margin-bottom: 30px;
        }

        .chart-container h3 {
            display: flex;
            justify-content: space-between;
            align-items: center;
            cursor: pointer;
            color: #2c3e50;
            padding: 10px;
            font-size: 20px;
            transition: color 0.3s ease;
        }

        .chart-container h3:hover {
            color: #3498db;
        }

        .chart-container .toggle-icon {
            font-size: 14px;
            color: #7f8c8d;
        }

        .chart-container canvas {
            max-width: 100%;
            height: 300px;
            display: none;
            margin-top: 10px;
        }

        /* Responsive */
        @media (max-width: 768px) {
            .filter-form, .device-control .button-group, .threshold-control .input-group {
                flex-direction: column;
                align-items: stretch;
            }

            .device-control button, .threshold-control input, .threshold-control button {
                width: 100%;
                margin-bottom: 10px;
            }

            .forecast-column {
                width: 120px;
            }

            h2 {
                font-size: 24px;
            }

            .chart-container h3 {
                font-size: 18px;
            }
        }

        @media (max-width: 480px) {
            th, td {
                font-size: 14px;
                padding: 8px;
            }

            .forecast-column {
                width: 100px;
                padding: 10px;
            }

            .forecast-column .temperature,
            .forecast-column .aqi,
            .forecast-column .dust-density,
            .forecast-column .humidity {
                font-size: 12px;
            }
        }
    </style>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body>
    <h2>Dữ liệu Cảm biến Node 2</h2>

    <div class="device-control">
        <h2>Điều khiển thiết bị</h2>
        <div class="button-group">
            <button id="modeButton" onclick="toggleDevice('mode', this)" data-state="OFF">Mode: OFF</button>
            <button id="device1Button" onclick="toggleDevice('device1', this)" data-state="OFF">Device 1: OFF</button>
            <button id="device2Button" onclick="toggleDevice('device2', this)" data-state="OFF">Device 2: OFF</button>
        </div>
    </div>

    <div class="threshold-control">
        <h2>Cập nhật ngưỡng</h2>
        <div class="input-group">
            <label for="aqiThreshold">Ngưỡng AQI:</label>
            <input type="number" id="aqiThreshold" placeholder="Nhập ngưỡng AQI" min="0" step="1">
            <button onclick="updateThreshold('aqi')">Cập nhật AQI</button>
            <label for="temperatureThreshold">Ngưỡng Nhiệt độ (°C):</label>
            <input type="number" id="temperatureThreshold" placeholder="Nhập ngưỡng nhiệt độ" min="0" step="0.1">
            <button onclick="updateThreshold('temperature')">Cập nhật Nhiệt độ</button>
        </div>
    </div>

    <?php
    $servername = "localhost";
    $username = "admin";
    $password = "admin";
    $dbname = "sensor_data";

    $conn = new mysqli($servername, $username, $password, $dbname);
    if ($conn->connect_error) {
        die("Kết nối thất bại: " . $conn->connect_error);
    }

    $time_range = isset($_GET['time_range']) ? intval($_GET['time_range']) : 24;

    // Hàm chuyển đổi đơn vị thời gian
    function formatTimeRange($hours) {
        if ($hours >= 24 && $hours % 24 === 0) {
            $days = $hours / 24;
            return "$days ngày";
        }
        return "$hours giờ";
    }

    $sql = "SELECT * FROM sensor_datanode2 WHERE timestamp >= NOW() - INTERVAL ? HOUR ORDER BY timestamp ASC";
    $stmt = $conn->prepare($sql);
    $stmt->bind_param("i", $time_range);
    $stmt->execute();
    $result = $stmt->get_result();

    $timestamps = [];
    $temperatures = [];
    $humidities = [];
    $dust_densities = [];
    $aqis = [];

    if ($result->num_rows > 0) {
        while($row = $result->fetch_assoc()) {
            $timestamps[] = $row["timestamp"];
            $temperatures[] = $row["temperature"];
            $humidities[] = $row["humidity"];
            $dust_densities[] = $row["dust_density"];
            $aqis[] = $row["aqi"];
        }
    }

    $stmt->close();

    $sql_latest = "SELECT * FROM sensor_datanode2 ORDER BY timestamp DESC LIMIT 1";
    $result_latest = $conn->query($sql_latest);

    $latest_data = [];
    if ($result_latest->num_rows > 0) {
        $row = $result_latest->fetch_assoc();
        $latest_data = [
            'temperature' => $row["temperature"],
            'humidity' => $row["humidity"],
            'dust_density' => $row["dust_density"],
            'aqi' => $row["aqi"],
            'timestamp' => $row["timestamp"]
        ];
    }

    $sql_forecast = "SELECT * FROM forecast_results_node2 ORDER BY id DESC LIMIT 24";
    $result_forecast = $conn->query($sql_forecast);

    $forecast_data = [];
    if ($result_forecast->num_rows > 0) {
        while($row = $result_forecast->fetch_assoc()) {
            $forecast_data[] = [
                'timestamp' => $row["timestamp"],
                'temperature' => $row["temperature_forecast"],
                'humidity' => $row["humidity_forecast"],
                'dust_density' => $row["dust_density_forecast"],
                'aqi' => $row["aqi_forecast"]
            ];
        }
    }
    $forecast_data = array_reverse($forecast_data);

    $conn->close();
    ?>

    <h3>Giá trị mới nhất</h3>
    <table id="latestDataTable">
        <tr>
            <th>Nhiệt độ (°C)</th>
            <th>Độ ẩm (%)</th>
            <th>Mật độ bụi</th>
            <th>AQI</th>
            <th>Thời gian</th>
        </tr>
        <tr>
            <td id="latestTemperature"><?php echo $latest_data['temperature'] ?? '-'; ?></td>
            <td id="latestHumidity"><?php echo $latest_data['humidity'] ?? '-'; ?></td>
            <td id="latestDustDensity"><?php echo $latest_data['dust_density'] ?? '-'; ?></td>
            <td id="latestAqi"><?php echo $latest_data['aqi'] ?? '-'; ?></td>
            <td id="latestTimestamp"><?php echo $latest_data['timestamp'] ?? '-'; ?></td>
        </tr>
    </table>

    <h3>Dự báo chất lượng không khí trong 24 giờ tới</h3>
    <div class="forecast-container">
        <div class="forecast-table">
            <?php
            foreach ($forecast_data as $index => $forecast) {
                $hour = date('H:i', strtotime($forecast['timestamp']));
                $temperature = round($forecast['temperature'], 1);
                $humidity = round($forecast['humidity'], 1);
                $dust_density = round($forecast['dust_density'], 1);
                $aqi = number_format($forecast['aqi'], 1);
                ?>
                <div class="forecast-column">
                    <div class="hour"><?php echo $hour; ?></div>
                    <div class="aqi"><span class="icon">🌫️</span><?php echo $aqi; ?></div>
                    <div class="dust-density"><span class="icon">🌫️</span><?php echo $dust_density; ?> µg/m³</div>
                    <div class="temperature"><span class="icon">🌡️</span><?php echo $temperature; ?>°C</div>
                    <div class="humidity"><span class="icon">💧</span><?php echo $humidity; ?>%</div>
                </div>
                <?php
            }
            ?>
        </div>
    </div>

    <div class="filter-form">
        <form id="timeRangeForm" method="GET" action="">
            <label for="time_range">Chọn khoảng thời gian biểu đồ: </label>
            <select name="time_range" id="time_range">
                <option value="1" <?php echo (isset($_GET['time_range']) && $_GET['time_range'] == '1') ? 'selected' : ''; ?>>1 giờ</option>
                <option value="6" <?php echo (isset($_GET['time_range']) && $_GET['time_range'] == '6') ? 'selected' : ''; ?>>6 giờ</option>
                <option value="12" <?php echo (isset($_GET['time_range']) && $_GET['time_range'] == '12') ? 'selected' : ''; ?>>12 giờ</option>
                <option value="24" <?php echo (isset($_GET['time_range']) && $_GET['time_range'] == '24') ? 'selected' : 'selected'; ?>>1 ngày</option>
                <option value="168" <?php echo (isset($_GET['time_range']) && $_GET['time_range'] == '168') ? 'selected' : ''; ?>>7 ngày</option>
            </select>
            <button type="submit">Cập nhật</button>
        </form>
    </div>

    <div class="chart-container" id="temperatureContainer">
        <h3 onclick="toggleChart('temperatureContainer', 'temperatureIcon')">
            <span class="chart-title">Nhiệt độ (°C) - <span class="time-range-label"><?php echo formatTimeRange($time_range); ?></span></span>
            <span class="toggle-icon" id="temperatureIcon">[Hiện]</span>
        </h3>
        <canvas id="temperatureChart"></canvas>
    </div>
    <div class="chart-container" id="humidityContainer">
        <h3 onclick="toggleChart('humidityContainer', 'humidityIcon')">
            <span class="chart-title">Độ ẩm (%) - <span class="time-range-label"><?php echo formatTimeRange($time_range); ?></span></span>
            <span class="toggle-icon" id="humidityIcon">[Hiện]</span>
        </h3>
        <canvas id="humidityChart"></canvas>
    </div>
    <div class="chart-container" id="dustDensityContainer">
        <h3 onclick="toggleChart('dustDensityContainer', 'dustDensityIcon')">
            <span class="chart-title">Mật độ bụi - <span class="time-range-label"><?php echo formatTimeRange($time_range); ?></span></span>
            <span class="toggle-icon" id="dustDensityIcon">[Hiện]</span>
        </h3>
        <canvas id="dustDensityChart"></canvas>
    </div>
    <div class="chart-container" id="aqiContainer">
        <h3 onclick="toggleChart('aqiContainer', 'aqiIcon')">
            <span class="chart-title">AQI - <span class="time-range-label"><?php echo formatTimeRange($time_range); ?></span></span>
            <span class="toggle-icon" id="aqiIcon">[Hiện]</span>
        </h3>
        <canvas id="aqiChart"></canvas>
    </div>

    <script>
        let timestamps = <?php echo json_encode($timestamps); ?>;
        let temperatures = <?php echo json_encode($temperatures); ?>;
        let humidities = <?php echo json_encode($humidities); ?>;
        let dust_densities = <?php echo json_encode($dust_densities); ?>;
        let aqis = <?php echo json_encode($aqis); ?>;

        let temperatureChart, humidityChart, dustDensityChart, aqiChart;

        function createChart(canvasId, label, data, color) {
            const ctx = document.getElementById(canvasId).getContext('2d');
            return new Chart(ctx, {
                type: 'line',
                data: {
                    labels: timestamps,
                    datasets: [{
                        label: label,
                        data: data,
                        borderColor: color,
                        fill: false
                    }]
                },
                options: {
                    responsive: true,
                    scales: {
                        x: {
                            title: {
                                display: true,
                                text: 'Thời gian'
                            }
                        },
                        y: {
                            title: {
                                display: true,
                                text: label
                            },
                            beginAtZero: true
                        }
                    }
                }
            });
        }

        function toggleChart(containerId, iconId) {
            const container = document.getElementById(containerId);
            const canvas = container.querySelector('canvas');
            const icon = document.getElementById(iconId);
            
            if (!canvas || !icon) {
                console.error('Không tìm thấy canvas hoặc icon cho container: ' + containerId);
                return;
            }

            if (canvas.style.display === 'none') {
                canvas.style.display = 'block';
                icon.textContent = '[Ẩn]';
            } else {
                canvas.style.display = 'none';
                icon.textContent = '[Hiện]';
            }
        }

        function updateCharts(chartData) {
            timestamps = chartData.timestamps;
            temperatures = chartData.temperatures;
            humidities = chartData.humidities;
            dust_densities = chartData.dust_densities;
            aqis = chartData.aqis;

            temperatureChart.data.labels = timestamps;
            temperatureChart.data.datasets[0].data = temperatures;
            temperatureChart.update();

            humidityChart.data.labels = timestamps;
            humidityChart.data.datasets[0].data = humidities;
            humidityChart.update();

            dustDensityChart.data.labels = timestamps;
            dustDensityChart.data.datasets[0].data = dust_densities;
            dustDensityChart.update();

            aqiChart.data.labels = timestamps;
            aqiChart.data.datasets[0].data = aqis;
            aqiChart.update();
        }

        function updateTable(latestData) {
            document.getElementById('latestTemperature').textContent = latestData.temperature ?? '-';
            document.getElementById('latestHumidity').textContent = latestData.humidity ?? '-';
            document.getElementById('latestDustDensity').textContent = latestData.dust_density ?? '-';
            document.getElementById('latestAqi').textContent = latestData.aqi ?? '-';
            document.getElementById('latestTimestamp').textContent = latestData.timestamp ?? '-';
        }

        function formatTimeRange(hours) {
            if (hours >= 24 && hours % 24 === 0) {
                const days = hours / 24;
                return `${days} ngày`;
            }
            return `${hours} giờ`;
        }

        function updateChartTitles(timeRange) {
            const labels = document.querySelectorAll('.time-range-label');
            labels.forEach(label => {
                label.textContent = formatTimeRange(timeRange);
            });
        }

        function fetchData() {
            const timeRange = document.getElementById('time_range').value;
            fetch(`?action=fetchData&time_range=${timeRange}`)
                .then(response => response.json())
                .then(data => {
                    if (data.error) {
                        console.error('Lỗi lấy dữ liệu:', data.error);
                        return;
                    }
                    updateCharts(data.chart_data);
                    updateTable(data.latest_data);
                    updateChartTitles(data.time_range);
                })
                .catch(error => console.error('Lỗi fetch:', error));
        }

        function toggleDevice(type, button) {
            let currentState = button.getAttribute('data-state');
            let newState = currentState === 'OFF' ? 'ON' : 'OFF';

            const hostname = window.location.hostname;
            const nodeRedUrl = `http://${hostname}:1880/node2/control`;

            fetch(nodeRedUrl, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    type: type,
                    state: newState
                })
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    button.setAttribute('data-state', newState);
                    if (type === 'mode') {
                        button.textContent = `Mode: ${newState}`;
                    } else if (type === 'device1') {
                        button.textContent = `Device 1: ${newState}`;
                    } else if (type === 'device2') {
                        button.textContent = `Device 2: ${newState}`;
                    }
                } else {
                    alert('Lỗi: ' + data.error);
                }
            })
            .catch(error => {
                console.error('Lỗi khi gửi yêu cầu:', error);
                alert('Lỗi khi gửi yêu cầu tới Node-RED');
            });
        }

        function updateThreshold(type) {
            let value;
            let payload = {};
            let inputElement;

            if (type === 'aqi') {
                inputElement = document.getElementById('aqiThreshold');
                value = inputElement.value;
                if (value === '') {
                    alert('Vui lòng nhập ngưỡng AQI!');
                    return;
                }
                payload.aqi_threshold = parseInt(value);
            } else if (type === 'temperature') {
                inputElement = document.getElementById('temperatureThreshold');
                value = inputElement.value;
                if (value === '') {
                    alert('Vui lòng nhập ngưỡng nhiệt độ!');
                    return;
                }
                payload.temperature_threshold = parseFloat(value);
            }

            const hostname = window.location.hostname;
            const nodeRedUrl = `http://${hostname}:1880/node2/control`;

            fetch(nodeRedUrl, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(payload)
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    alert(`Cập nhật ngưỡng ${type === 'aqi' ? 'AQI' : 'nhiệt độ'} thành công!`);
                    // Xóa giá trị trong ô nhập liệu sau khi cập nhật thành công
                    inputElement.value = '';
                } else {
                    alert('Lỗi: ' + data.error);
                }
            })
            .catch(error => {
                console.error('Lỗi khi gửi yêu cầu:', error);
                alert('Lỗi khi gửi yêu cầu tới Node-RED');
            });
        }

        if (timestamps.length > 0) {
            temperatureChart = createChart('temperatureChart', 'Nhiệt độ (°C)', temperatures, 'red');
            humidityChart = createChart('humidityChart', 'Độ ẩm (%)', humidities, 'blue');
            dustDensityChart = createChart('dustDensityChart', 'Mật độ bụi', dust_densities, 'green');
            aqiChart = createChart('aqiChart', 'AQI', aqis, 'purple');
        } else {
            console.log('Không có dữ liệu để vẽ biểu đồ.');
        }

        setInterval(fetchData, 300000);

        document.getElementById('timeRangeForm').addEventListener('submit', function(event) {
            event.preventDefault();
            const timeRange = document.getElementById('time_range').value;
            const newUrl = window.location.pathname + `?time_range=${timeRange}`;
            window.history.pushState({}, '', newUrl);
            fetchData();
        });

        fetchData();

        // WebSocket để nhận trạng thái từ Node-RED
        document.addEventListener('DOMContentLoaded', function() {
            const ws = new WebSocket('ws://' + window.location.hostname + ':1880/ws/node2/state');

            ws.onopen = function() {
                console.log('Kết nối WebSocket thành công');
            };

            ws.onmessage = function(event) {
                const data = JSON.parse(event.data);
                const type = data.type;
                const state = data.state;

                const button = document.getElementById(type + 'Button');
                if (button) {
                    button.setAttribute('data-state', state);
                    if (type === 'mode') {
                        button.textContent = `Mode: ${state}`;
                    } else if (type === 'device1') {
                        button.textContent = `Device 1: ${state}`;
                    } else if (type === 'device2') {
                        button.textContent = `Device 2: ${state}`;
                    }
                }
            };

            ws.onerror = function(error) {
                console.error('Lỗi WebSocket:', error);
            };

            ws.onclose = function() {
                console.log('Kết nối WebSocket đã đóng, thử kết nối lại...');
                setTimeout(function() {
                    location.reload();
                }, 5000);
            };
        });
    </script>
</body>
</html>