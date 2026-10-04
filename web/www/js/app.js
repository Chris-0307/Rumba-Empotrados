"use strict";


const POLL_STATE_MS = 600;
const POLL_MAP_MS = 1200;

const $ = (id) => document.getElementById(id);

const ui = {
  loginView: $("loginView"),
  dashboardView: $("dashboardView"),
  loginForm: $("loginForm"),
  loginTab: $("loginTab"), registerTab: $("registerTab"),
  authSubmit: $("authSubmit"), authDescription: $("authDescription"),
  confirmPasswordGroup: $("confirmPasswordGroup"),
  confirmPassword: $("confirmPassword"), sessionUser: $("sessionUser"),
  sensorFloor: $("sensorFloor"), ledFloor: $("ledFloor"),
  floorState: $("floorState"), floorHelp: $("floorHelp"),
  floorResetButton: $("floorResetButton"),
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
  audioStopButton: $("audioStopButton"),
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
let authMode = "login";
let floorBlocked = true;
let sessionName = "";
let authPending = false;

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
    const code = data.error || `Error HTTP ${response.status}`;
    const messages = { floor_safety_blocked: "Movimiento bloqueado por el sensor de suelo. Revisa su estado y confirma la recuperación.", invalid_credentials: "Usuario o contraseña incorrectos.", username_exists: "Ese usuario ya existe. Inicia sesión o utiliza otro nombre.", manual_mode_required: "Selecciona el modo manual para mover el robot." };
    throw new Error(messages[code] || code);
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
  heldDirection = null;
  floorBlocked = true;
  authToken = "";
  sessionName = "";
  ui.sessionUser.textContent = "";
  document.querySelectorAll(".move-button").forEach(button => button.classList.remove("pressed"));
  ui.dashboardView.hidden = true;
  ui.loginView.hidden = false;
}

async function showDashboard() {
  loggedIn = true;
  ui.loginView.hidden = true;
  ui.dashboardView.hidden = false;
  ui.sessionUser.textContent = sessionName;
  renderSensors({});
  clearError();

  await Promise.allSettled([
    refreshState(),
    refreshMap(),
    loadSongs()
  ]);

  if (loggedIn) startPolling();
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
  updateMovementButtons();

}

function updateMovementButtons() {
  document.querySelectorAll(".move-button").forEach(button => {
    button.disabled = button.id === "stopButton" ? false : currentMode !== "manual" || floorBlocked;
  });
}

function renderSensors(sensors) {
  ui.sensorFront.textContent = formatDistance(sensors.front_cm);
  ui.sensorFloor.textContent = formatDistance(sensors.floor_cm);
  const hasFloorData = Object.prototype.hasOwnProperty.call(sensors, "floor_cm") &&
    Object.prototype.hasOwnProperty.call(sensors, "floor_state");
  const hasFrontData = Object.prototype.hasOwnProperty.call(sensors, "front_cm");
  const state = hasFloorData ? (sensors.floor_state || "unknown") : hasFrontData ? "unsupported" : "unknown";
  const labels = { present: "Piso presente", cliff: "Posible borde", sensor_error: "Error / sin eco", stale: "Lectura vencida", warming_up: "Confirmando piso", unconfigured: "Falta calibración", unsupported: "La API no devuelve datos de suelo", unknown: "Sin lectura de suelo" };
  ui.floorState.textContent = labels[state] || "Estado no disponible";
  ui.floorState.dataset.state = state;
  floorBlocked = sensors.movement_blocked !== false;
  const warning = !["present", "cliff", "sensor_error"].includes(state) || (state === "present" && floorBlocked);
  const present = state === "present" && !floorBlocked;
  const danger = state === "cliff" || state === "sensor_error";
  setLed(ui.ledFloor, present || danger, danger);
  ui.ledFloor.classList.toggle("warning", warning);
  ui.ledFloor.setAttribute("aria-label", ui.floorState.textContent);
  ui.ledFloor.title = ui.floorState.textContent;
  const help = {
    present: sensors.floor_latched ? "Piso confirmado. Pulsa Parar / confirmar recuperación antes de volver a mover el robot." : "Piso confirmado por el robot.",
    cliff: "Posible desnivel. Recoloca el robot sobre piso firme antes de confirmar la recuperación.",
    sensor_error: "El servidor no recibe un eco válido del sensor de suelo. Revisa su conexión.",
    stale: "La lectura está vencida. Espera una lectura válida antes de mover el robot.",
    warming_up: "Confirmando tres lecturas consecutivas de piso.",
    unconfigured: "El sensor puede estar leyendo, pero falta configurar su umbral en el servidor.",
    unsupported: "La respuesta de /api/sensors contiene el frontal, pero no los campos del suelo. Comprueba que esté ejecutándose el servidor actualizado.",
    unknown: "Todavía no hay datos de suelo disponibles. Comprueba la conexión con el robot."
  };
  const limit = formatDistance(sensors.floor_max_cm);
  ui.floorHelp.textContent = (help[state] || "Estado de suelo no reconocido.") + (limit !== "--" ? ` Umbral: ${limit} cm.` : "");
  if (floorBlocked && heldDirection) {
    heldDirection = null;
    document.querySelectorAll(".move-button").forEach(button => button.classList.remove("pressed"));
  }
  updateMovementButtons();
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
      ui.audioStatus.textContent = `Finalizado / detenido: ${title}`;
    }
  } else {
    ui.audioStatus.textContent = "Sin reproducción";
  }
}

