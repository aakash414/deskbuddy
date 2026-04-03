const LOCATION_LABELS = {
  desk: "At desk",
  cubicle: "Private cubicle",
  roof: "Rooftop",
  office_floor_cubicle: "Downstairs cubicle",
  unknown: "Unknown",
};

const state = {
  location: "unknown",
  label: "Unknown",
  bssid: null,
  updatedAt: null,
};

const locationStore = {
  update(location, bssid) {
    state.location = location || "unknown";
    state.label = LOCATION_LABELS[state.location] || "Unknown";
    state.bssid = bssid || null;
    state.updatedAt = new Date().toISOString();
  },

  get() {
    return {
      location: state.location,
      label: state.label,
      bssid: state.bssid,
    };
  },

  getLabel() {
    return state.label;
  },

  isAway() {
    if (!state.updatedAt) return true;
    const timeout =
      (Number(process.env.LOCATION_TIMEOUT) || 300) * 1000;
    return Date.now() - new Date(state.updatedAt).getTime() > timeout;
  },

  lastUpdate() {
    return state.updatedAt;
  },
};

module.exports = { locationStore };
