-- Create database if not exists
CREATE DATABASE IF NOT EXISTS environmental_monitoring_system CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

USE environmental_monitoring_system;

-- Create sensor_data table
CREATE TABLE IF NOT EXISTS sensor_data (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    temperature FLOAT NOT NULL,
    humidity FLOAT NOT NULL,
    pressure FLOAT NOT NULL,
    co2 FLOAT NOT NULL,
    dust FLOAT NOT NULL,
    aqi FLOAT NOT NULL,
    timestamp DATETIME NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_timestamp (timestamp)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;