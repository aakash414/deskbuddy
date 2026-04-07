const express = require("express");
const cors = require("cors");
const { resolve } = require("path");

const { loadEnv } = require("./lib/env");
const { stateEngine, STATES } = require("./lib/state-engine");
const { sensorStore } = require("./lib/stores/sensors");
const { locationStore } = require("./lib/stores/location");
const { overrideStore } = require("./lib/stores/override");
const { calendarPoller } = require("./lib/integrations/calendar");
const { jiraPoller } = require("./lib/integrations/jira");

loadEnv();

const app = express();
app.use(cors());
app.use(express.json());

const PORT = process.env.PORT || 3777;
const API_KEY = process.env.API_KEY;

function requireApiKey(req, res, next) {
  if (!API_KEY) return next();
  if (req.headers["x-api-key"] !== API_KEY) {
    return res.status(401).json({ error: "unauthorized" });
  }
  next();
}

// GET /status — ESP32 polls this every 30s
app.get("/status", (req, res) => {
  const state = stateEngine.resolve();
  res.json(state);
});

const VALID_STATES = Object.keys(STATES);

// POST /sensors — ESP32 pushes every 10s
app.post("/sensors", requireApiKey, (req, res) => {
  const { moisture, light, touched } = req.body;
  if (moisture !== undefined && (typeof moisture !== "number" || moisture < 0 || moisture > 100)) {
    return res.status(400).json({ error: "moisture must be a number between 0 and 100" });
  }
  if (light !== undefined && (typeof light !== "number" || light < 0)) {
    return res.status(400).json({ error: "light must be a non-negative number" });
  }
  sensorStore.update({ moisture, light, touched: touched !== undefined ? !!touched : undefined });
  res.json({ ok: true });
});

// POST /location — MacBook daemon pushes every 30s
app.post("/location", requireApiKey, (req, res) => {
  const { location, bssid } = req.body;
  if (location !== undefined && typeof location !== "string") {
    return res.status(400).json({ error: "location must be a string" });
  }
  locationStore.update(location, bssid);
  res.json({ ok: true, label: locationStore.getLabel() });
});

// POST /override — manual state force from dashboard
app.post("/override", requireApiKey, (req, res) => {
  const { state, expiresIn } = req.body;
  if (!state || !VALID_STATES.includes(state)) {
    return res.status(400).json({ error: `state must be one of: ${VALID_STATES.join(", ")}` });
  }
  if (expiresIn !== undefined && (typeof expiresIn !== "number" || expiresIn <= 0)) {
    return res.status(400).json({ error: "expiresIn must be a positive number" });
  }
  const expiresAt = overrideStore.set(
    state,
    expiresIn || Number(process.env.OVERRIDE_DEFAULT_EXPIRY) || 1800
  );
  res.json({ ok: true, expiresAt });
});

// GET /dashboard — debug web UI
app.get("/dashboard", (req, res) => {
  res.sendFile(resolve(__dirname, "public/dashboard.html"));
});

// GET /health — server health check
app.get("/health", (req, res) => {
  res.json({
    status: "healthy",
    uptime: Math.floor(process.uptime()),
    lastSensorUpdate: sensorStore.lastUpdate(),
    lastLocationUpdate: locationStore.lastUpdate(),
    calendarConnected: calendarPoller.isConnected(),
    jiraConnected: jiraPoller.isConnected(),
  });
});

app.listen(PORT, () => {
  console.log(`DeskBuddy server running on port ${PORT}`);

  calendarPoller.start();
  jiraPoller.start();
});

function shutdown() {
  calendarPoller.stop();
  jiraPoller.stop();
  process.exit(0);
}
process.on("SIGTERM", shutdown);
process.on("SIGINT", shutdown);
