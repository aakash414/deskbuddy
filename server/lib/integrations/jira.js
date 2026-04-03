const https = require("https");

const state = {
  hasInProgressTask: false,
  currentTask: null,
  summary: null,
  status: null,
  connected: false,
  cachedAt: null,
};

let pollInterval = null;

function getAuthHeader() {
  const email = process.env.JIRA_EMAIL;
  const token = process.env.JIRA_TOKEN;
  if (!email || !token) return null;
  return "Basic " + Buffer.from(`${email}:${token}`).toString("base64");
}

async function fetchCurrentTask() {
  const domain = process.env.JIRA_DOMAIN;
  const auth = getAuthHeader();
  if (!domain || !auth) {
    state.connected = false;
    return;
  }

  const jql = encodeURIComponent(
    'assignee=currentUser() AND sprint in openSprints() AND status="In Progress"'
  );
  const fields = "summary,status,key";
  const path = `/rest/api/3/search?jql=${jql}&fields=${fields}&maxResults=1`;

  return new Promise((resolve) => {
    const req = https.request(
      {
        hostname: domain,
        path,
        method: "GET",
        headers: {
          Authorization: auth,
          Accept: "application/json",
        },
      },
      (res) => {
        let data = "";
        res.on("data", (chunk) => (data += chunk));
        res.on("end", () => {
          try {
            const json = JSON.parse(data);
            processResult(json);
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

function processResult(result) {
  if (!result.issues || result.issues.length === 0) {
    state.hasInProgressTask = false;
    state.currentTask = null;
    state.summary = null;
    state.status = null;
    return;
  }

  const issue = result.issues[0];
  state.hasInProgressTask = true;
  state.currentTask = issue.key;
  state.summary = issue.fields?.summary || null;
  state.status = issue.fields?.status?.name || null;
}

const jiraPoller = {
  start() {
    if (!process.env.JIRA_TOKEN) {
      console.log("Jira: no credentials, skipping");
      return;
    }
    console.log("Jira: polling every 60s");
    fetchCurrentTask();
    pollInterval = setInterval(fetchCurrentTask, 60000);
  },

  stop() {
    if (pollInterval) clearInterval(pollInterval);
  },

  get() {
    return {
      hasInProgressTask: state.hasInProgressTask,
      currentTask: state.currentTask,
      summary: state.summary,
      status: state.status,
    };
  },

  isConnected() {
    return state.connected;
  },
};

module.exports = { jiraPoller };
