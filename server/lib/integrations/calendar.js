const https = require("https");

const state = {
  inMeeting: false,
  currentMeeting: null,
  nextMeeting: null,
  nextMeetingIn: null,
  connected: false,
  cachedAt: null,
};

let accessToken = null;
let tokenExpiresAt = 0;
let pollInterval = null;

async function refreshAccessToken() {
  const clientId = process.env.GOOGLE_CLIENT_ID;
  const clientSecret = process.env.GOOGLE_CLIENT_SECRET;
  const refreshToken = process.env.GOOGLE_REFRESH_TOKEN;

  if (!clientId || !clientSecret || !refreshToken) return null;

  const body = new URLSearchParams({
    client_id: clientId,
    client_secret: clientSecret,
    refresh_token: refreshToken,
    grant_type: "refresh_token",
  }).toString();

  return new Promise((resolve) => {
    const req = https.request(
      {
        hostname: "oauth2.googleapis.com",
        path: "/token",
        method: "POST",
        headers: {
          "Content-Type": "application/x-www-form-urlencoded",
          "Content-Length": Buffer.byteLength(body),
        },
      },
      (res) => {
        let data = "";
        res.on("data", (chunk) => (data += chunk));
        res.on("end", () => {
          try {
            const json = JSON.parse(data);
            if (json.access_token) {
              accessToken = json.access_token;
              tokenExpiresAt = Date.now() + (json.expires_in - 60) * 1000;
              resolve(json.access_token);
            } else {
              resolve(null);
            }
          } catch {
            resolve(null);
          }
        });
      }
    );
    req.on("error", () => resolve(null));
    req.write(body);
    req.end();
  });
}

async function getToken() {
  if (accessToken && Date.now() < tokenExpiresAt) return accessToken;
  return refreshAccessToken();
}

async function fetchEvents() {
  const token = await getToken();
  if (!token) {
    state.connected = false;
    return;
  }

  const now = new Date();
  const twoHoursLater = new Date(now.getTime() + 2 * 60 * 60 * 1000);

  const params = new URLSearchParams({
    timeMin: now.toISOString(),
    timeMax: twoHoursLater.toISOString(),
    singleEvents: "true",
    orderBy: "startTime",
    maxResults: "5",
  });

  const path = `/calendar/v3/calendars/primary/events?${params}`;

  return new Promise((resolve) => {
    const req = https.request(
      {
        hostname: "www.googleapis.com",
        path,
        method: "GET",
        headers: { Authorization: `Bearer ${token}` },
      },
      (res) => {
        let data = "";
        res.on("data", (chunk) => (data += chunk));
        res.on("end", () => {
          try {
            const json = JSON.parse(data);
            processEvents(json.items || []);
            state.connected = true;
            state.cachedAt = new Date().toISOString();
          } catch {
            state.connected = false;
          }
          resolve();
        });
      }
    );
    req.on("error", () => {
      state.connected = false;
      resolve();
    });
    req.end();
  });
}

function processEvents(events) {
  const now = Date.now();

  const current = events.find((e) => {
    const start = new Date(e.start?.dateTime || e.start?.date).getTime();
    const end = new Date(e.end?.dateTime || e.end?.date).getTime();
    return start <= now && end > now;
  });

  const next = events.find((e) => {
    const start = new Date(e.start?.dateTime || e.start?.date).getTime();
    return start > now;
  });

  state.inMeeting = !!current;
  state.currentMeeting = current ? current.summary || "Meeting" : null;
  state.nextMeeting = next ? next.summary || "Upcoming" : null;
  state.nextMeetingIn = next
    ? Math.round(
        (new Date(next.start?.dateTime || next.start?.date).getTime() - now) /
          60000
      )
    : null;
}

const calendarPoller = {
  start() {
    if (!process.env.GOOGLE_REFRESH_TOKEN) {
      console.log("Calendar: no credentials, skipping");
      return;
    }
    console.log("Calendar: polling every 60s");
    fetchEvents();
    pollInterval = setInterval(fetchEvents, 60000);
  },

  stop() {
    if (pollInterval) clearInterval(pollInterval);
  },

  get() {
    return {
      inMeeting: state.inMeeting,
      currentMeeting: state.currentMeeting,
      nextMeeting: state.nextMeeting,
      nextMeetingIn: state.nextMeetingIn,
    };
  },

  isConnected() {
    return state.connected;
  },
};

module.exports = { calendarPoller };
