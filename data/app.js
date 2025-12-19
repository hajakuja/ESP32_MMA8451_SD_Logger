const el = (id) => document.getElementById(id);

async function api(path) {
  const r = await fetch(path, { cache: "no-store" });
  if (!r.ok) throw new Error(await r.text());
  return await r.json();
}

function fmtBytes(n) {
  if (n < 1024) return `${n} B`;
  if (n < 1024*1024) return `${(n/1024).toFixed(1)} KB`;
  if (n < 1024*1024*1024) return `${(n/1024/1024).toFixed(2)} MB`;
  return `${(n/1024/1024/1024).toFixed(2)} GB`;
}

async function refreshStatus() {
  try {
    const s = await api("/api/status");
    el("rec").textContent = s.recording ? "YES" : "NO";
    el("file").textContent = s.file || "-";
    el("samples").textContent = s.samples ?? "-";
    el("intv").textContent = s.interval_ms ?? "-";
    el("mode").textContent = s.mode ?? "-";
    el("ip").textContent = s.ip ? `(${s.ip})` : "";
    // Keep input interval in sync if user hasn't changed it
  } catch (e) {
    el("rec").textContent = "ERR";
  }
}

async function start() {
  const file = el("filename").value || "log.csv";
  const interval = parseInt(el("interval").value || "20", 10);
  const q = new URLSearchParams({ file, interval_ms: String(interval) });
  const res = await api(`/api/start?${q}`);
  if (!res.ok) alert(res.error || "Start failed");
  await refreshStatus();
  await listFiles();
}

async function stop() {
  await api("/api/stop");
  await refreshStatus();
}

async function listFiles() {
  const box = el("files");
  box.textContent = "Loading...";
  try {
    const files = await api("/api/list");
    if (!files.length) {
      box.textContent = "No files found.";
      return;
    }
    box.innerHTML = "";
    files.sort((a,b) => (a.name > b.name ? 1 : -1));
    for (const f of files) {
      const row = document.createElement("div");
      row.className = "fileRow";
      const left = document.createElement("div");
      left.innerHTML = `<div><strong>${f.name}</strong></div><div class="meta">${fmtBytes(f.size)}</div>`;
      const actions = document.createElement("div");
      actions.className = "actions";

      const dl = document.createElement("a");
      dl.href = `/api/download?file=${encodeURIComponent(f.name)}`;
      dl.textContent = "Download";

      const del = document.createElement("button");
      del.className = "danger";
      del.textContent = "Delete";
      del.onclick = async () => {
        if (!confirm(`Delete ${f.name}?`)) return;
        const r = await api(`/api/delete?file=${encodeURIComponent(f.name)}`);
        if (!r.ok) alert(r.error || "Delete failed");
        await listFiles();
      };

      actions.appendChild(dl);
      actions.appendChild(del);
      row.appendChild(left);
      row.appendChild(actions);
      box.appendChild(row);
    }
  } catch (e) {
    box.textContent = `Error: ${e.message}`;
  }
}

el("startBtn").addEventListener("click", () => start().catch(e => alert(e.message)));
el("stopBtn").addEventListener("click", () => stop().catch(e => alert(e.message)));
el("refreshBtn").addEventListener("click", () => refreshStatus());
el("listBtn").addEventListener("click", () => listFiles());

refreshStatus();
listFiles();
setInterval(refreshStatus, 1500);
