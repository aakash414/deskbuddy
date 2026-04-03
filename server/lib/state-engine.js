const { sensorStore } = require("./stores/sensors");
const { locationStore } = require("./stores/location");
const { overrideStore } = require("./stores/override");
const { calendarPoller } = require("./integrations/calendar");
const { jiraPoller } = require("./integrations/jira");

const STATES = {
  thirsty: "thirsty",
  overwatered: "overwatered",
  loved: "loved",
  sleeping: "sleeping",
  meeting: "meeting",
  rooftop: "rooftop",
  focus: "focus",
  coding: "coding",
  idle: "idle",
  happy: "happy",
};

const TEXT = {
  thirsty: { alt: ["At desk", "Thirsty!"] },
  overwatered: { alt: ["At desk", "Too wet!"] },
  loved: { alt: [null, "Loved"] },
  sleeping: { alt: ["Away"] },
  meeting: { alt: [null, "In meeting"] },
  rooftop: { alt: ["Rooftop", "Chillin"] },
  focus: { alt: [null, "Do not disturb"] },
  coding: { alt: [null, "Focused"] },
  idle: { alt: [null, "Available"] },
  happy: { alt: [null, "Happy"] },
};

function resolveState() {
  const sensors = sensorStore.get();
  const location = locationStore.get();
  const calendar = calendarPoller.get();
  const jira = jiraPoller.get();
  const override = overrideStore.get();

  let state = STATES.happy;

  // Priority 9: fallback
  state = STATES.happy;

  // Priority 8: Jira activity
  if (jira.hasInProgressTask) {
    state = STATES.coding;
  } else {
    state = STATES.idle;
  }

  // Priority 7: location
  if (location.location === "roof") {
    state = STATES.rooftop;
  } else if (
    location.location === "cubicle" ||
    location.location === "office_floor_cubicle"
  ) {
    state = STATES.focus;
  }

  // Priority 6: calendar
  if (calendar.inMeeting) {
    state = STATES.meeting;
  }

  // Priority 5: user away
  if (locationStore.isAway()) {
    state = STATES.sleeping;
  }

  // Priority 4: darkness
  if (sensors.light !== null && sensors.lightStatus === "dark") {
    state = STATES.sleeping;
  }

  // Priority 3: touch
  if (sensors.touched) {
    state = STATES.loved;
  }

  // Priority 2: manual override
  if (override) {
    state = override;
  }

  // Priority 1: plant emergency
  if (sensors.moistureStatus === "dry") {
    state = STATES.thirsty;
  } else if (sensors.moistureStatus === "soggy") {
    state = STATES.overwatered;
  }

  const textConfig = TEXT[state] || TEXT.happy;
  const locationLabel = location.label || "Unknown";
  const altTexts = buildAltTexts(state, textConfig, locationLabel, calendar, jira);

  return {
    state,
    location: location.location,
    locationLabel,
    alternatingText: altTexts,
    calendar: {
      inMeeting: calendar.inMeeting,
      currentMeeting: calendar.currentMeeting,
      nextMeeting: calendar.nextMeeting,
      nextMeetingIn: calendar.nextMeetingIn,
    },
    plant: {
      moisture: sensors.moisture,
      moistureStatus: sensors.moistureStatus,
      light: sensors.light,
      lightStatus: sensors.lightStatus,
    },
    updatedAt: new Date().toISOString(),
  };
}

function buildAltTexts(state, textConfig, locationLabel, calendar, jira) {
  const texts = [];

  for (const t of textConfig.alt) {
    if (t === null) {
      // null means "use dynamic value"
      if (state === STATES.meeting && calendar.currentMeeting) {
        texts.push(calendar.currentMeeting);
      } else if (state === STATES.focus) {
        texts.push(locationLabel);
      } else {
        texts.push(locationLabel);
      }
    } else {
      texts.push(t);
    }
  }

  return texts;
}

const stateEngine = {
  resolve: resolveState,
};

module.exports = { stateEngine, STATES };
