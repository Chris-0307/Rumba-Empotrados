"use strict";


//const API = "/cgi-bin/robot_api.cgi";
const API = "http_//172.20.10.12:8080";
const POLL_STATE_MS = 600;
const POLL_MAP_MS = 1200;

const $ = (id) => document.getElementById(id);

const ui = {
  loginView: $("loginView"),
  dashboardView: $("dashboardView"),
  loginForm: $("loginForm"),
  loginError: $("loginError"),
  raspberryIp: $("raspberryIp"),
  raspberryPort: $("raspberryPort"),
  testConnectionButton: $("testConnectionButton"),
  testConnectionResult: $("testConnectionResult"),
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
  ledPower: $("ledPower"),
  ledAutonomous: $("ledAutonomous"),
  ledManual: $("ledManual"),
  ledObstacle: $("ledObstacle"),
  songSelect: $("songSelect"),
  playButton: $("playButton"),
  pauseButton: $("pauseButton"),
  audioNextButton: $("audioNextButton"),
  volumeSlider: $("volumeSlider"),
  volumeValue: $("volumeValue"),
  audioStatus: $("audioStatus"),
  mapCanvas: $("mapCanvas")
};

let apiBase = "";
let authToken = "";
let loggedIn = false;
let currentMode = null;
let heldDirection = null;
let stateTimer = null;
let mapTimer = null;
let songs = [];

function getApiBase() {
  const ip = ui.raspberryIp.value.trim();
  const port = ui.raspberryPort.value.trim();

  if (!ip || !port) {
    throw new Error("Falta la IP o el puerto de la Raspberry");
  }

  return `http://${ip}:${port}`;
}

