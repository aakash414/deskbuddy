const state = {
  active: false,
  stateName: null,
  expiresAt: null,
};

const overrideStore = {
  set(stateName, expiresInSeconds) {
    const expiresAt = new Date(
      Date.now() + expiresInSeconds * 1000
    ).toISOString();
    state.active = true;
    state.stateName = stateName;
    state.expiresAt = expiresAt;
    return expiresAt;
  },

  get() {
    if (!state.active) return null;
    if (new Date(state.expiresAt).getTime() < Date.now()) {
      state.active = false;
      state.stateName = null;
      state.expiresAt = null;
      return null;
    }
    return state.stateName;
  },

  clear() {
    state.active = false;
    state.stateName = null;
    state.expiresAt = null;
  },
};

module.exports = { overrideStore };