// Cada sección conserva su actualización independiente; modo y suelo habilitan movimiento.
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
  if (!loggedIn || currentMode !== "manual" || floorBlocked || heldDirection) return;

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
    if (!keepalive && loggedIn) { clearError(); await refreshState(); }
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

function selectAuthMode(mode) {
  if (authPending) return;
  authMode = mode;
  const registering = mode === "register";
  ui.loginTab.classList.toggle("active", !registering);
  ui.registerTab.classList.toggle("active", registering);
  ui.loginTab.setAttribute("aria-pressed", String(!registering));
  ui.registerTab.setAttribute("aria-pressed", String(registering));
  ui.authSubmit.textContent = registering ? "Crear cuenta e iniciar sesión" : "Iniciar sesión";
  ui.authDescription.textContent = registering ? "Crea tu cuenta para acceder al robot." : "Inicia sesión para controlar tu robot.";
  ui.confirmPasswordGroup.hidden = !registering;
  ui.confirmPassword.required = registering;
  ui.confirmPassword.value = "";
  $("password").autocomplete = registering ? "new-password" : "current-password";
  $("password").minLength = registering ? 12 : 1;
  $("username").minLength = registering ? 3 : 1;
  $("username").maxLength = 32;
  $("password").maxLength = 120;
  ui.loginError.hidden = true;
}
ui.loginTab.addEventListener("click", () => selectAuthMode("login"));
ui.registerTab.addEventListener("click", () => selectAuthMode("register"));

ui.loginForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  if (authPending) return;
  ui.loginError.hidden = true;
  const form = new FormData(ui.loginForm);
  const credentials = { username: String(form.get("username") || ""), password: String(form.get("password") || "") };
  if (authMode === "register" && credentials.password !== ui.confirmPassword.value) {
    ui.loginError.textContent = "Las contraseñas no coinciden."; ui.loginError.hidden = false; return;
  }
  if (authMode === "register" && !/^[A-Za-z0-9_]{3,32}$/.test(credentials.username)) {
    ui.loginError.textContent = "El usuario debe tener de 3 a 32 letras, números o guiones bajos."; ui.loginError.hidden = false; return;
  }
  authPending = true;
  ui.authSubmit.disabled = true; ui.loginTab.disabled = true; ui.registerTab.disabled = true;
  ui.authSubmit.textContent = "Conectando…";
  let created = false;
  try {
    apiBase = getApiBase();
    if (authMode === "register") { await apiRequest("/api/auth/register", "POST", credentials, false); created = true; }
    const data = await apiRequest("/api/auth/login", "POST", credentials, false);
    authToken = data.token || "";
    if (!authToken) throw new Error("La API no devolvió un token");
    sessionName = credentials.username;
    authMode = "login";
    $("password").value = ""; ui.confirmPassword.value = "";
    await showDashboard();
  } catch (error) {
    ui.loginError.textContent = created ? `Cuenta creada. Vuelve a iniciar sesión: ${error.message}` : error.message;
    ui.loginError.hidden = false;
    if (created) authMode = "login";
  } finally {
    authPending = false;
    ui.authSubmit.disabled = false; ui.loginTab.disabled = false; ui.registerTab.disabled = false;
    const message = ui.loginError.textContent, hasError = !ui.loginError.hidden;
    selectAuthMode(authMode);
    if (hasError) { ui.loginError.textContent = message; ui.loginError.hidden = false; }
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
ui.floorResetButton.addEventListener("click", () => stopMovement());
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

ui.audioStopButton.addEventListener("click", async () => {
  try { await apiRequest("/api/audio/stop", "POST", {}); await refreshState(); }
  catch (error) { showError(error.message); }
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

selectAuthMode("login");
showLogin();