async function apiRequest(path, method = "GET", body = null, useAuth = true, keepalive = false) {
  const headers = {};

  if (body !== null) {
    headers["Content-Type"] = "application/json";
  }

  if (useAuth && authToken) {
    headers.Authorization = `Bearer ${authToken}`;
  }

  const response = await fetch(`${apiBase}${path}`, {
    method,
    headers,
    cache: "no-store",
    keepalive,
    body: body !== null ? JSON.stringify(body) : undefined
  });

  let data = {};
  try {
    data = await response.json();
  } catch (_) {
    // La API normalmente responde JSON.
  }

  if (response.status === 401 && useAuth) {
    authToken = "";
    showLogin();
    throw new Error("Sesión no válida");
  }

  if (!response.ok) {
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

  await Promise.allSettled([
    refreshState(),
    refreshMap(),
    loadSongs()
  ]);

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
  if (value === null || value === undefined || value === "") return "--";
  if (!Number.isFinite(Number(value)) || Number(value) < 0) {
    return "--";
  }
  return Number(value).toFixed(1);
}

function setLed(element, on, alert = false) {
  element.classList.toggle("on", Boolean(on));
  element.classList.toggle("alert", Boolean(on) && alert);
}

function songTitle(songId) {
  const song = songs.find((item) => Number(item.id) === Number(songId));
  return song ? song.title : `Canción ${songId}`;
}

function renderMode(modeData) {
  currentMode = modeData.mode || "unknown";
  const manual = currentMode === "manual";
  const automatic = currentMode === "automatic";

  ui.modeBadge.textContent = manual ? "MANUAL" : automatic ? "AUTÓNOMO" : "--";
  ui.manualModeButton.classList.toggle("active", manual);
  ui.autonomousModeButton.classList.toggle("active", automatic);

  ui.manualControlCard.style.opacity = manual ? "1" : ".55";
  document.querySelectorAll(".move-button").forEach((button) => {
    button.disabled = button.id === "stopButton" ? false : !manual;
  });

}

function renderSensors(sensors) {
  ui.sensorFront.textContent = formatDistance(sensors.front_cm);
}

function renderIndicators(indicators) {
  setLed(ui.ledPower, indicators.system === "functional");
  setLed(ui.ledAutonomous, indicators.mode === "automatic");
  setLed(ui.ledManual, indicators.mode === "manual");
  setLed(ui.ledObstacle, indicators.obstacle === true, true);

}

function renderAudio(audio) {
  if (Number.isFinite(Number(audio.volume))) {
    ui.volumeSlider.value = audio.volume;
    ui.volumeValue.textContent = audio.volume;
  }

  if (Number(audio.song_id) > 0) {
    ui.songSelect.value = String(audio.song_id);
    const title = songTitle(audio.song_id);

    if (audio.state === "playing") {
      ui.audioStatus.textContent = `Reproduciendo: ${title}`;
    } else if (audio.state === "paused") {
      ui.audioStatus.textContent = `Pausado: ${title}`;
    } else {
      ui.audioStatus.textContent = title;
    }
  } else {
    ui.audioStatus.textContent = "Sin reproducción";
  }
}

// Each section reports its own availability; only mode controls movement.
function sectionStatus(id, anchor, message) {
  let element = document.getElementById(id);
  if (!element) {
    element = document.createElement("p");
    element.id = id;
    element.className = "muted";
    element.setAttribute("role", "status");
    anchor.closest(".card").append(element);
  }
  element.textContent = message;
  element.hidden = !message;
}

let refreshingState = false;
let refreshingMap = false;

async function refreshState() {
  if (!loggedIn || refreshingState) return;
  refreshingState = true;
  try {
    await Promise.allSettled([
      apiRequest("/api/mode").then((data) => {
        if (!loggedIn) return;
        renderMode(data);
        setConnection(true);
        sectionStatus("modeStatus", ui.modeBadge, "");
      }).catch((error) => {
        if (!loggedIn) return;
        renderMode({ mode: "unknown" });
        setConnection(false);
        sectionStatus("modeStatus", ui.modeBadge, `Modo no disponible: ${error.message}`);
      }),
      apiRequest("/api/sensors").then((data) => {
        if (!loggedIn) return;
        renderSensors(data);
        sectionStatus("sensorStatus", ui.sensorFront, "");
      }).catch(() => {
        if (!loggedIn) return;
        renderSensors({});
        sectionStatus("sensorStatus", ui.sensorFront, "Sensor no disponible");
      }),
      apiRequest("/api/indicators").then((data) => {
        if (!loggedIn) return;
        renderIndicators(data);
        sectionStatus("indicatorStatus", ui.ledPower, "");
      }).catch(() => {
        if (!loggedIn) return;
        renderIndicators({});
        sectionStatus("indicatorStatus", ui.ledPower, "Indicadores no disponibles");
      }),
      apiRequest("/api/audio/state").then((data) => {
        if (!loggedIn) return;
        renderAudio(data);
        sectionStatus("audioAvailability", ui.audioStatus, "");
      }).catch(() => {
        if (!loggedIn) return;
        ui.audioStatus.textContent = "Estado de audio no disponible";
        sectionStatus("audioAvailability", ui.audioStatus, "Audio no disponible");
      })
    ]);
  } finally {
    refreshingState = false;
  }
}

async function refreshMap() {
  if (!loggedIn || refreshingMap) return;
  refreshingMap = true;
  try {
    const data = await apiRequest("/api/map");
    if (!loggedIn) return;
    drawMap(data);
    sectionStatus("mapStatus", ui.mapCanvas, "");
  } catch (_) {
    if (!loggedIn) return;
    drawMap({});
    sectionStatus("mapStatus", ui.mapCanvas, "Mapa no disponible");
  } finally {
    refreshingMap = false;
  }
}

async function loadSongs() {
  try {
    const data = await apiRequest("/api/audio/songs");
    songs = Array.isArray(data) ? data : [];
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
      option.value = song.id;
      option.textContent = song.title;
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
      let value = "unknown";

      if (Array.isArray(cells[y])) {
        value = cells[y][x] || "unknown";
      } else {
        value = cells[y * width + x] || "unknown";
      }

      ctx.fillStyle = colors[value] || colors.unknown;
      ctx.fillRect(
        ox + x * cell,
        oy + y * cell,
        Math.max(1, cell - 1),
        Math.max(1, cell - 1)
      );
    }
  }

  if (map.robot && Number.isFinite(Number(map.robot.x)) && Number.isFinite(Number(map.robot.y))) {
    const cx = ox + (Number(map.robot.x) + 0.5) * cell;
    const cy = oy + (Number(map.robot.y) + 0.5) * cell;

    ctx.beginPath();
    ctx.arc(cx, cy, Math.max(3, cell * 0.34), 0, Math.PI * 2);
    ctx.fillStyle = "#22c55e";
    ctx.fill();
  }
}

