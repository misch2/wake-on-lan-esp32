#pragma once

/**
 * Embedded Bootstrap 5 web interface for the Wake-on-LAN controller.
 * Fetches /api/devices on load and renders a card per device with a Wake button.
 * Wake requests are sent as POST /api/wake?mac=<mac>.
 */
static const char HTML_CONTENT[] = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Wake on LAN Controller</title>
  <link
    href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css"
    rel="stylesheet"
    integrity="sha384-QWTKZyjpPEjISv5WaRU9OFeRpok6YctnYmDr5pNlyT2bRjXh0JMhjY6hW+ALEwIH"
    crossorigin="anonymous"
  />
  <style>
    body { background-color: #f0f2f5; }
    .device-card {
      transition: transform 0.15s ease, box-shadow 0.15s ease;
    }
    .device-card:hover {
      transform: translateY(-3px);
      box-shadow: 0 0.5rem 1rem rgba(0,0,0,.15) !important;
    }
    .mac-badge { font-size: 0.78rem; letter-spacing: 0.04em; }
    #toast-container { z-index: 1090; }
  </style>
</head>
<body>

<nav class="navbar navbar-dark bg-dark shadow-sm">
  <div class="container">
    <span class="navbar-brand fs-5 fw-semibold">
      &#128268; Wake on LAN Controller
    </span>
    <span id="status-badge" class="badge bg-secondary">Loading&hellip;</span>
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

<script
  src="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js"
  integrity="sha384-YvpcrYf0tY3lHB60NNkmXc4s9bIOgUxi8T/jzmMXVbQOqIPHMNMKXIPJPFVl3d8z"
  crossorigin="anonymous"
></script>

<script>
  async function loadDevices() {
    try {
      const response = await fetch('/api/devices');
      if (!response.ok) throw new Error('HTTP ' + response.status);
      const devices = await response.json();
      renderDevices(devices);
      document.getElementById('status-badge').className = 'badge bg-success';
      document.getElementById('status-badge').textContent = devices.length + ' device(s)';
    } catch (err) {
      document.getElementById('device-list').innerHTML =
        '<div class="col-12"><div class="alert alert-danger">Failed to load devices: ' + err.message + '</div></div>';
      document.getElementById('status-badge').className = 'badge bg-danger';
      document.getElementById('status-badge').textContent = 'Error';
    }
  }

  function renderDevices(devices) {
    const list = document.getElementById('device-list');
    if (devices.length === 0) {
      list.innerHTML = '<div class="col-12"><div class="alert alert-warning">No devices configured.</div></div>';
      return;
    }
    list.innerHTML = devices.map(d => `
      <div class="col-12 col-sm-6 col-xl-4">
        <div class="card device-card shadow-sm h-100">
          <div class="card-body d-flex justify-content-between align-items-center gap-3">
            <div class="overflow-hidden">
              <h5 class="card-title mb-1 text-truncate">${escHtml(d.alias)}</h5>
              <span class="badge bg-light text-secondary mac-badge font-monospace">${escHtml(d.mac)}</span>
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

  async function wakeDevice(btn, mac, alias) {
    btn.disabled = true;
    btn.innerHTML = '<span class="spinner-border spinner-border-sm" role="status"></span>';
    try {
      const response = await fetch('/api/wake?mac=' + encodeURIComponent(mac), { method: 'POST' });
      const data = await response.json();
      if (response.ok) {
        showToast('Magic packet sent to <strong>' + escHtml(alias) + '</strong>', 'success');
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

  // Minimal XSS helpers
  const escEl = document.createElement('span');
  function escHtml(s) {
    escEl.textContent = String(s);
    return escEl.innerHTML;
  }
  function escAttr(s) {
    return String(s).replace(/"/g, '&quot;').replace(/'/g, '&#39;');
  }

  loadDevices();
</script>
</body>
</html>
)HTML";
