// U'rWay Browser Extension Service Worker (Manifest V3)
// Privacy Rule: Never log URL paths, query parameters, page titles, or text. Top-level domains only.

let activeDomain = null;
let startTime = null;

const PRODUCTIVE_DOMAINS = [
  "github.com", "stackoverflow.com", "arxiv.org", "leetcode.com", 
  "coursera.org", "udemy.com", "medium.com", "docs.python.org"
];

function sanitizeToDomain(url) {
  try {
    const parsed = new URL(url);
    return parsed.hostname.toLowerCase();
  } catch (e) {
    return null;
  }
}

async function sendTelemetry(domain, durationSeconds) {
  if (!domain || durationSeconds < 5) return; // Ignore micro-glances
  
  const isProductive = PRODUCTIVE_DOMAINS.some(d => domain.includes(d));
  
  try {
    await fetch("http://localhost:8000/api/v1/ingest/browser", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        domain: domain,
        session_duration_seconds: durationSeconds,
        is_productive: isProductive
      })
    });
    console.log(`[U'rWay Ext] Privacy Telemetry Sent: ${domain} (${durationSeconds}s, Prod: ${isProductive})`);
  } catch (err) {
    console.warn("[U'rWay Ext] Server offline or unreachable:", err);
  }
}

chrome.tabs.onActivated.addListener(async (activeInfo) => {
  const now = Date.now();
  if (activeDomain && startTime) {
    const duration = Math.round((now - startTime) / 1000);
    sendTelemetry(activeDomain, duration);
  }

  try {
    const tab = await chrome.tabs.get(activeInfo.tabId);
    activeDomain = sanitizeToDomain(tab.url);
    startTime = now;
  } catch (e) {
    activeDomain = null;
  }
});
