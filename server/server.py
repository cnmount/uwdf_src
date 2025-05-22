# server.py - Flask server for UWDF Platform
# Copyright (C) 2025 UWDF Team
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

from flask import Flask, request, jsonify
import sqlite3
import time
import json
import os
from datetime import datetime
from cryptography.fernet import Fernet
from flask_cors import CORS

app = Flask(__name__)
CORS(app)  # Enable CORS for web interface

# SQLite database configuration
DB_PATH = "uwdf_data.db"
BACKUP_PATH = "backups/"

# Encryption key for backups
key = Fernet.generate_key()
cipher = Fernet(key)

def init_db():
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute('''
        CREATE TABLE IF NOT EXISTS Sensor_Data (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            sensor_id TEXT,
            sensor_type TEXT,
            value REAL,
            timestamp TEXT
        )
    ''')
    c.execute('''
        CREATE TABLE IF NOT EXISTS User_Access_Log (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER,
            timestamp TEXT,
            outcome TEXT
        )
    ''')
    conn.commit()
    conn.close()

def backup_db():
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    backup_file = os.path.join(BACKUP_PATH, f"backup_{timestamp}.db")
    with open(DB_PATH, "rb") as src, open(backup_file, "wb") as dst:
        data = src.read()
        encrypted_data = cipher.encrypt(data)
        dst.write(encrypted_data)

# Receive sensor data from ESP32S2
@app.route("/receive_data", methods=["POST"])
def receive_data():
    data = request.get_json()
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    
    for sensor_name, sensor_data in data.items():
        c.execute('''
            INSERT INTO Sensor_Data (sensor_id, sensor_type, value, timestamp)
            VALUES (?, ?, ?, ?)
        ''', (
            sensor_data["sensorId"],
            sensor_name,
            sensor_data["value"],
            sensor_data["timestamp"]
        ))
    
    conn.commit()
    conn.close()
    return jsonify({"status": "success"}), 200

# Authentication endpoint
@app.route("/auth/login", methods=["POST"])
def login():
    credentials = request.get_json()
    user_id = credentials.get("user_id")
    # Authentication logic (replace with secure implementation)
    if user_id in [1, 2]:
        token = f"token_{user_id}_{int(time.time())}"
        conn = sqlite3.connect(DB_PATH)
        c = conn.cursor()
        c.execute('''
            INSERT INTO User_Access_Log (user_id, timestamp, outcome)
            VALUES (?, ?, ?)
        ''', (user_id, datetime.now().isoformat(), "success"))
        conn.commit()
        conn.close()
        return jsonify({"access_token": token}), 200
    else:
        conn = sqlite3.connect(DB_PATH)
        c = conn.cursor()
        c.execute('''
            INSERT INTO User_Access_Log (user_id, timestamp, outcome)
            VALUES (?, ?, ?)
        ''', (user_id, datetime.now().isoformat(), "failed"))
        conn.commit()
        conn.close()
        return jsonify({"error": "Unauthorized"}), 401

# Get authorized sensors
@app.route("/sensors/authorized", methods=["GET"])
def get_authorized_sensors():
    user_id = request.headers.get("user_id")
    # Permission data (replace with database-driven logic)
    permissions = {
        "1": ["heart_rate", "temperature"],
        "2": ["motion"]
    }
    return jsonify({"sensors": permissions.get(user_id, [])}), 200

# Get real-time sensor data
@app.route("/sensors/data/<sensor_id>", methods=["GET"])
def get_sensor_data(sensor_id):
    user_id = request.headers.get("user_id")
    # Permission check
    permissions = {
        "1": ["HR001", "TEMP001"],
        "2": ["MOT001"]
    }
    if user_id not in permissions or sensor_id not in permissions[user_id]:
        return jsonify({"error": "Forbidden"}), 403
    
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute('''
        SELECT sensor_id, sensor_type, value, timestamp
        FROM Sensor_Data
        WHERE sensor_id = ?
        ORDER BY timestamp DESC
        LIMIT 1
    ''', (sensor_id,))
    data = c.fetchone()
    conn.close()
    
    if data:
        return jsonify({
            "sensorId": data[0],
            "type": data[1],
            "value": data[2],
            "timestamp": data[3],
            "unit": "bpm" if data[1] == "heart_rate" else "°C" if data[1] == "temperature" else "intensity"
        }), 200
    return jsonify({"error": "Not found"}), 404

# Get historical sensor data
@app.route("/sensors/history/<sensor_id>", methods=["GET"])
def get_sensor_history(sensor_id):
    user_id = request.headers.get("user_id")
    # Permission check
    permissions = {
        "1": ["HR001", "TEMP001"],
        "2": ["MOT001"]
    }
    if user_id not in permissions or sensor_id not in permissions[user_id]:
        return jsonify({"error": "Forbidden"}), 403
    
    start_time = request.args.get("start_time")
    end_time = request.args.get("end_time")
    
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute('''
        SELECT sensor_id, sensor_type, value, timestamp
        FROM Sensor_Data
        WHERE sensor_id = ? AND timestamp BETWEEN ? AND ?
    ''', (sensor_id, start_time, end_time))
    data = c.fetchall()
    conn.close()
    
    return jsonify([{
        "sensorId": row[0],
        "type": row[1],
        "value": row[2],
        "timestamp": row[3],
        "unit": "bpm" if row[1] == "heart_rate" else "°C" if data[1] == "temperature" else "intensity"
    } for row in data]), 200

if __name__ == "__main__":
    os.makedirs(BACKUP_PATH, exist_ok=True)
    init_db()
    # Schedule periodic backups
    import threading
    def periodic_backup():
        while True:
            time.sleep(24 * 3600)  # Every 24 hours
            backup_db()
    threading.Thread(target=periodic_backup, daemon=True).start()
    app.run(host="0.0.0.0", port=5000, ssl_context="adhoc")  # Enable HTTPS