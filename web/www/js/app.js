"use strict";

const API = "/cgi-bin/robot_api.cgi";
const POLL_STATE_MS = 600;
const POLL_MAP_MS = 1200;

const $ = (id) => document.getElementById(id);

const ui = {
  loginView: $("loginView"),
  dashboardView: $("dashboardView"),
  loginForm: $("loginForm"),
  loginError: $("loginError"),
  globalError: $("globalError"),
  connectionText: $("connectionText"),
  connectionDot: $("connectionDot"),
  logoutButton: $("logoutButton"),
  modeBadge: $("modeBadge"),
  autonomousModeButton: $("autonomousModeButton"),
  manualModeButton: $("manualModeButton"),
  manualControlCard: $("manualControlCard"),
  speedSlider: $("speedSlider"),
  speedValue: $("speedValue"),
  stopButton: $("stopButton"),
  sensorFront: $("sensorFront"),
  sensorLeft: $("sensorLeft"),
  sensorRight: $("sensorRight"),
  ledPower: $("ledPower"),
  ledAutonomous: $("ledAutonomous"),
  ledManual: $("ledManual"),
  ledObstacle: $("ledObstacle"),
  songSelect: $("songSelect"),
  playButton: $("playButton"),
  pauseButton: $("pauseButton"),
  audioStopButton: $("audioStopButton"),
  volumeSlider: $("volumeSlider"),
  volumeValue: $("volumeValue"),
  audioStatus: $("audioStatus"),
  mapCanvas: $("mapCanvas")
};

let loggedIn = false;
let currentMode = null;
let heldDirection = null;
let stateTimer = null;
let mapTimer = null;

function formBody(values) {
  return new URLSearchParams(values).toString();
}

async function apiGet(action) {
  const response = await fetch(`${API}?action=${encodeURIComponent(action)}`, {
    cache: "no-store",
    credentials: "same-origin"
  });
  return parseResponse(response);
}

async function apiPost(action, values = {}, keepalive = false) {
  const response = await fetch(API, {
    method: "POST",
    credentials: "same-origin",
    cache: "no-store",
    keepalive,
    headers: { "Content-Type": "application/x-www-form-urlencoded;charset=UTF-8" },
    body: formBody({ action, ...values })
  });
  return parseResponse(response);
}

async function parseResponse(response) {
  let data = {};
  try { data = await response.json(); } catch (_) { /* respuesta no JSON */ }

  if (response.status === 401) {
    showLogin();
    throw new Error(data.error || "Sesión no válida");
  }
  if (!response.ok || data.ok === false) {
    throw new Error(data.error || `Error HTTP ${response.status}`);
  }
  return data;
}

function setConnection(online) {
  ui.connectionDot.classList.toggle("online", online);
  ui.connectionDot.classList.toggle("offline", !online);
  ui.connectionText.textContent = online ? "Conectado al robot" : "Sin conexión";
}

function showError(message) {
  ui.globalError.textContent = message;
  ui.globalError.hidden = false;
}

function clearError() {
  ui.globalError.hidden = true;
}

function showLogin() {
  loggedIn = false;
  stopPolling();
  ui.dashboardView.hidden = true;
  ui.loginView.hidden = false;
}

async function showDashboard() {
  loggedIn = true;
  ui.loginView.hidden = true;
  ui.dashboardView.hidden = false;
  await Promise.allSettled([refreshState(), refreshMap(), loadSongs()]);
  startPolling();
}

function startPolling() {
  stopPolling();
  stateTimer = setInterval(refreshState, POLL_STATE_MS);
  mapTimer = setInterval(refreshMap, POLL_MAP_MS);
}

function stopPolling() {
  if (stateTimer) clearInterval(stateTimer);
  if (mapTimer) clearInterval(mapTimer);
  stateTimer = null;
  mapTimer = null;
}

function formatDistance(value) {
  return Number.isFinite(Number(value)) && Number(value) >= 0 ? Number(value).toFixed(1) : "--";
}

function setLed(element, on, alert = false) {
  element.classList.toggle("on", Boolean(on));
  element.classList.toggle("alert", alert);
}

