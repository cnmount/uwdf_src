/*
 * main.js - Frontend logic for UWDF Platform
 * Copyright (C) 2025 UWDF Team
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
const API_BASE = "https://localhost:5000";
let currentUserId = null;

// WebSocket for real-time data
const ws = new WebSocket("wss://localhost:5000/receive_data");

ws.onmessage = (event) => {
    const data = JSON.parse(event.data);
    updateSensorDisplay(data);
};

// Show/hide pages
function showPage(pageId) {
    document.querySelectorAll(".page").forEach(page => {
        page.classList.remove("active");
    });
    document.getElementById(pageId).classList.add("active");
}

// Update sensor status display
function updateSensorDisplay(data) {
    const sensorList = document.getElementById("sensor-list");
    sensorList.innerHTML = "";
    for (const [sensorName, sensorData] of Object.entries(data)) {
        const sensorDiv = document.createElement("div");
        sensorDiv.className = "sensor";
        sensorDiv.innerHTML = `
            <h3>${sensorName}</h3>
            <p>ID: ${sensorData.sensorId}</p>
            <p>Value: ${sensorData.value} ${sensorName === "heart_rate" ? "bpm" : sensorName === "temperature" ? "°C" : "intensity"}</p>
            <p>Timestamp: ${sensorData.timestamp}</p>
            <button onclick="toggleSensor('${sensorData.sensorId}')">${sensorData.active ? "Deactivate" : "Activate"}</button>
        `;
        sensorList.appendChild(sensorDiv);
    }
}

// Toggle sensor state
function toggleSensor(sensorId) {
    console.log(`Toggling sensor ${sensorId}`);
    // Send request to server to toggle sensor
}

// Login
document.getElementById("account-form").addEventListener("submit", async (e) => {
    e.preventDefault();
    const userId = document.getElementById("user-id").value;
    const password = document.getElementById("password").value;
    
    const response = await fetch(`${API_BASE}/auth/login`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ user_id: userId, password })
    });
    
    const result = await response.json();
    if (response.ok) {
        currentUserId = userId;
        alert("Login successful!");
    } else {
        alert("Login failed: " + result.error);
    }
});

// Load authorized sensors
async function loadAuthorizedSensors() {
    if (!currentUserId) return;
    const response = await fetch(`${API_BASE}/sensors/authorized`, {
        headers: { "user_id": currentUserId }
    });
    const data = await response.json();
    const userList = document.getElementById("user-list");
    userList.innerHTML = `<p>Authorized Sensors: ${data.sensors.join(", ")}</p>`;
}

// Add new user
document.getElementById("add-user-form").addEventListener("submit", async (e) => {
    e.preventDefault();
    const newUserId = document.getElementById("new-user-id").value;
    const sensors = Array.from(document.getElementById("sensors").selectedOptions).map(opt => opt.value);
    
    console.log(`Adding user ${newUserId} with access to ${sensors}`);
    // Send request to server to add user
    alert("User added");
});

// Initial load
document.addEventListener("DOMContentLoaded", () => {
    showPage("sensor-status");
    loadAuthorizedSensors();
});