async function setMode(mode) {
  try {
    if (heldDirection) {
      await stopMovement();
    }

    const data = await apiRequest("/api/mode", "PUT", { mode });
    renderMode(data);
    clearError();
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
    await apiRequest("/api/move", "POST", {
      direction,
      speed: Number(ui.speedSlider.value)
    });
  } catch (error) {
    heldDirection = null;
    button?.classList.remove("pressed");
    showError(error.message);
  }
}

async function stopMovement(keepalive = false) {
  document.querySelectorAll(".move-button.pressed").forEach((button) => {
    button.classList.remove("pressed");
  });

  heldDirection = null;

  try {
    await apiRequest("/api/move", "POST", {
      direction: "stop",
      speed: 0
    }, true, keepalive);
  } catch (error) {
    if (!keepalive) showError(error.message);
  }
}

function emergencyStop() {
  if (!loggedIn || !authToken || !apiBase) return;

  fetch(`${apiBase}/api/move`, {
    method: "POST",
    keepalive: true,
    headers: {
      "Content-Type": "application/json",
      Authorization: `Bearer ${authToken}`
    },
    body: JSON.stringify({ direction: "stop", speed: 0 })
  }).catch(() => {});

  heldDirection = null;
}

ui.testConnectionButton.addEventListener("click", async () => {
  ui.testConnectionResult.textContent = "Probando conexión...";

  try {
    const testBase = getApiBase();
    const response = await fetch(`${testBase}/api/status`, {
      cache: "no-store"
    });

    const data = await response.json();

    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }

    if (data.status !== "ok") {
      throw new Error("Respuesta inesperada");
    }

    apiBase = testBase;
    ui.testConnectionResult.textContent = "Conexión OK";
  } catch (error) {
    ui.testConnectionResult.textContent = `Error: ${error.message}`;
  }
});

ui.loginForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  ui.loginError.hidden = true;

  const form = new FormData(ui.loginForm);

  try {
    apiBase = getApiBase();

    const data = await apiRequest("/api/auth/login", "POST", {
      username: String(form.get("username") || ""),
      password: String(form.get("password") || "")
    }, false);

    authToken = data.token || "";

    if (!authToken) {
      throw new Error("La API no devolvió un token");
    }

    $("password").value = "";
    await showDashboard();
  } catch (error) {
    ui.loginError.textContent = error.message;
    ui.loginError.hidden = false;
  }
});

ui.logoutButton.addEventListener("click", async () => {
  try {
    await apiRequest("/api/move", "POST", { direction: "stop", speed: 0 });
  } catch (_) {
    // La parada se intenta antes de cerrar la sesión.
  }

  try {
    await apiRequest("/api/auth/logout", "POST", {});
  } catch (_) {
    // Si falla el logout remoto, se cierra localmente de todas formas.
  }

  authToken = "";
  showLogin();
});

ui.autonomousModeButton.addEventListener("click", () => setMode("automatic"));
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
window.addEventListener("pointerup", () => {
  if (heldDirection) stopMovement();
});
window.addEventListener("pagehide", emergencyStop);
document.addEventListener("visibilitychange", () => {
  if (document.hidden && heldDirection) emergencyStop();
});

ui.playButton.addEventListener("click", async () => {
  if (!ui.songSelect.value) return;

  try {
    await apiRequest("/api/audio/play", "POST", {
      song_id: Number(ui.songSelect.value)
    });
    await refreshState();
  } catch (error) {
    showError(error.message);
  }
});

ui.pauseButton.addEventListener("click", async () => {
  try {
    await apiRequest("/api/audio/pause", "POST", {});
    await refreshState();
  } catch (error) {
    showError(error.message);
  }
});

ui.audioNextButton.addEventListener("click", async () => {
  try {
    await apiRequest("/api/audio/next", "POST", {});
    await refreshState();
  } catch (error) {
    showError(error.message);
  }
});

ui.volumeSlider.addEventListener("input", () => {
  ui.volumeValue.textContent = ui.volumeSlider.value;
});

ui.volumeSlider.addEventListener("change", async () => {
  try {
    await apiRequest("/api/audio/volume", "PUT", {
      volume: Number(ui.volumeSlider.value)
    });
  } catch (error) {
    showError(error.message);
  }
});

showLogin();