function renderState(state) {
  currentMode = state.mode || "unknown";
  const manual = currentMode === "manual";

  ui.modeBadge.textContent = manual ? "MANUAL" : currentMode === "autonomous" ? "AUTÓNOMO" : "--";
  ui.manualModeButton.classList.toggle("active", manual);
  ui.autonomousModeButton.classList.toggle("active", currentMode === "autonomous");

  ui.manualControlCard.style.opacity = manual ? "1" : ".55";
  document.querySelectorAll(".move-button").forEach((button) => {
    button.disabled = !manual;
  });

  const sensors = state.sensors || {};
  ui.sensorFront.textContent = formatDistance(sensors.front);
  ui.sensorLeft.textContent = formatDistance(sensors.left);
  ui.sensorRight.textContent = formatDistance(sensors.right);

  const leds = state.leds || {};
  setLed(ui.ledPower, leds.power);
  setLed(ui.ledAutonomous, leds.autonomous);
  setLed(ui.ledManual, leds.manual);
  setLed(ui.ledObstacle, leds.obstacle, true);

  const audio = state.audio || {};
  if (Number.isFinite(Number(audio.volume))) {
    ui.volumeSlider.value = audio.volume;
    ui.volumeValue.textContent = audio.volume;
  }
  if (audio.file) {
    ui.audioStatus.textContent = `${audio.state || "playing"}: ${audio.file}`;
  } else {
    ui.audioStatus.textContent = audio.state === "paused" ? "Pausado" : "Sin reproducción";
  }
}

async function refreshState() {
  if (!loggedIn) return;
  try {
    const data = await apiGet("state");
    setConnection(true);
    clearError();
    renderState(data);
  } catch (error) {
    setConnection(false);
    if (loggedIn) showError(error.message);
  }
}

async function refreshMap() {
  if (!loggedIn) return;
  try {
    const data = await apiGet("map");
    drawMap(data);
  } catch (error) {
    if (loggedIn) showError(error.message);
  }
}

async function loadSongs() {
  try {
    const data = await apiGet("audio_list");
    const songs = Array.isArray(data.songs) ? data.songs : [];
    ui.songSelect.innerHTML = "";
    if (!songs.length) {
      const option = document.createElement("option");
      option.textContent = "No hay MP3 disponibles";
      option.value = "";
      ui.songSelect.append(option);
      return;
    }
    for (const song of songs) {
      const option = document.createElement("option");
      option.value = song;
      option.textContent = song;
      ui.songSelect.append(option);
    }
  } catch (error) {
    showError(error.message);
  }
}

function drawMap(map) {
  const canvas = ui.mapCanvas;
  const ctx = canvas.getContext("2d");
  const width = Number(map.width || 0);
  const height = Number(map.height || 0);
  const cells = Array.isArray(map.cells) ? map.cells : [];

  ctx.clearRect(0, 0, canvas.width, canvas.height);
  ctx.fillStyle = "#020617";
  ctx.fillRect(0, 0, canvas.width, canvas.height);
  if (width <= 0 || height <= 0) return;

  const cell = Math.min(canvas.width / width, canvas.height / height);
  const ox = (canvas.width - width * cell) / 2;
  const oy = (canvas.height - height * cell) / 2;
  const colors = {
    unknown: "#1e293b",
    visited: "#64748b",
    obstacle: "#ef4444"
  };

  for (let y = 0; y < height; y += 1) {
    for (let x = 0; x < width; x += 1) {
      const value = cells[y * width + x] || "unknown";
      ctx.fillStyle = colors[value] || colors.unknown;
      ctx.fillRect(ox + x * cell, oy + y * cell, Math.max(1, cell - 1), Math.max(1, cell - 1));
    }
  }

  if (map.robot && Number.isFinite(Number(map.robot.x)) && Number.isFinite(Number(map.robot.y))) {
    const cx = ox + (Number(map.robot.x) + .5) * cell;
    const cy = oy + (Number(map.robot.y) + .5) * cell;
    ctx.beginPath();
    ctx.arc(cx, cy, Math.max(3, cell * .34), 0, Math.PI * 2);
    ctx.fillStyle = "#22c55e";
    ctx.fill();
  }
}

