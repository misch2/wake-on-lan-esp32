#pragma once

/**
 * Embedded Bootstrap 5 web interface for the Wake-on-LAN controller.
 *
 * Features:
 *   - Device cards show alias, MAC, IP, and live online/offline badge.
 *   - Status is polled from /api/devices every REFRESH_INTERVAL_S seconds.
 *   - Incremental DOM updates avoid full re-render flicker on refresh.
 *   - After a Wake request, refresh fires 3 s later to catch the device coming up.
 *   - Wake requests are sent as POST /api/wake?mac=<mac>.
 */
static const char HTML_CONTENT[] = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Wake on LAN Controller</title>
  <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css" rel="stylesheet" />
  <style>
    body { background-color: #f0f2f5; }
    .device-card { transition: transform 0.15s ease, box-shadow 0.15s ease; }
    .device-card:hover {
      transform: translateY(-3px);
      box-shadow: 0 0.5rem 1rem rgba(0,0,0,.15) !important;
    }
    .mono { font-size: 0.78rem; letter-spacing: 0.04em; font-family: monospace; }
    .status-dot {
      display: inline-block;
      width: 10px; height: 10px;
      border-radius: 50%;
      margin-right: 5px;
      flex-shrink: 0;
    }
    .status-dot.online  { background: #198754; box-shadow: 0 0 5px #19875488; }
    .status-dot.offline { background: #adb5bd; }
    #refresh-info { font-size: 0.8rem; }
    #toast-container { z-index: 1090; }
  </style>
</head>
<body>

<nav class="navbar navbar-dark bg-dark shadow-sm">
  <div class="container">
    <span class="navbar-brand fs-5 fw-semibold">&#128268; Wake on LAN Controller</span>
    <div class="d-flex align-items-center gap-2">
      <span id="refresh-info" class="text-secondary d-none">&#x21bb; <span id="refresh-countdown"></span>s</span>
      <span id="status-badge" class="badge bg-secondary">Loading&hellip;</span>
      <a href="/admin" class="btn btn-outline-light btn-sm">&#9881; Admin</a>
    </div>
  </div>
</nav>

<div class="container py-4">
  <div id="alert-area"></div>
  <div class="row gy-3" id="device-list">
    <div class="col-12 text-center text-muted py-5">
      <div class="spinner-border" role="status"></div>
      <p class="mt-2">Fetching device list&hellip;</p>
    </div>
  </div>
</div>

<div id="toast-container" class="position-fixed bottom-0 end-0 p-3"></div>

<script src="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js"></script>

<script>
  const REFRESH_INTERVAL_S = 10;
  let refreshTimer = null;
  let countdownTimer = null;
  let secondsLeft = REFRESH_INTERVAL_S;
  let initialLoad = true;

  // ── Data fetching ──────────────────────────────────────────────────────────

  async function loadDevices() {
    try {
      const response = await fetch('/api/devices');
      if (!response.ok) throw new Error('HTTP ' + response.status);
      const devices = await response.json();

      if (initialLoad) {
        renderDevices(devices);
        initialLoad = false;
      } else {
        updateStatuses(devices);
      }

      const onlineCount = devices.filter(d => d.online).length;
      document.getElementById('status-badge').className = 'badge bg-success';
      document.getElementById('status-badge').textContent =
        onlineCount + '/' + devices.length + ' online';
    } catch (err) {
      if (initialLoad) {
        document.getElementById('device-list').innerHTML =
          '<div class="col-12"><div class="alert alert-danger">Failed to load devices: ' +
          escHtml(err.message) + '</div></div>';
      }
      document.getElementById('status-badge').className = 'badge bg-danger';
      document.getElementById('status-badge').textContent = 'Error';
    }
    scheduleRefresh();
  }

  function scheduleRefresh(delaySecs) {
    const secs = delaySecs || REFRESH_INTERVAL_S;
    clearTimeout(refreshTimer);
    clearInterval(countdownTimer);
    secondsLeft = secs;
    document.getElementById('refresh-info').classList.remove('d-none');
    document.getElementById('refresh-countdown').textContent = secondsLeft;

    countdownTimer = setInterval(() => {
      secondsLeft--;
      document.getElementById('refresh-countdown').textContent = Math.max(0, secondsLeft);
      if (secondsLeft <= 0) clearInterval(countdownTimer);
    }, 1000);

    refreshTimer = setTimeout(loadDevices, secs * 1000);
  }

  // ── Full initial render ────────────────────────────────────────────────────

  function renderDevices(devices) {
    const list = document.getElementById('device-list');
    if (devices.length === 0) {
      list.innerHTML = '<div class="col-12"><div class="alert alert-warning">No devices configured.</div></div>';
      return;
    }
    list.innerHTML = devices.map((d, idx) => `
      <div class="col-12 col-sm-6 col-xl-4">
        <div class="card device-card shadow-sm h-100" id="card-${idx}">
          <div class="card-body d-flex justify-content-between align-items-center gap-3">
            <div class="overflow-hidden flex-grow-1">
              <div class="d-flex align-items-center mb-1">
                <span class="status-dot ${d.online ? 'online' : 'offline'}" id="dot-${idx}" title="${d.online ? 'Online' : 'Offline'}"></span>
                <h5 class="card-title mb-0 text-truncate">${escHtml(d.alias)}</h5>
              </div>
              <div class="d-flex flex-wrap gap-1 mt-1">
                <span class="badge bg-light text-secondary mono">${escHtml(d.mac)}</span>
                ${d.ip ? `<span class="badge bg-light text-secondary mono">${escHtml(d.ip)}</span>` : ''}
              </div>
              <div class="mt-1">
                <span class="badge ${d.online ? 'text-bg-success' : 'text-bg-secondary'}" id="badge-${idx}">
                  ${d.online ? 'Online' : 'Offline'}
                </span>
              </div>
            </div>
            <button
              class="btn btn-success flex-shrink-0"
              onclick="wakeDevice(this, '${escAttr(d.mac)}', '${escAttr(d.alias)}')"
              title="Send WOL magic packet"
            >
              &#9654;&nbsp;Wake
            </button>
          </div>
        </div>
      </div>
    `).join('');
  }

  // ── Incremental status-only update (avoids full re-render flicker) ─────────

  function updateStatuses(devices) {
    devices.forEach((d, idx) => {
      const dot   = document.getElementById('dot-'   + idx);
      const badge = document.getElementById('badge-' + idx);
      if (!dot || !badge) return;

      dot.className = 'status-dot ' + (d.online ? 'online' : 'offline');
      dot.title     = d.online ? 'Online' : 'Offline';
      badge.className   = 'badge ' + (d.online ? 'text-bg-success' : 'text-bg-secondary');
      badge.textContent = d.online ? 'Online' : 'Offline';
    });
  }

  // ── Wake action ────────────────────────────────────────────────────────────

  async function wakeDevice(btn, mac, alias) {
    btn.disabled = true;
    btn.innerHTML = '<span class="spinner-border spinner-border-sm" role="status"></span>';
    try {
      const response = await fetch('/api/wake?mac=' + encodeURIComponent(mac), { method: 'POST' });
      const data = await response.json();
      if (response.ok) {
        showToast('Magic packet sent to <strong>' + escHtml(alias) + '</strong>', 'success');
        // Refresh sooner – device should appear online soon after WOL
        scheduleRefresh(3);
      } else {
        showToast('Error: ' + escHtml(data.error || 'Unknown error'), 'danger');
      }
    } catch (err) {
      showToast('Request failed: ' + escHtml(err.message), 'danger');
    } finally {
      btn.disabled = false;
      btn.innerHTML = '&#9654;&nbsp;Wake';
    }
  }

  // ── Toast helper ───────────────────────────────────────────────────────────

  function showToast(message, type) {
    const id = 'toast-' + Date.now();
    const container = document.getElementById('toast-container');
    container.insertAdjacentHTML('beforeend', `
      <div id="${id}" class="toast align-items-center text-bg-${type} border-0" role="alert" aria-live="assertive">
        <div class="d-flex">
          <div class="toast-body">${message}</div>
          <button type="button" class="btn-close btn-close-white me-2 m-auto" data-bs-dismiss="toast" aria-label="Close"></button>
        </div>
      </div>
    `);
    const el = document.getElementById(id);
    new bootstrap.Toast(el, { delay: 4000 }).show();
    el.addEventListener('hidden.bs.toast', () => el.remove());
  }

  // ── XSS helpers ───────────────────────────────────────────────────────────

  const _escEl = document.createElement('span');
  function escHtml(s) { _escEl.textContent = String(s); return _escEl.innerHTML; }
  function escAttr(s) { return String(s).replace(/"/g, '&quot;').replace(/'/g, '&#39;'); }

  // ── Boot ───────────────────────────────────────────────────────────────────

  loadDevices();
</script>
</body>
</html>
)HTML";

// ─────────────────────────────────────────────────────────────────────────────
// Login page
// ─────────────────────────────────────────────────────────────────────────────
static const char HTML_LOGIN[] = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>WOL Admin \u2013 Login</title>
  <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css" rel="stylesheet" />
  <style>
    body { background-color: #f0f2f5; }
    .login-card { max-width: 380px; }
  </style>
</head>
<body class="d-flex align-items-center justify-content-center min-vh-100">
<div class="login-card w-100 mx-3">
  <div class="text-center mb-4">
    <h3 class="fw-bold">&#128268; Wake on LAN</h3>
    <p class="text-muted mb-0">Admin login</p>
  </div>
  <div class="card shadow">
    <div class="card-body p-4">
      <div id="err" class="alert alert-danger d-none" role="alert"></div>
      <form id="login-form" onsubmit="doLogin(event)" novalidate>
        <div class="mb-3">
          <label class="form-label fw-semibold" for="inp-password">Password</label>
          <input type="password" class="form-control" id="inp-password"
            placeholder="Enter password" required autofocus />
          <div class="invalid-feedback">Password is required.</div>
        </div>
        <button type="submit" class="btn btn-primary w-100" id="login-btn">Login</button>
      </form>
    </div>
  </div>
</div>
<script>
  async function doLogin(e) {
    e.preventDefault();
    const form = document.getElementById('login-form');
    if (!form.checkValidity()) { form.classList.add('was-validated'); return; }
    const btn = document.getElementById('login-btn');
    const err = document.getElementById('err');
    btn.disabled = true;
    btn.innerHTML = '<span class="spinner-border spinner-border-sm" role="status"></span>&nbsp;Logging in\u2026';
    err.classList.add('d-none');
    try {
      const r = await fetch('/api/login', {
        method:  'POST',
        headers: { 'Content-Type': 'application/json' },
        body:    JSON.stringify({ password: document.getElementById('inp-password').value }),
      });
      if (r.ok) {
        const params = new URLSearchParams(window.location.search);
        window.location.href = params.get('next') || '/admin';
      } else {
        const data = await r.json().catch(() => ({}));
        err.textContent = data.error || 'Login failed.';
        err.classList.remove('d-none');
      }
    } catch (ex) {
      err.textContent = 'Request failed: ' + ex.message;
      err.classList.remove('d-none');
    } finally {
      btn.disabled = false;
      btn.innerHTML = 'Login';
    }
  }
</script>
</body>
</html>
)HTML";

// ─────────────────────────────────────────────────────────────────────────────
// Admin page – add / remove devices stored in LittleFS
// ─────────────────────────────────────────────────────────────────────────────
static const char HTML_ADMIN[] = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>WOL Admin</title>
  <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css" rel="stylesheet" />
  <style>
    body { background-color: #f0f2f5; }
    .mono { font-size: 0.82rem; letter-spacing: 0.04em; font-family: monospace; }
    #toast-container { z-index: 1090; }
  </style>
</head>
<body>

<nav class="navbar navbar-dark bg-dark shadow-sm">
  <div class="container">
    <span class="navbar-brand fs-5 fw-semibold">&#9881; WOL Admin</span>
    <div class="d-flex gap-2">
      <a href="/" class="btn btn-outline-light btn-sm">&#8592; Devices</a>
      <button onclick="doLogout()" class="btn btn-outline-danger btn-sm">Logout</button>
    </div>
  </div>
</nav>

<div class="container py-4" style="max-width:860px">
  <div id="alert-area"></div>

  <!-- ── Device table ─────────────────────────────────────────────────── -->
  <div class="card shadow-sm mb-4">
    <div class="card-header d-flex justify-content-between align-items-center">
      <h5 class="mb-0">Configured Devices</h5>
      <button class="btn btn-sm btn-outline-secondary" onclick="loadDevices()">&#x21bb; Refresh</button>
    </div>
    <div class="card-body p-0">
      <div class="table-responsive">
        <table class="table table-hover align-middle mb-0">
          <thead class="table-light">
            <tr>
              <th>Alias</th>
              <th>MAC Address</th>
              <th>IP Address</th>
              <th class="text-end">Action</th>
            </tr>
          </thead>
          <tbody id="device-rows">
            <tr><td colspan="4" class="text-center py-4 text-muted">
              <div class="spinner-border spinner-border-sm me-2" role="status"></div>Loading&hellip;
            </td></tr>
          </tbody>
        </table>
      </div>
    </div>
  </div>

  <!-- ── Add device form ──────────────────────────────────────────────── -->
  <div class="card shadow-sm mb-4">
    <div class="card-header"><h5 class="mb-0">Add Device</h5></div>
    <div class="card-body">
      <form id="add-form" onsubmit="addDevice(event)" novalidate>
        <div class="row g-3">
          <div class="col-12 col-md-4">
            <label class="form-label fw-semibold" for="inp-alias">Name / Alias</label>
            <input type="text" class="form-control" id="inp-alias"
              placeholder="My Desktop" required maxlength="64" />
          </div>
          <div class="col-12 col-md-4">
            <label class="form-label fw-semibold" for="inp-mac">MAC Address</label>
            <input type="text" class="form-control mono" id="inp-mac"
              placeholder="AA:BB:CC:DD:EE:FF" required maxlength="17"
              pattern="[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}"
              title="Format: AA:BB:CC:DD:EE:FF" />
          </div>
          <div class="col-12 col-md-3">
            <label class="form-label fw-semibold" for="inp-ip">
              IP Address <span class="text-muted fw-normal">(optional, for ping)</span>
            </label>
            <input type="text" class="form-control mono" id="inp-ip"
              placeholder="192.168.1.10" maxlength="15" />
          </div>
          <div class="col-12 col-md-1 d-flex align-items-end">
            <button type="submit" class="btn btn-success w-100" id="add-btn">Add</button>
          </div>
        </div>
      </form>
    </div>
  </div>

  <!-- ── Change password ───────────────────────────────────────────────── -->
  <div class="card shadow-sm">
    <div class="card-header"><h5 class="mb-0">&#128274; Change Password</h5></div>
    <div class="card-body">
      <form id="pw-form" onsubmit="changePassword(event)" novalidate>
        <div class="row g-3">
          <div class="col-12 col-md-4">
            <label class="form-label fw-semibold" for="inp-pw-cur">Current Password</label>
            <input type="password" class="form-control" id="inp-pw-cur"
              placeholder="Current password" required />
          </div>
          <div class="col-12 col-md-4">
            <label class="form-label fw-semibold" for="inp-pw-new">New Password</label>
            <input type="password" class="form-control" id="inp-pw-new"
              placeholder="New password" required minlength="4" />
          </div>
          <div class="col-12 col-md-3">
            <label class="form-label fw-semibold" for="inp-pw-cfm">Confirm New</label>
            <input type="password" class="form-control" id="inp-pw-cfm"
              placeholder="Repeat new password" required />
          </div>
          <div class="col-12 col-md-1 d-flex align-items-end">
            <button type="submit" class="btn btn-warning w-100" id="pw-btn">Save</button>
          </div>
        </div>
      </form>
    </div>
  </div>
</div>

<div id="toast-container" class="position-fixed bottom-0 end-0 p-3"></div>

<script src="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js"></script>

<script>
  // ── Auth helpers ─────────────────────────────────────────────────────────
  function checkAuth(r) {
    if (r.status === 401) { window.location.href = '/login'; return false; }
    return true;
  }
  async function doLogout() {
    await fetch('/api/logout', { method: 'POST' }).catch(() => {});
    window.location.href = '/login';
  }

  // ── Fetch & render device table ──────────────────────────────────────────

  async function loadDevices() {
    try {
      const r = await fetch('/api/devices');
      if (!checkAuth(r)) return;
      if (!r.ok) throw new Error('HTTP ' + r.status);
      const devices = await r.json();
      const tbody = document.getElementById('device-rows');

      if (devices.length === 0) {
        tbody.innerHTML =
          '<tr><td colspan="4" class="text-center py-4 text-muted">No devices configured yet.</td></tr>';
        return;
      }

      tbody.innerHTML = devices.map(d => `
        <tr>
          <td>${escHtml(d.alias)}</td>
          <td class="mono">${escHtml(d.mac)}</td>
          <td class="mono">${d.ip ? escHtml(d.ip) : '<span class="text-muted">—</span>'}</td>
          <td class="text-end">
            <button class="btn btn-sm btn-outline-danger"
              onclick="deleteDevice('${escAttr(d.mac)}','${escAttr(d.alias)}')">
              &#x1F5D1;&nbsp;Remove
            </button>
          </td>
        </tr>
      `).join('');
    } catch (err) {
      showAlert('Failed to load devices: ' + escHtml(err.message), 'danger');
    }
  }

  // ── Delete a device ──────────────────────────────────────────────────────

  async function deleteDevice(mac, alias) {
    if (!confirm('Remove "' + alias + '" from the list?\nThis cannot be undone.')) return;
    try {
      const r = await fetch('/api/devices?mac=' + encodeURIComponent(mac), { method: 'DELETE' });
      if (!checkAuth(r)) return;
      const data = await r.json();
      if (r.ok) {
        showToast('Device <strong>' + escHtml(alias) + '</strong> removed.', 'success');
        loadDevices();
      } else {
        showToast('Error: ' + escHtml(data.error || 'Unknown error'), 'danger');
      }
    } catch (err) {
      showToast('Request failed: ' + escHtml(err.message), 'danger');
    }
  }

  // ── Add a device ─────────────────────────────────────────────────────────

  async function addDevice(e) {
    e.preventDefault();
    const form  = document.getElementById('add-form');
    if (!form.checkValidity()) { form.classList.add('was-validated'); return; }
    form.classList.remove('was-validated');

    const alias = document.getElementById('inp-alias').value.trim();
    const mac   = document.getElementById('inp-mac').value.trim().toUpperCase();
    const ip    = document.getElementById('inp-ip').value.trim();
    const btn   = document.getElementById('add-btn');

    btn.disabled = true;
    btn.innerHTML = '<span class="spinner-border spinner-border-sm" role="status"></span>';

    try {
      const r = await fetch('/api/devices', {
        method:  'POST',
        headers: { 'Content-Type': 'application/json' },
        body:    JSON.stringify({ alias, mac, ip }),
      });
      if (!checkAuth(r)) return;
      const data = await r.json();
      if (r.ok) {
        showToast('Device <strong>' + escHtml(alias) + '</strong> added!', 'success');
        form.reset();
        loadDevices();
      } else {
        showToast('Error: ' + escHtml(data.error || 'Unknown error'), 'danger');
      }
    } catch (err) {
      showToast('Request failed: ' + escHtml(err.message), 'danger');
    } finally {
      btn.disabled = false;
      btn.innerHTML = 'Add';
    }
  }

  // ── Change password ───────────────────────────────────────────────────────

  async function changePassword(e) {
    e.preventDefault();
    const form   = document.getElementById('pw-form');
    const newPw  = document.getElementById('inp-pw-new').value;
    const cfm    = document.getElementById('inp-pw-cfm');
    cfm.setCustomValidity(newPw !== cfm.value ? 'Passwords do not match.' : '');
    if (!form.checkValidity()) { form.classList.add('was-validated'); return; }
    form.classList.remove('was-validated');
    const btn = document.getElementById('pw-btn');
    btn.disabled = true;
    btn.innerHTML = '<span class="spinner-border spinner-border-sm" role="status"></span>';
    try {
      const r = await fetch('/api/password', {
        method:  'POST',
        headers: { 'Content-Type': 'application/json' },
        body:    JSON.stringify({
          current: document.getElementById('inp-pw-cur').value,
          newPassword: newPw,
        }),
      });
      if (!checkAuth(r)) return;
      const data = await r.json();
      if (r.ok) {
        showToast('Password changed successfully.', 'success');
        form.reset();
      } else {
        showToast('Error: ' + escHtml(data.error || 'Unknown error'), 'danger');
      }
    } catch (err) {
      showToast('Request failed: ' + escHtml(err.message), 'danger');
    } finally {
      btn.disabled = false;
      btn.innerHTML = 'Save';
    }
  }

  // ── Helpers ──────────────────────────────────────────────────────────────

  function showAlert(msg, type) {
    document.getElementById('alert-area').innerHTML =
      `<div class="alert alert-${type} alert-dismissible fade show" role="alert">
         ${msg}
         <button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
       </div>`;
  }

  function showToast(message, type) {
    const id = 'toast-' + Date.now();
    const c  = document.getElementById('toast-container');
    c.insertAdjacentHTML('beforeend', `
      <div id="${id}" class="toast align-items-center text-bg-${type} border-0"
           role="alert" aria-live="assertive" aria-atomic="true">
        <div class="d-flex">
          <div class="toast-body">${message}</div>
          <button type="button" class="btn-close btn-close-white me-2 m-auto"
                  data-bs-dismiss="toast" aria-label="Close"></button>
        </div>
      </div>`);
    const el = document.getElementById(id);
    new bootstrap.Toast(el, { delay: 4000 }).show();
    el.addEventListener('hidden.bs.toast', () => el.remove());
  }

  const _escEl = document.createElement('span');
  function escHtml(s)  { _escEl.textContent = String(s); return _escEl.innerHTML; }
  function escAttr(s) { return String(s).replace(/"/g, '&quot;').replace(/'/g, '&#39;'); }

  // ── Boot ──────────────────────────────────────────────────────────────────

  loadDevices();
</script>
</body>
</html>
)HTML";
