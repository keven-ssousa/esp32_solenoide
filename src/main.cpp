#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>
#include "wifi_secrets.h"

// ── Network ───────────────────────────────────────────────────────────────────
static const char* const WIFI_SSID     = WIFI_SECRET_SSID;
static const char* const WIFI_PASSWORD = WIFI_SECRET_PASSWORD;

static const IPAddress WIFI_IP  (192, 168,   1,  22);
static const IPAddress WIFI_GW  (192, 168,   1,   1);
static const IPAddress WIFI_SN  (255, 255, 255,   0);
static const IPAddress WIFI_DNS1(  8,   8,   8,   8);
static const IPAddress WIFI_DNS2(  8,   8,   4,   4);

// ── NTP ───────────────────────────────────────────────────────────────────────
static const char* NTP_SERVER  = "a.st1.ntp.br"; // servidor NTP oficial do NIC.br
static const char* NTP_SERVER2 = "b.st1.ntp.br";
static const char* TZ_BRAZIL   = "BRT3";          // Brasília: UTC-3, sem horário de verão

// ── Hardware ──────────────────────────────────────────────────────────────────
const int RELAY_PIN     = 14;
const int MAX_SCHEDULES = 10;

// ── Schedule ──────────────────────────────────────────────────────────────────
struct Schedule {
  uint8_t  hour;
  uint8_t  minute;
  uint16_t durationSec;
  bool     enabled;
};

Schedule schedules[MAX_SCHEDULES];
int      scheduleCount = 0;

uint8_t lastTriggerDay [MAX_SCHEDULES];
uint8_t lastTriggerHour[MAX_SCHEDULES];
uint8_t lastTriggerMin [MAX_SCHEDULES];

// ── Valve state ───────────────────────────────────────────────────────────────
bool          valveOpen        = false;
unsigned long scheduledCloseAt = 0; // millis() to auto-close; 0 = none

Preferences prefs;
WebServer   server(80);

