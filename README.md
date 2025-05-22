# UWDF Platform

The UWDF (Universal Wearable Data Framework) Platform is a prototype for healthcare monitoring, integrating sensor data collection, processing, storage, and visualization. It uses an ESP32S2 development board with an I2C HUB for sensor integration, a laptop-based server for data processing and storage, and a web interface for user interaction.

## Features
- **Sensor Data Collection**: Collects data from sensors (heart rate and SpO2 via MAX30100, temperature, motion) via ESP32S2.
- **Data Processing**: Averages heart rate and SpO2 using an exponential moving average; smooths temperature and motion data with a simple moving average.
- **Data Storage**: Stores data in a local SQLite database with periodic encrypted backups.
- **Web Interface**: Displays real-time and historical sensor data, supports account and user management.
- **API Access**: Provides secure APIs for authenticated clients to access sensor data.

## Repository Structure
```
UWDF-Prototype/
├── src/
│   ├── uwdf_main.ino                   # ESP32S2 main program
│   ├── XLOT_MAX301xx_PulseOximeter.h   # MAX30100 sensor header
│   └── XLOT_MAX301xx_PulseOximeter.cpp # MAX30100 sensor implementation
├── server/
│   ├── server.py                       # Laptop web server and API
│   └── database_schema.sql             # SQLite database schema
├── web/
│   ├── index.html                      # Web interface
│   ├── main.js                         # Frontend logic
│   └── styles.css                      # Interface styles
├── README.md                           # Project documentation
└── LICENSE                             # GNU General Public License v3
```

## Requirements

### Hardware
- ESP32S2 development board
- I2C HUB expansion board
- MAX30100 sensor (for heart rate and SpO2)
- Temperature and motion sensors (implementation-specific)
- Laptop for data processing and hosting the web server

### Software
- **ESP32S2**:
  - Arduino IDE or PlatformIO
  - Libraries: ArduinoJson, WebSocketServer, MAX30100 (included as `XLOT_MAX301xx_PulseOximeter.h/cpp`)
- **Laptop**:
  - Python 3.8+
  - Libraries: Flask, sqlite3, cryptography, flask-cors
- **Web Interface**:
  - Modern web browser (e.g., Chrome, Firefox)

## Installation
1. **ESP32S2 Setup**:
   - Install Arduino IDE or PlatformIO.
   - Install ArduinoJson and WebSocketServer libraries.
   - Place `XLOT_MAX301xx_PulseOximeter.h` and `XLOT_MAX301xx_PulseOximeter.cpp` in the project directory.
   - Update `WIFI_SSID` and `WIFI_PASSWORD` in `uwdf_main.ino`.
   - Upload `uwdf_main.ino` to the ESP32S2.
2. **Laptop Server Setup**:
   - Install Python dependencies: `pip install flask flask-cors cryptography`.
   - Initialize the SQLite database: `python -c "from server import init_db; init_db()"`.
   - Run the server: `python server.py`.
3. **Web Interface**:
   - Access the interface at `https://localhost:5000` in a browser.
   - Log in to view sensor data and manage users.

## Usage
- **Sensor Monitoring**: View real-time and historical sensor data (heart rate, SpO2, temperature, motion) on the web interface.
- **Account Management**: Update user credentials via the Account page.
- **User Management**: Add/remove users and set sensor access permissions.
- **API Access**: Use endpoints like `/sensors/data/<sensor_id>` with proper authentication.

## Notes
- The MAX30100 sensor provides heart rate and SpO2 data, processed with an exponential moving average. Temperature and motion sensor reads are placeholders; replace with actual I2C sensor implementations (e.g., BME280 for temperature, MPU6050 for motion).
- The server uses ad-hoc HTTPS for simplicity. In production, configure proper SSL certificates.
- Authentication in `server.py` is simplified; implement a secure system (e.g., JWT) for production.
- This project is licensed under the GNU General Public License v3 due to the inclusion of the GPL-licensed MAX30100 library. All derivative works must also be licensed under GPL v3.

## License
This project is licensed under the GNU General Public License v3. See the [LICENSE](LICENSE) file for details.