async function setMode(mode) {
  try {
    if (heldDirection) await stopMovement();
    await apiPost("set_mode", { mode });
    await refreshState();
  } catch (error) {
    showError(error.message);
  }
}

async function startMovement(direction, button) {
  if (currentMode !== "manual" || heldDirection) return;
  heldDirection = direction;
  button?.classList.add("pressed");
  try {
    await apiPost("move", { direction, speed: ui.speedSlider.value });
  } catch (error) {
    heldDirection = null;
    button?.classList.remove("pressed");
    showError(error.message);
  }
}

async function stopMovement(keepalive = false) {
  document.querySelectorAll(".move-button.pressed").forEach((button) => button.classList.remove("pressed"));
  if (!heldDirection && !keepalive) {
    try { await apiPost("stop"); } catch (_) { /* mantener parada como mejor esfuerzo */ }
    return;
  }
  heldDirection = null;
  try {
    await apiPost("stop", {}, keepalive);
  } catch (error) {
    if (!keepalive) showError(error.message);
  }
}

function emergencyStopBeacon() {
  if (!loggedIn) return;
  const body = new Blob([formBody({ action: "stop" })], { type: "application/x-www-form-urlencoded;charset=UTF-8" });
  navigator.sendBeacon(API, body);
  heldDirection = null;
}

ui.loginForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  ui.loginError.hidden = true;
  const data = new FormData(ui.loginForm);
  try {
    await apiPost("login", {
      username: String(data.get("username") || ""),
      password: String(data.get("password") || "")
    });
    ui.loginForm.reset();
    await showDashboard();
  } catch (error) {
    ui.loginError.textContent = error.message;
    ui.loginError.hidden = false;
  }
});

ui.logoutButton.addEventListener("click", async () => {
  emergencyStopBeacon();
  try { await apiPost("logout"); } catch (_) { /* logout local de todas formas */ }
  showLogin();
});

ui.autonomousModeButton.addEventListener("click", () => setMode("autonomous"));
ui.manualModeButton.addEventListener("click", () => setMode("manual"));

ui.speedSlider.addEventListener("input", () => {
  ui.speedValue.textContent = ui.speedSlider.value;
});

for (const button of document.querySelectorAll(".move-button[data-direction]")) {
  button.addEventListener("pointerdown", (event) => {
    event.preventDefault();
    button.setPointerCapture?.(event.pointerId);
    startMovement(button.dataset.direction, button);
  });
  button.addEventListener("pointerup", (event) => {
    event.preventDefault();
    stopMovement();
  });
  button.addEventListener("pointercancel", () => stopMovement());
}

ui.stopButton.addEventListener("click", () => stopMovement());
window.addEventListener("pointerup", () => { if (heldDirection) stopMovement(); });
window.addEventListener("pagehide", emergencyStopBeacon);
document.addEventListener("visibilitychange", () => {
  if (document.hidden && heldDirection) emergencyStopBeacon();
});

ui.playButton.addEventListener("click", async () => {
  if (!ui.songSelect.value) return;
  try { await apiPost("audio_play", { file: ui.songSelect.value }); await refreshState(); }
  catch (error) { showError(error.message); }
});

ui.pauseButton.addEventListener("click", async () => {
  try { await apiPost("audio_pause"); await refreshState(); }
  catch (error) { showError(error.message); }
});

ui.audioStopButton.addEventListener("click", async () => {
  try { await apiPost("audio_stop"); await refreshState(); }
  catch (error) { showError(error.message); }
});

ui.volumeSlider.addEventListener("input", () => { ui.volumeValue.textContent = ui.volumeSlider.value; });
ui.volumeSlider.addEventListener("change", async () => {
  try { await apiPost("audio_volume", { volume: ui.volumeSlider.value }); }
  catch (error) { showError(error.message); }
});

(async function boot() {
  try {
    await apiGet("state");
    await showDashboard();
  } catch (_) {
    showLogin();
  }
})();
