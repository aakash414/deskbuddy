const state = {
  moisture: null,
  light: null,
  touched: false,
  touchedAt: null,
  updatedAt: null,
};

const sensorStore = {
  update({ moisture, light, touched }) {
    if (moisture !== undefined) state.moisture = moisture;
    if (light !== undefined) state.light = light;
    if (touched) state.touchedAt = new Date().toISOString();
    state.touched = !!touched;
    state.updatedAt = new Date().toISOString();
  },

  get() {
    const touchHold = Number(process.env.TOUCH_HOLD) || 10;
    const touchActive =
      state.touchedAt &&
      Date.now() - new Date(state.touchedAt).getTime() < touchHold * 1000;

    return {
      moisture: state.moisture,
      moistureStatus: getMoistureStatus(state.moisture),
      light: state.light,
      lightStatus: getLightStatus(state.light),
      touched: touchActive,
    };
  },

  lastUpdate() {
    return state.updatedAt;
  },
};

function getMoistureStatus(val) {
  if (val === null) return "unknown";
  const dry = Number(process.env.MOISTURE_DRY) || 20;
  const soggy = Number(process.env.MOISTURE_SOGGY) || 90;
  if (val < dry) return "dry";
  if (val > soggy) return "soggy";
  return "good";
}

function getLightStatus(val) {
  if (val === null) return "unknown";
  const dark = Number(process.env.LIGHT_DARK) || 50;
  if (val < dark) return "dark";
  return "bright";
}

module.exports = { sensorStore };