// ── HTML ──────────────────────────────────────────────────────────────────────
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Válvula Solenoide</title>
  <style>
    *{box-sizing:border-box;margin:0;padding:0}
    :root{--bg:#0f172a;--surf:#1e293b;--bdr:#334155;--pri:#3b82f6;--red:#ef4444;--grn:#22c55e;--txt:#f1f5f9;--muted:#94a3b8}
    body{font-family:system-ui,sans-serif;background:var(--bg);color:var(--txt);min-height:100vh;padding:1rem;display:flex;flex-direction:column;align-items:center;gap:1rem}
    h1{font-size:1.1rem;color:var(--muted);letter-spacing:.08em;text-transform:uppercase}
    h2{font-size:.75rem;color:var(--muted);text-transform:uppercase;letter-spacing:.06em;margin-bottom:.75rem}
    .card{background:var(--surf);border:1px solid var(--bdr);border-radius:12px;padding:1.25rem;width:100%;max-width:420px}
    .clock{font-size:2.5rem;font-weight:700;font-variant-numeric:tabular-nums;text-align:center;letter-spacing:.04em}
    .sync{text-align:center;font-size:.72rem;color:var(--muted);margin-top:.2rem}
    .badge{display:inline-block;padding:.3rem 1rem;border-radius:999px;font-size:.9rem;font-weight:600;border:1.5px solid;margin-bottom:.75rem}
    .badge.open{color:var(--grn);border-color:var(--grn)}
    .badge.closed{color:var(--red);border-color:var(--red)}
    .auto-info{font-size:.75rem;color:var(--pri);margin-bottom:.6rem;min-height:1rem}
    .row{display:flex;gap:.6rem}
    button{flex:1;padding:.65rem;font-size:.95rem;font-weight:600;color:#fff;border:none;border-radius:8px;cursor:pointer;transition:opacity .15s,transform .1s}
    button:hover{opacity:.85}button:active{transform:scale(.97)}button:disabled{opacity:.35;cursor:not-allowed}
    .btn-open{background:var(--grn)}
    .btn-close{background:var(--red)}
    .btn-add{background:var(--pri);flex:none;padding:.65rem 1.1rem}
    .btn-sm{background:transparent;border:1px solid var(--bdr);flex:none;padding:.28rem .55rem;font-size:.75rem;color:var(--muted)}
    .form-row{display:flex;gap:.5rem;align-items:flex-end;margin-top:.9rem;flex-wrap:wrap}
    label{display:flex;flex-direction:column;gap:.25rem;font-size:.78rem;color:var(--muted);flex:1;min-width:90px}
    input{background:var(--bg);border:1px solid var(--bdr);border-radius:6px;color:var(--txt);padding:.5rem .6rem;font-size:.9rem;width:100%}
    input:focus{outline:none;border-color:var(--pri)}
    table{width:100%;border-collapse:collapse;font-size:.88rem}
    th{color:var(--muted);font-weight:500;font-size:.72rem;text-align:left;padding:0 0 .4rem}
    td{padding:.38rem 0;border-top:1px solid var(--bdr);vertical-align:middle}
    td:last-child{display:flex;gap:.3rem;justify-content:flex-end}
    .on{color:var(--grn)}.off{color:var(--muted)}
    .empty{color:var(--muted);font-size:.88rem}
  </style>
</head>
<body>
  <h1>Válvula Solenoide</h1>

  <div class="card">
    <div class="clock" id="clock">--:--:--</div>
    <div class="sync"  id="sync">sincronizando NTP...</div>
  </div>

  <div class="card">
    <h2>Controle Manual</h2>
    <div class="badge closed" id="badge">Válvula Fechada</div>
    <div class="auto-info" id="autoInfo"></div>
    <div class="row">
      <button class="btn-open"  id="btnOpen"  onclick="sendCmd('open')">Abrir</button>
      <button class="btn-close" id="btnClose" onclick="sendCmd('close')" disabled>Fechar</button>
    </div>
  </div>

  <div class="card">
    <h2>Agendamentos Diários</h2>
    <div id="schedList"><p class="empty">Carregando...</p></div>
    <form class="form-row" onsubmit="addSchedule(event)">
      <label>Horário<input type="time"   id="sTime" required></label>
      <label>Duração (min)<input type="number" id="sDur" min="1" max="999" value="5" required></label>
      <button type="submit" class="btn-add">+ Adicionar</button>
    </form>
  </div>

  <script>
    // ── Clock ─────────────────────────────────────────────────────────────────
    let srvMs = null, fetchAt = null;
    function syncClock() {
      fetch('/time').then(r=>r.json()).then(d=>{
        if (!d.synced) { document.getElementById('sync').textContent='NTP não sincronizado'; return; }
        const [h,m,s] = d.time.split(':').map(Number);
        const t = new Date(); t.setHours(h,m,s,0);
        srvMs = t.getTime(); fetchAt = performance.now();
        document.getElementById('sync').textContent = 'NTP sincronizado ✓';
      }).catch(()=>{});
    }
    function tickClock() {
      if (srvMs === null) return;
      const t  = new Date(srvMs + (performance.now() - fetchAt));
      const hh = String(t.getHours()).padStart(2,'0');
      const mm = String(t.getMinutes()).padStart(2,'0');
      const ss = String(t.getSeconds()).padStart(2,'0');
      document.getElementById('clock').textContent = hh+':'+mm+':'+ss;
    }
    syncClock();
    setInterval(syncClock, 30000);
    setInterval(tickClock, 1000);

    // ── Valve ─────────────────────────────────────────────────────────────────
    async function sendCmd(route) {
      document.getElementById('btnOpen').disabled  = true;
      document.getElementById('btnClose').disabled = true;
      try { const r = await fetch('/'+route); updateValve(await r.json()); } catch(e){}
    }
    function updateValve(j) {
      const badge = document.getElementById('badge');
      const info  = document.getElementById('autoInfo');
      if (j.open) {
        badge.textContent = 'Válvula Aberta'; badge.className = 'badge open';
        document.getElementById('btnOpen').disabled  = true;
        document.getElementById('btnClose').disabled = false;
        if (j.closeIn > 0) {
          const m = Math.floor(j.closeIn/60), s = j.closeIn%60;
          info.textContent = 'Fecha em ' + (m>0 ? m+'min ' : '') + s+'s';
        } else { info.textContent = ''; }
      } else {
        badge.textContent = 'Válvula Fechada'; badge.className = 'badge closed';
        document.getElementById('btnOpen').disabled  = false;
        document.getElementById('btnClose').disabled = true;
        info.textContent = '';
      }
    }
    setInterval(()=>fetch('/status').then(r=>r.json()).then(updateValve).catch(()=>{}), 5000);

    // ── Schedules ─────────────────────────────────────────────────────────────
    function loadSchedules() {
      fetch('/schedules').then(r=>r.json()).then(renderSchedules).catch(()=>{});
    }
    function renderSchedules(list) {
      const el = document.getElementById('schedList');
      if (!list.length) { el.innerHTML='<p class="empty">Nenhum agendamento.</p>'; return; }
      let h = '<table><thead><tr><th>Horário</th><th>Duração</th><th>Estado</th><th></th></tr></thead><tbody>';
      list.forEach(s => {
        const hh  = String(s.hour).padStart(2,'0'), mm = String(s.minute).padStart(2,'0');
        const dur = s.duration >= 60 ? Math.round(s.duration/60)+'min' : s.duration+'s';
        h += '<tr>'
           + '<td>'+hh+':'+mm+'</td>'
           + '<td>'+dur+'</td>'
           + '<td class="'+(s.enabled?'on':'off')+'">'+(s.enabled?'Ativo':'Pausado')+'</td>'
           + '<td>'
           + '<button class="btn-sm" onclick="toggleSched('+s.id+')">'+(s.enabled?'Pausar':'Ativar')+'</button>'
           + '<button class="btn-sm" onclick="deleteSched('+s.id+')">Excluir</button>'
           + '</td></tr>';
      });
      el.innerHTML = h + '</tbody></table>';
    }
    async function addSchedule(e) {
      e.preventDefault();
      const [h,m] = document.getElementById('sTime').value.split(':').map(Number);
      const d = parseInt(document.getElementById('sDur').value) * 60;
      await fetch('/schedule/add?h='+h+'&m='+m+'&d='+d);
      loadSchedules();
    }
    async function deleteSched(id) {
      await fetch('/schedule/delete?id='+id); loadSchedules();
    }
    async function toggleSched(id) {
      await fetch('/schedule/toggle?id='+id); loadSchedules();
    }

    // ── Init ──────────────────────────────────────────────────────────────────
    fetch('/status').then(r=>r.json()).then(updateValve).catch(()=>{});
    loadSchedules();
  </script>
</body>
</html>
)rawliteral";

// ── Persistence ───────────────────────────────────────────────────────────────
void saveSchedules() {
  prefs.begin("sched", false);
  prefs.putInt("cnt", scheduleCount);
  for (int i = 0; i < scheduleCount; i++) {
    char k[4];
    sprintf(k, "h%d", i); prefs.putUChar(k,  schedules[i].hour);
    sprintf(k, "m%d", i); prefs.putUChar(k,  schedules[i].minute);
    sprintf(k, "d%d", i); prefs.putUShort(k, schedules[i].durationSec);
    sprintf(k, "e%d", i); prefs.putBool(k,   schedules[i].enabled);
  }
  prefs.end();
}

void loadSchedules() {
  prefs.begin("sched", true);
  scheduleCount = prefs.getInt("cnt", 0);
  for (int i = 0; i < scheduleCount; i++) {
    char k[4];
    sprintf(k, "h%d", i); schedules[i].hour        = prefs.getUChar(k,  0);
    sprintf(k, "m%d", i); schedules[i].minute      = prefs.getUChar(k,  0);
    sprintf(k, "d%d", i); schedules[i].durationSec = prefs.getUShort(k, 60);
    sprintf(k, "e%d", i); schedules[i].enabled     = prefs.getBool(k,   true);
  }
  prefs.end();
  memset(lastTriggerDay,  0,   sizeof(lastTriggerDay));
  memset(lastTriggerHour, 255, sizeof(lastTriggerHour));
  memset(lastTriggerMin,  255, sizeof(lastTriggerMin));
}

// ── Valve helpers ─────────────────────────────────────────────────────────────
void openValve() {
  valveOpen = true;
  digitalWrite(RELAY_PIN, LOW);
  Serial.println(">> VALVE OPEN");
}

void closeValve() {
  valveOpen        = false;
  scheduledCloseAt = 0;
  digitalWrite(RELAY_PIN, HIGH);
  Serial.println(">> VALVE CLOSED");
}

// ── Schedule checker (called every 10s) ───────────────────────────────────────
void checkSchedules() {
  struct tm t;
  if (!getLocalTime(&t)) return;
  for (int i = 0; i < scheduleCount; i++) {
    if (!schedules[i].enabled) continue;
    if (t.tm_hour != schedules[i].hour || t.tm_min != schedules[i].minute) continue;
    if (lastTriggerDay[i]  == (uint8_t)t.tm_mday &&
        lastTriggerHour[i] == (uint8_t)t.tm_hour &&
        lastTriggerMin[i]  == (uint8_t)t.tm_min)  continue;
    lastTriggerDay[i]  = t.tm_mday;
    lastTriggerHour[i] = t.tm_hour;
    lastTriggerMin[i]  = t.tm_min;
    openValve();
    scheduledCloseAt = millis() + (unsigned long)schedules[i].durationSec * 1000UL;
    Serial.printf(">> SCHEDULE TRIGGERED: %02d:%02d close_in=%ds\n",
                  schedules[i].hour, schedules[i].minute, schedules[i].durationSec);
  }
}

// ── JSON builders ─────────────────────────────────────────────────────────────
String statusJson() {
  long closeIn = 0;
  if (scheduledCloseAt > 0) {
    unsigned long now = millis();
    closeIn = (scheduledCloseAt > now) ? (long)((scheduledCloseAt - now) / 1000UL) : 0;
  }
  return String("{\"open\":") + (valveOpen ? "true" : "false")
       + ",\"closeIn\":" + closeIn + "}";
}

String schedulesJson() {
  String j = "[";
  for (int i = 0; i < scheduleCount; i++) {
    if (i > 0) j += ",";
    j += "{\"id\":"      + String(i)
      +  ",\"hour\":"    + schedules[i].hour
      +  ",\"minute\":"  + schedules[i].minute
      +  ",\"duration\":" + schedules[i].durationSec
      +  ",\"enabled\":" + (schedules[i].enabled ? "true" : "false")
      +  "}";
  }
  return j + "]";
}

// ── HTTP handlers ─────────────────────────────────────────────────────────────
void handleRoot()     { server.send(200, "text/html", INDEX_HTML); }

void handleOpen() {
  scheduledCloseAt = 0;
  openValve();
  server.send(200, "application/json", statusJson());
}

void handleClose() {
  closeValve();
  server.send(200, "application/json", statusJson());
}

void handleStatus()    { server.send(200, "application/json", statusJson()); }
void handleSchedules() { server.send(200, "application/json", schedulesJson()); }

void handleTime() {
  struct tm t;
  if (!getLocalTime(&t)) {
    server.send(200, "application/json", "{\"synced\":false,\"time\":\"--:--:--\"}");
    return;
  }
  char buf[12];
  strftime(buf, sizeof(buf), "%H:%M:%S", &t);
  server.send(200, "application/json",
    String("{\"synced\":true,\"time\":\"") + buf + "\"}");
}

void handleScheduleAdd() {
  if (scheduleCount >= MAX_SCHEDULES) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"limit reached\"}");
    return;
  }
  int h = server.arg("h").toInt();
  int m = server.arg("m").toInt();
  int d = server.arg("d").toInt();
  if (h < 0 || h > 23 || m < 0 || m > 59 || d < 1) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid params\"}");
    return;
  }
  schedules[scheduleCount++] = { (uint8_t)h, (uint8_t)m, (uint16_t)d, true };
  saveSchedules();
  Serial.printf(">> SCHEDULE ADDED: %02d:%02d %ds\n", h, m, d);
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleScheduleDelete() {
  int id = server.arg("id").toInt();
  if (id < 0 || id >= scheduleCount) { server.send(400, "application/json", "{\"ok\":false}"); return; }
  for (int i = id; i < scheduleCount - 1; i++) schedules[i] = schedules[i + 1];
  scheduleCount--;
  saveSchedules();
  Serial.printf(">> SCHEDULE DELETED: id=%d\n", id);
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleScheduleToggle() {
  int id = server.arg("id").toInt();
  if (id < 0 || id >= scheduleCount) { server.send(400, "application/json", "{\"ok\":false}"); return; }
  schedules[id].enabled = !schedules[id].enabled;
  saveSchedules();
  Serial.printf(">> SCHEDULE %d: %s\n", id, schedules[id].enabled ? "ENABLED" : "DISABLED");
  server.send(200, "application/json",
    String("{\"ok\":true,\"enabled\":") + (schedules[id].enabled ? "true" : "false") + "}");
}

// ── Setup / Loop ──────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  Serial.printf("Relay pin: GPIO %d — HIGH (closed)\n", RELAY_PIN);

  loadSchedules();
  Serial.printf("Loaded %d schedule(s) from NVS.\n", scheduleCount);

  WiFi.config(WIFI_IP, WIFI_GW, WIFI_SN, WIFI_DNS1, WIFI_DNS2);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.printf("\nConnected! Visit: http://%s\n", WiFi.localIP().toString().c_str());

  configTime(0, 0, NTP_SERVER, NTP_SERVER2);
  setenv("TZ", TZ_BRAZIL, 1);
  tzset();
  Serial.println("NTP sync requested.");

  server.on("/",                handleRoot);
  server.on("/open",            handleOpen);
  server.on("/close",           handleClose);
  server.on("/status",          handleStatus);
  server.on("/time",            handleTime);
  server.on("/schedules",       handleSchedules);
  server.on("/schedule/add",    handleScheduleAdd);
  server.on("/schedule/delete", handleScheduleDelete);
  server.on("/schedule/toggle", handleScheduleToggle);
  server.on("/favicon.ico", []() { server.send(204); });
  server.onNotFound([]()    { server.send(404); });
  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  server.handleClient();

  if (scheduledCloseAt > 0 && millis() >= scheduledCloseAt) {
    closeValve();
  }

  static unsigned long lastCheck = 0;
  if (millis() - lastCheck >= 10000) {
    lastCheck = millis();
    checkSchedules();
  }
}
