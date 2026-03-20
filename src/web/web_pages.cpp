#include "web.h"
#include "../storage.h"
#include "../motor.h"
#include "../door.h"
#include "../light.h"
#include "../lux.h"
#include "../system.h"
#include "../logger.h"
#include "../mqtt.h"
#include "../pins.h"
#include "../bme.h"
#include "../relay.h"
#include <WiFi.h>
#include <Arduino.h>
#include <ArduinoJson.h>

extern const char *FW_VERSION;

// ==================================================
// ERWEITERT
// ==================================================
void handleAdvanced()
{
    String html = renderThemeHead("Erweitert");
    html += R"rawliteral(
<div class="header"><h3>🔧 Erweitert</h3></div>
<div class="container">
  <div class="card">
    <div class="card-title">System</div>
    <div class="status-row"><span class="label">Firmware</span>       <span>%FW_VERSION%</span></div>
    <div class="status-row"><span class="label">WLAN Signal</span>    <span>%RSSI% dBm</span></div>
    <div class="status-row"><span class="label">Freier Speicher</span><span>%FREE_HEAP% KB</span></div>
  </div>
  <div class="card">
    <div class="card-title">Werkzeuge</div>
    <button onclick="location.href='/systemtest'" class="btn-open">🧪 Systemtest</button>
    <button onclick="location.href='/mqtt'"        class="btn-open">📡 MQTT Einstellungen</button>
    <button onclick="location.href='/espnow'"      class="btn-open">📶 ESP-NOW Geräte</button>
    <button onclick="location.href='/rgb'"         class="btn-open">🎨 Lichtfarbe & Helligkeit</button>
    <button onclick="location.href='/calibration'" class="btn-open">🎯 Kalibrierung</button>
    <button onclick="location.href='/blockade'"    class="btn-open">⚡ Blockadeerkennung</button>
    <button onclick="location.href='/log'"         class="btn-open">📜 Logbuch</button>
    <button onclick="location.href='/fw'"          class="btn-open">⬆️ Firmware Update</button>
    <button onclick="toggleTheme()"                class="btn-open">🌙 Dark/Light Mode</button>
  </div>
  <div class="card danger-zone">
    <div class="card-title" style="color:var(--red);">⚠️ System</div>
    <button class="btn-close" onclick="rebootESP()">🔄 ESP Neustart</button>
  </div>
</div>
<style>
.status-row{display:flex;justify-content:space-between;margin-bottom:12px;font-size:15px;}
.label{color:var(--muted);}
.danger-zone{border:1px solid rgba(239,68,68,0.3);}
.btn-open{margin-top:10px;background:var(--green);color:white;}
.btn-close{margin-top:10px;background:var(--red);color:white;}
</style>
<script>
function rebootESP(){ if(!confirm("ESP wirklich neu starten?")) return; fetch("/reset",{method:"POST"}); setTimeout(()=>location.href="/",4000); }
function toggleTheme(){ fetch("/set-theme",{method:"POST",body:new URLSearchParams({theme:document.documentElement.getAttribute("data-theme")==="dark"?"light":"dark"})}).then(()=>location.reload()); }
</script>
)rawliteral";
    html += renderFooter();
    html.replace("%FW_VERSION%", FW_VERSION);
    html.replace("%RSSI%",       String(WiFi.RSSI()));
    html.replace("%FREE_HEAP%",  String(ESP.getFreeHeap() / 1024));
    server.send(200, "text/html; charset=UTF-8", html);
}

// ==================================================
// ESP-NOW GERÄTE
// ==================================================
void handleEspNow()
{
    String html = renderThemeHead("ESP-NOW Geräte");

    // Letztes Signal BME formatieren
    String bmeLastSeen = "–";
    String bmeMacStr   = "–";
    if (bmeLastReceived > 0) {
        unsigned long ago = (millis() - bmeLastReceived) / 1000;
        if      (ago < 60)   bmeLastSeen = "vor " + String(ago) + " s";
        else if (ago < 3600) bmeLastSeen = "vor " + String(ago/60) + " min";
        else                 bmeLastSeen = "vor " + String(ago/3600) + " h";
        char mac[18];
        snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
            bmeLastSenderMac[0], bmeLastSenderMac[1], bmeLastSenderMac[2],
            bmeLastSenderMac[3], bmeLastSenderMac[4], bmeLastSenderMac[5]);
        bmeMacStr = String(mac);
    }

    html += R"rawliteral(
<div class="header"><h3>📶 ESP-NOW Geräte</h3></div>
<div class="container">

  <!-- Eigene MAC -->
  <div class="card">
    <div class="card-title">Diese Klappe (Empfänger)</div>
    <div class="status-row">
      <span class="label">MAC-Adresse</span>
      <code id="ownMac" style="font-size:14px;letter-spacing:1px;">…</code>
    </div>
    <div style="color:var(--muted);font-size:12px;margin-top:4px;">
      Diese MAC in jeden Sender-Sketch als <code>RECEIVER_MAC</code> eintragen.
    </div>
  </div>

  <!-- Geräteübersicht -->
  <div class="card">
    <div class="card-title">Verbundene Geräte</div>
    <div class="device-row %BME_STATUS_CLASS%">
      <div class="device-icon">🌡</div>
      <div class="device-info">
        <div class="device-name">BME280 Außensensor</div>
        <div class="device-meta"><span id="bmeLastSeen">%BME_LAST_SEEN%</span> · <span id="bmeSenderMac">%BME_SENDER_MAC%</span></div>
      </div>
      <div id="bmeBadge" class="device-badge %BME_BADGE_CLASS%">%BME_BADGE_TEXT%</div>
    </div>
    <div class="device-row">
      <div class="device-icon">🔌</div>
      <div class="device-info">
        <div class="device-name">Relais ESP32</div>
        <div class="device-meta"><span id="relayLastSeen">%RELAY_LAST_SEEN%</span> · <span id="relayMacMeta">%RELAY_MAC_META%</span></div>
      </div>
      <div id="relayBadge" class="device-badge %RELAY_BADGE_CLASS%">%RELAY_BADGE_TEXT%</div>
    </div>
    <!-- Weitere Geräte hier ergänzen -->
  </div>

  <!-- BME280 Einstellungen -->
  <div class="card">
    <div class="card-title">🌡 BME280 Außensensor</div>
    <div class="row-toggle">
      <div>
        <div style="font-size:15px;">ESP-NOW Empfang</div>
        <div style="color:var(--muted);font-size:12px;margin-top:2px;">Aus = I²C direkt · Ein = ESP-NOW</div>
      </div>
      <label class="toggle">
        <input type="checkbox" id="bmeEspNow" %BME_ESPNOW_CHECKED% onchange="saveBmeSource()">
        <span class="slider"></span>
      </label>
    </div>
    <div id="bmeInfo" style="display:%BME_INFO_DISPLAY%;margin-top:14px;background:var(--bg);border-radius:12px;padding:14px;font-size:13px;line-height:1.8;">
      <strong>Einrichtung Sender-ESP32:</strong><br>
      1. Sender-Sketch öffnen (<code>bme280_sender/</code>)<br>
      2. WLAN-Zugangsdaten eintragen<br>
      3. Flashen – keine MAC nötig, Klappe erkennt Sender automatisch
    </div>
  </div>

  <!-- Relais ESP -->
  <div class="card">
    <div class="card-title">🔌 Relais ESP32</div>
    <div class="row-toggle">
      <div>
        <div style="font-size:15px;">Relais aktivieren</div>
        <div style="color:var(--muted);font-size:12px;margin-top:2px;">Schaltet bei Automatik-Öffnung AN / Schließung AUS</div>
      </div>
      <label class="toggle">
        <input type="checkbox" id="relayEnabled" %RELAY_ENABLED_CHECKED% onchange="saveRelay()">
        <span class="slider"></span>
      </label>
    </div>
    <div id="relayConfig" style="display:%RELAY_CONFIG_DISPLAY%;margin-top:14px;">
      <div style="margin-bottom:8px;font-size:13px;color:var(--muted);">MAC-Adresse des Relais-ESP32:</div>
      <input type="text" id="relayMac" value="%RELAY_MAC_VALUE%"
        placeholder="AA:BB:CC:DD:EE:FF"
        style="width:100%;padding:10px;border-radius:10px;border:1px solid var(--border);background:var(--bg);color:var(--text);font-size:14px;font-family:monospace;letter-spacing:1px;box-sizing:border-box;">
      <button onclick="saveRelay()" style="margin-top:10px;width:100%;padding:10px;background:var(--green);color:#fff;border:none;border-radius:10px;font-size:14px;cursor:pointer;">Speichern</button>
      <div id="relaySaveMsg" style="display:none;color:var(--green);font-size:13px;margin-top:8px;text-align:center;">✅ Gespeichert</div>
      <div style="margin-top:14px;background:var(--bg2,var(--bg));border-radius:10px;padding:12px;font-size:12px;line-height:1.8;color:var(--muted);">
        <strong>Einrichtung:</strong><br>
        1. Relais-Sketch flashen (aus <code>relay_sender/</code>)<br>
        2. Im seriellen Monitor angezeigte MAC hier eintragen<br>
        3. Speichern – Relais schaltet bei nächster Automatik
      </div>
    </div>
  </div>

</div>
<style>
.status-row{display:flex;justify-content:space-between;align-items:center;margin-bottom:10px;font-size:15px;}
.label{color:var(--muted);}
.device-row{display:flex;align-items:center;gap:12px;padding:10px 0;border-bottom:1px solid var(--border);}
.device-row:last-child{border-bottom:none;}
.device-icon{font-size:24px;width:36px;text-align:center;}
.device-info{flex:1;}
.device-name{font-size:15px;font-weight:600;}
.device-meta{font-size:12px;color:var(--muted);margin-top:2px;}
.device-badge{padding:4px 12px;border-radius:999px;font-size:12px;font-weight:600;white-space:nowrap;}
.badge-ok{background:rgba(34,197,94,0.15);color:var(--green);}
.badge-warn{background:rgba(245,158,11,0.15);color:var(--orange);}
.badge-off{background:rgba(107,114,128,0.15);color:var(--muted);}
.row-toggle{display:flex;justify-content:space-between;align-items:center;}
.toggle{position:relative;display:inline-block;width:44px;height:24px;flex-shrink:0;}
.toggle input{opacity:0;width:0;height:0;}
.slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background:var(--muted);border-radius:24px;transition:.3s;}
.slider:before{position:absolute;content:"";height:18px;width:18px;left:3px;bottom:3px;background:white;border-radius:50%;transition:.3s;}
input:checked+.slider{background:var(--green);}
input:checked+.slider:before{transform:translateX(20px);}
</style>
<script>
fetch("/bme-mac").then(r=>r.text()).then(t=>{
  const parts=t.split("|");
  document.getElementById("ownMac").textContent=parts[0]||t;
});
function refreshStatus(){
  fetch("/espnow-status",{cache:"no-store"}).then(r=>r.json()).then(d=>{
    // Badge
    const badge=document.getElementById("bmeBadge");
    if(badge){
      if(!d.bmeEnabled){badge.className="device-badge badge-off";badge.textContent="Deaktiviert";}
      else if(d.bmeOk) {badge.className="device-badge badge-ok"; badge.textContent="Online";}
      else             {badge.className="device-badge badge-warn";badge.textContent="Kein Signal";}
    }
    // Meta
    const ls=document.getElementById("bmeLastSeen");   if(ls) ls.textContent=d.bmeLastSeen;
    const sm=document.getElementById("bmeSenderMac");  if(sm) sm.textContent=d.bmeSenderMac;
    // Relais Badge
    const rb=document.getElementById("relayBadge");
    if(rb){
      if(!d.relayEnabled)      {rb.className="device-badge badge-off"; rb.textContent="Deaktiviert";}
      else if(!d.relayMac)     {rb.className="device-badge badge-warn";rb.textContent="Keine MAC";}
      else if(d.relayOnline)   {rb.className="device-badge badge-ok";  rb.textContent="Online";}
      else                     {rb.className="device-badge badge-warn";rb.textContent="Kein Signal";}
    }
    const rm=document.getElementById("relayMacMeta"); if(rm) rm.textContent=d.relayMac||"Keine MAC";
    const rl=document.getElementById("relayLastSeen");if(rl) rl.textContent=d.relayLastSeen||"–";
  }).catch(()=>{});
}
refreshStatus();
setInterval(refreshStatus, 3000);
function saveBmeSource(){
  const src=document.getElementById("bmeEspNow").checked?"1":"0";
  fetch("/save-bme-source",{method:"POST",body:new URLSearchParams({source:src})})
    .then(()=>{
      document.getElementById("bmeInfo").style.display=src==="1"?"block":"none";
    });
}
function saveRelay(){
  const enabled=document.getElementById("relayEnabled").checked?"1":"0";
  const mac=document.getElementById("relayMac").value.trim();
  fetch("/save-relay",{method:"POST",body:new URLSearchParams({enabled,mac})})
    .then(()=>{
      document.getElementById("relayConfig").style.display=enabled==="1"?"block":"none";
      const msg=document.getElementById("relaySaveMsg");
      msg.style.display="block";
      setTimeout(()=>msg.style.display="none",2000);
    });
}
</script>
)rawliteral";

    html += renderFooter();
    // Eigene Werte ersetzen
    bool bmeActive = (bmeSource == BME_SOURCE_ESPNOW);
    String badgeClass, badgeText;
    if (!bmeActive)               { badgeClass = "badge-off";  badgeText = "Deaktiviert"; }
    else if (bmeOk)               { badgeClass = "badge-ok";   badgeText = "Online"; }
    else                          { badgeClass = "badge-warn"; badgeText = "Kein Signal"; }

    html.replace("%BME_ESPNOW_CHECKED%", bmeActive ? "checked" : "");
    html.replace("%BME_INFO_DISPLAY%",   bmeActive ? "block" : "none");
    html.replace("%BME_STATUS_CLASS%",   "");
    html.replace("%BME_BADGE_CLASS%",    badgeClass);
    html.replace("%BME_BADGE_TEXT%",     badgeText);
    html.replace("%BME_LAST_SEEN%",      bmeLastSeen);
    html.replace("%BME_SENDER_MAC%",     bmeMacStr);

    // Relais-Platzhalter
    String relayMacStr = "";
    if (relayMacValid()) {
        char rm[18];
        snprintf(rm, sizeof(rm), "%02X:%02X:%02X:%02X:%02X:%02X",
            relayMac[0], relayMac[1], relayMac[2],
            relayMac[3], relayMac[4], relayMac[5]);
        relayMacStr = String(rm);
    }
    String relayBadgeClass, relayBadgeText;
    bool relayOnline = relayEnabled && relayMacValid() &&
                       relayLastHeartbeat > 0 &&
                       (millis() - relayLastHeartbeat < 90000UL);
    if (!relayEnabled)       { relayBadgeClass = "badge-off";  relayBadgeText = "Deaktiviert"; }
    else if (!relayMacValid()){ relayBadgeClass = "badge-warn"; relayBadgeText = "Keine MAC"; }
    else if (relayOnline)    { relayBadgeClass = "badge-ok";   relayBadgeText = "Online"; }
    else                     { relayBadgeClass = "badge-warn"; relayBadgeText = "Kein Signal"; }

    String relayLastSeenStr = "–";
    if (relayLastHeartbeat > 0) {
        unsigned long ago = (millis() - relayLastHeartbeat) / 1000;
        if      (ago < 60)   relayLastSeenStr = "vor " + String(ago) + " s";
        else if (ago < 3600) relayLastSeenStr = "vor " + String(ago/60) + " min";
        else                 relayLastSeenStr = "vor " + String(ago/3600) + " h";
    }

    html.replace("%RELAY_ENABLED_CHECKED%", relayEnabled ? "checked" : "");
    html.replace("%RELAY_CONFIG_DISPLAY%",  relayEnabled ? "block" : "none");
    html.replace("%RELAY_MAC_VALUE%",       relayMacStr);
    html.replace("%RELAY_MAC_META%",        relayMacStr.isEmpty() ? "Keine MAC" : relayMacStr);
    html.replace("%RELAY_LAST_SEEN%",       relayLastSeenStr);
    html.replace("%RELAY_BADGE_CLASS%",     relayBadgeClass);
    html.replace("%RELAY_BADGE_TEXT%",      relayBadgeText);

    server.send(200, "text/html; charset=UTF-8", html);
}

// ==================================================
// BLOCKADEERKENNUNG
// ==================================================
void handleBlockade()
{
    // Aktuellen Strom live messen (nur wenn Motor steht – Leerlaufwert zeigen)
    float liveCurrent = 0.0f;
    if (motorState == MOTOR_STOPPED)
    {
        const int S = 20; long sum = 0;
        for (int i = 0; i < S; i++) { sum += analogRead(ACS712_PIN); delay(1); }
        float vMeas   = (sum / S) * (3.3f / 4095.0f);
        float vSensor = vMeas / (20.0f / 30.0f);
        liveCurrent   = fabsf((vSensor - ACS712_ZERO_V) / (ACS712_MV_PER_A / 1000.0f));
    }

    String html = renderThemeHead("Blockadeerkennung");
    html += R"rawliteral(
<div class="header" style="position:relative;">
  <h3>⚡ Blockadeerkennung</h3>
  <button onclick="toggleHelp()" style="position:absolute;top:22px;right:16px;width:32px;height:32px;border-radius:50%;background:var(--bg);color:var(--muted);font-size:16px;font-weight:700;padding:0;border:1px solid var(--border);">?</button>
</div>

<!-- HILFE MODAL -->
<div id="helpOverlay" onclick="toggleHelp()" style="display:none;position:fixed;inset:0;background:rgba(0,0,0,0.5);z-index:100;padding:20px;box-sizing:border-box;overflow-y:auto;">
  <div onclick="event.stopPropagation()" style="background:var(--card);border-radius:20px;padding:22px;max-width:400px;margin:auto;margin-top:40px;">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:16px;">
      <span style="font-weight:700;font-size:17px;">⚡ Was ist Blockadeerkennung?</span>
      <button onclick="toggleHelp()" style="width:28px;height:28px;border-radius:50%;background:var(--bg);color:var(--muted);font-size:14px;padding:0;border:none;">✕</button>
    </div>

    <p style="font-size:14px;line-height:1.6;color:var(--text);">
      Der Motor der Klappe zieht normalerweise nur wenig Strom wenn er frei läuft.
      Klemmt etwas – zum Beispiel ein Huhn oder ein Ast – steigt der Strom stark an.
      Die Blockadeerkennung misst das und stoppt den Motor bevor etwas kaputtgeht.
    </p>

    <div style="background:var(--bg);border-radius:14px;padding:14px;margin:14px 0;font-size:14px;line-height:1.8;">
      <div><span style="color:var(--muted);">🔋 Baseline</span><br>
      Normaler Strom beim freien Lauf – wird automatisch beim Motorstart gemessen.</div>
      <hr style="border:none;border-top:1px solid var(--border);margin:10px 0;">
      <div><span style="color:var(--muted);">📈 Höchster Strom</span><br>
      Der höchste gemessene Wert seit dem letzten Reset – nützlich zur Einstellung.</div>
      <hr style="border:none;border-top:1px solid var(--border);margin:10px 0;">
      <div><span style="color:var(--muted);">⚙️ Einstellung</span><br>
      Wie viel mehr Strom als normal erlaubt ist bevor der Motor stoppt.</div>
      <hr style="border:none;border-top:1px solid var(--border);margin:10px 0;">
      <div><span style="color:var(--muted);">🚨 Aktiver Schwellwert</span><br>
      Baseline + Einstellung = Auslösepunkt. Wird dieser Wert überschritten stoppt der Motor sofort.</div>
    </div>

    <div style="background:rgba(34,197,94,0.1);border-radius:14px;padding:14px;font-size:14px;line-height:1.6;">
      <strong>💡 So richtest du es ein:</strong><br>
      1. Motor einmal normal laufen lassen<br>
      2. <em>Höchster Strom</em> ablesen → das ist dein Normalwert<br>
      3. Klappe kurz von Hand blockieren<br>
      4. <em>Höchster Strom</em> ablesen → das ist der Blockade-Wert<br>
      5. Einstellung = Mitte zwischen beiden Werten<br><br>
      <strong>Beispiel:</strong> Normal 0.8 A, Blockade 3.5 A<br>
      → Einstellung: (3.5 − 0.8) ÷ 2 = <strong>1.4 A</strong><br>
      → Auslösung bei: 0.8 + 1.4 = <strong>2.2 A</strong>
    </div>

    <button onclick="toggleHelp()" style="margin-top:16px;width:100%;padding:12px;border:none;border-radius:12px;font-size:15px;font-weight:600;background:var(--green);color:white;cursor:pointer;">
      Verstanden ✓
    </button>
  </div>
</div>

<div class="container">

  <div class="card">
    <div class="card-title">Live-Messung (Motor gestoppt)</div>
    <div class="status-row">
      <span class="label">Aktueller Strom</span>
      <span id="live">%LIVE_A% A</span>
    </div>
    <div class="status-row">
      <span class="label">Eingemessene Baseline</span>
      <span id="baseline">%BASELINE_A% A</span>
    </div>
    <div class="status-row">
      <span class="label">Höchster gemessener Strom</span>
      <span id="peak" style="font-weight:600;color:var(--red);">%PEAK_A% A</span>
    </div>
    <div class="status-row">
      <span class="label">Aktiver Schwellwert</span>
      <span>%TRIGGER_A% A <span style="color:var(--muted);font-size:12px;">(Baseline + Einstellung)</span></span>
    </div>
    <button onclick="resetPeak()" class="btn-reset" style="margin-top:8px;">↺ Peak zurücksetzen</button>
  </div>

  <div class="card">
    <div class="card-title">Einstellungen</div>
    <form id="frm">
      <div class="row-toggle">
        <span>Blockadeerkennung aktiv</span>
        <label class="toggle"><input type="checkbox" id="en" name="enabled" %CHECKED%><span class="slider"></span></label>
      </div>
      <div class="field-row" style="margin-top:16px;">
        <label>Schwellwert über Baseline (A)</label>
        <input type="number" id="thr" name="threshold" value="%THRESHOLD%" min="0.5" max="10" step="0.1" style="width:80px;text-align:right;">
      </div>
      <div style="color:var(--muted);font-size:12px;margin-top:6px;">
        Empfehlung: Leerlaufstrom × 4 bis 5.<br>
        Bei zu vielen Fehlauslösungen erhöhen, bei träger Reaktion verringern.
      </div>
      <button type="button" onclick="save()" class="btn-open" style="margin-top:16px;">💾 Speichern</button>
    </form>
    <div id="msg" style="margin-top:10px;font-size:14px;color:var(--green);display:none;">✅ Gespeichert</div>
  </div>

</div>
<style>
.status-row{display:flex;justify-content:space-between;align-items:center;margin-bottom:12px;font-size:15px;}
.label{color:var(--muted);}
.row-toggle{display:flex;justify-content:space-between;align-items:center;margin-bottom:4px;}
.field-row{display:flex;justify-content:space-between;align-items:center;}
.field-row label{color:var(--muted);font-size:14px;}
.field-row input{background:var(--bg);color:var(--text);border:1px solid var(--border);border-radius:8px;padding:6px 10px;font-size:15px;}
.toggle{position:relative;display:inline-block;width:44px;height:24px;}
.toggle input{opacity:0;width:0;height:0;}
.slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background:var(--muted);border-radius:24px;transition:.3s;}
.slider:before{position:absolute;content:"";height:18px;width:18px;left:3px;bottom:3px;background:white;border-radius:50%;transition:.3s;}
input:checked+.slider{background:var(--green);}
input:checked+.slider:before{transform:translateX(20px);}
.btn-open{width:100%;padding:12px;border:none;border-radius:12px;font-size:15px;font-weight:600;cursor:pointer;background:var(--green);color:white;}
.btn-reset{width:100%;padding:10px;border:none;border-radius:12px;font-size:14px;font-weight:600;cursor:pointer;background:var(--bg);color:var(--muted);border:1px solid var(--border);}
</style>
<script>
function toggleHelp(){
  const o=document.getElementById("helpOverlay");
  o.style.display = o.style.display==="none" ? "block" : "none";
}
function save(){
  const body=new URLSearchParams();
  body.append("enabled", document.getElementById("en").checked?"1":"0");
  body.append("threshold", document.getElementById("thr").value);
  fetch("/save-blockade",{method:"POST",body})
    .then(r=>{if(r.ok){const m=document.getElementById("msg");m.style.display="block";setTimeout(()=>m.style.display="none",2000);}});
}
function refreshLive(){
  fetch("/blockade-live").then(r=>r.text()).then(t=>{
    document.getElementById("live").textContent=t+" A";
  });
  fetch("/blockade-peak").then(r=>r.text()).then(t=>{
    document.getElementById("peak").textContent=t+" A";
  });
  fetch("/blockade-baseline").then(r=>r.text()).then(t=>{
    document.getElementById("baseline").textContent = t==="--" ? "-- (Motor läuft noch nicht)" : t+" A";
  });
}
function resetPeak(){
  fetch("/blockade-peak-reset",{method:"POST"}).then(()=>{
    document.getElementById("peak").textContent="0.00 A";
  });
}
setInterval(refreshLive, 1000);
refreshLive();
</script>
)rawliteral";
    html += renderFooter();
    html.replace("%LIVE_A%",     String(liveCurrent,  2));
    html.replace("%BASELINE_A%", String(currentBaseline, 2));
    html.replace("%PEAK_A%",     String(peakCurrentA, 2));
    html.replace("%TRIGGER_A%",  String(currentBaseline + blockadeThresholdA, 2));
    html.replace("%CHECKED%",    blockadeEnabled ? "checked" : "");
    html.replace("%THRESHOLD%",  String(blockadeThresholdA, 1));
    server.send(200, "text/html; charset=UTF-8", html);
}

// ==================================================
// FIRMWARE UPDATE
// ==================================================
void handleFw()
{
    String html = renderThemeHead("Firmware Update");
    html += R"rawliteral(
<div class="header"><h2>⬆️ Firmware Update</h2></div>
<div class="container">
  <div class="card">
    <div style="margin-bottom:12px;font-size:14px;color:var(--muted);">Aktuelle Version: <strong>%FW_VERSION%</strong></div>
    <div style="background:rgba(239,68,68,0.08);padding:10px 12px;border-radius:10px;margin-top:10px;">
      <span style="color:var(--red);font-weight:700;">⚠ Während des Updates:</span><br>
      • Gerät nicht ausschalten<br>• WLAN nicht trennen<br>• Nach Update startet das Gerät neu!
    </div><br>
    <div class="card-title">Neue Firmware hochladen <i>(*.bin)</i></div>
    <form id="uploadForm">
      <input type="file" id="file" name="update" accept=".bin" required>
      <button type="submit" class="btn-open">Update starten</button>
    </form>
    <div id="progressContainer" style="margin-top:15px;display:none;">
      <div style="height:10px;background:var(--bg);border-radius:10px;"><div id="progressBar" style="height:10px;width:0%;background:var(--green);border-radius:10px;"></div></div>
      <div id="progressText" style="margin-top:8px;font-size:13px;color:var(--muted);">0 %</div>
    </div>
    <div id="statusMsg" style="margin-top:15px;font-weight:600;"></div>
  </div>
</div>
<script>
document.getElementById("uploadForm").addEventListener("submit",function(e){
  e.preventDefault();
  const f=document.getElementById("file"); if(!f.files.length){alert("Bitte Datei wählen.");return;}
  const fd=new FormData(); fd.append("update",f.files[0]);
  const xhr=new XMLHttpRequest(); xhr.open("POST","/update",true);
  xhr.upload.onprogress=function(e){if(e.lengthComputable){const p=Math.round(e.loaded/e.total*100);document.getElementById("progressContainer").style.display="block";document.getElementById("progressBar").style.width=p+"%";document.getElementById("progressText").innerText=p+" %";}};
  xhr.onload=function(){if(xhr.status==200){document.getElementById("statusMsg").innerHTML="✅ Update erfolgreich. Neustart...";setTimeout(()=>location.href="/",5000);}else document.getElementById("statusMsg").innerHTML="❌ Update fehlgeschlagen.";};
  xhr.send(fd);
});
</script>
)rawliteral";
    html += renderFooter();
    html.replace("%FW_VERSION%", FW_VERSION);
    server.send(200, "text/html; charset=UTF-8", html);
}

// ==================================================
// SYSTEMTEST
// ==================================================
void handleSelftest()
{
    String html = renderThemeHead("Systemtest");
    html += R"rawliteral(
<div class="header"><h3>🧪 Systemtest</h3></div>
<div class="container">
  <div class="card">
    <div class="card-title">System</div>
    <div class="status-row"><span class="label">Freier Speicher</span><span id="heapStatus">%FREE_HEAP% KB</span></div>
    <div class="status-row"><span class="label">Firmware</span>       <span>%FW_VERSION%</span></div>
  </div>
  <div class="card">
    <div class="card-title">Sensoren</div>
    <div class="status-row"><span class="label">Lichtsensor</span> <span id="luxStatus">–</span></div>
    <div class="status-row"><span class="label">RTC (DS3231)</span><span id="rtcStatus">–</span></div>
    <div class="status-row"><span class="label">BME280</span>      <span id="bmeStatus">–</span></div>
    <div class="status-row"><span class="label">Türposition</span> <span>%DOOR_STATE%</span></div>
  </div>
  <div class="card">
    <div class="card-title">Netzwerk</div>
    <div class="status-row"><span class="label">MQTT</span>       <span id="mqttStatus">%MQTT_STATUS%</span></div>
    <div class="status-row"><span class="label">WLAN Signal</span><span id="rssiStatus">%RSSI% dBm</span></div>
    <div class="status-row"><span class="label">IP-Adresse</span> <span>%IP%</span></div>
  </div>
  <div class="card"><button onclick="location.reload()" class="btn-open">🔄 Erneut prüfen</button></div>
</div>
<style>
.status-row{display:flex;justify-content:space-between;margin-bottom:12px;font-size:15px;}
.label{color:var(--muted);}
.ok{color:var(--green);font-weight:600;}.warn{color:var(--orange);font-weight:600;}.error{color:var(--red);font-weight:600;}
.btn-open{margin-top:10px;background:var(--green);color:white;}
</style>
<script>
function setClass(el,cls){el.classList.remove("ok","warn","error");el.classList.add(cls);}
function evaluateHeap(){const h=parseInt("%FREE_HEAP%"),el=document.getElementById("heapStatus");if(!el||isNaN(h))return;setClass(el,h>100?"ok":h>50?"warn":"error");}
function evaluateRSSI(){const r=parseInt("%RSSI%"),el=document.getElementById("rssiStatus");if(!el||isNaN(r))return;setClass(el,r>-65?"ok":r>-80?"warn":"error");}
function evaluateMQTT(){const el=document.getElementById("mqttStatus"),s="%MQTT_STATUS%";if(!el)return;setClass(el,s==="Verbunden"?"ok":s==="Deaktiviert"?"warn":"error");}
function evaluateRTC(ok,text){const el=document.getElementById("rtcStatus");if(!el)return;el.textContent=text||(ok?"OK":"Nicht gefunden");setClass(el,ok?"ok":"error");}
function setLuxStatus(isOk,value){const el=document.getElementById("luxStatus");if(!el)return;if(!isOk||!Number.isFinite(value)||value<0){el.textContent="Nicht gefunden / Fehler";setClass(el,"error");}else{el.textContent=value.toFixed(1)+" lx";setClass(el,"ok");}}
function setBmeStatus(isOk,t,h,p){const el=document.getElementById("bmeStatus");if(!el)return;if(!isOk){el.textContent="n/a (kein Sensor)";setClass(el,"warn");}else{el.textContent=t+"°C · "+h+"% · "+p+" hPa";setClass(el,"ok");}}
evaluateHeap();evaluateRSSI();evaluateMQTT();
fetch("/systemtest-status",{cache:"no-store"}).then(r=>r.json()).then(d=>{evaluateRTC(d.rtcOk===1||d.rtcOk===true,d.rtcStatus);setLuxStatus(!!d.bhOk,Number(d.lux));setBmeStatus(!!d.bmeOk,d.bmeTemp,d.bmeHumidity,d.bmePressure);}).catch(()=>{});
</script>
)rawliteral";
    html += renderFooter();
    html.replace("%FREE_HEAP%",   String(ESP.getFreeHeap() / 1024));
    html.replace("%FW_VERSION%",  FW_VERSION);
    html.replace("%DOOR_STATE%",  doorOpen ? "Offen" : "Geschlossen");
    html.replace("%RSSI%",        String(WiFi.RSSI()));
    html.replace("%IP%",          WiFi.localIP().toString());
    bool mc = mqttSettings.enabled && mqttClientConnected();
    html.replace("%MQTT_STATUS%", !mqttSettings.enabled ? "Deaktiviert" : mc ? "Verbunden" : "Nicht verbunden");
    server.send(200, "text/html; charset=UTF-8", html);
}

// ==================================================
// MQTT EINSTELLUNGEN
// ==================================================
void handleMqtt()
{
    String html = renderThemeHead("MQTT Einstellungen");
    html += R"rawliteral(
<div class="header"><h3>📡 MQTT Einstellungen</h3></div>
<div class="container">
  <div class="card">
    <form method="POST" action="/save-mqtt">
      <div class="section">
        <div class="section-title">🔌 Verbindung</div>
        <div class="field"><label><input type="checkbox" name="enabled" %MQTT_ENABLED%> MQTT aktivieren</label></div>
        <div class="row">
          <div class="field"><label>Broker Host</label><input name="host" value="%HOST%"></div>
          <div class="field small"><label>Port</label><input name="port" type="number" value="%PORT%"></div>
        </div>
      </div>
      <div class="section">
        <div class="section-title">🆔 Identität</div>
        <div class="field"><label>Client ID</label><input name="clientId" value="%CLIENTID%"></div>
        <div class="field"><label>Base Topic</label><input name="base" value="%BASE%"></div>
        <div class="field"><label>User</label><input name="user" value="%USER%"></div>
        <div class="field">
          <label>Passwort</label>
          <div class="password-wrapper">
            <input id="mqttPass" name="pass" type="password" value="%PASS%">
            <button type="button" onclick="togglePass()" class="eye-btn">👁</button>
          </div>
        </div>
        <button type="button" onclick="testMqtt()" class="btn-test">🧪 Verbindung testen</button>
        <div id="mqttTestResult" style="margin-top:8px;font-size:13px;font-weight:600;text-align:center;"></div>
      </div>
      <div class="status-box %MQTT_STATUS_CLASS%">%MQTT_STATUS%</div>
      <button type="submit" class="btn-open">💾 Speichern</button>
    </form>
  </div>
</div>
<style>
.section{margin-bottom:20px;}.password-wrapper{display:flex;align-items:center;gap:8px;}.password-wrapper input{flex:1;}
.eye-btn{width:38px;height:38px;border-radius:12px;padding:0;margin:0;background:var(--green);color:white;font-size:16px;}
.btn-test{background:var(--orange);width:60%;margin:14px auto 0 auto;display:block;padding:8px 12px;font-size:14px;border-radius:12px;color:white;}
.section-title{font-size:13px;font-weight:600;text-transform:uppercase;margin-bottom:12px;color:var(--muted);}
.row{display:flex;gap:12px;}.field{flex:1;display:flex;flex-direction:column;}.field.small{flex:0 0 100px;}
label{font-size:13px;margin-bottom:6px;color:var(--muted);}
.status-box{margin:18px 0;padding:10px;border-radius:10px;font-size:14px;font-weight:600;text-align:center;}
.status-ok{background:rgba(34,197,94,0.1);color:var(--green);}.status-error{background:rgba(239,68,68,0.1);color:var(--red);}.status-warn{background:rgba(245,158,11,0.1);color:var(--orange);}
</style>
<script>
function testMqtt(){
  const result=document.getElementById("mqttTestResult");result.innerHTML="⏳ Teste...";
  fetch("/mqtt-test",{method:"POST",body:new FormData(document.querySelector("form"))}).then(r=>r.text()).then(t=>{
    if(t==="OK"){result.innerHTML="✅ Verbindung erfolgreich";result.style.color="var(--green)";}
    else{result.innerHTML="❌ Verbindung fehlgeschlagen";result.style.color="var(--red)";}
  }).catch(()=>{result.innerHTML="❌ Fehler";result.style.color="var(--red)";});
}
function togglePass(){const p=document.getElementById("mqttPass");p.type=(p.type==="password")?"text":"password";}
</script>
)rawliteral";

    html += renderFooter();
    bool connected = mqttSettings.enabled && mqttClientConnected();
    html.replace("%HOST%",             mqttSettings.host);
    html.replace("%PORT%",             String(mqttSettings.port == 0 || mqttSettings.port == 65535 ? 1883 : mqttSettings.port));
    html.replace("%USER%",             mqttSettings.user);
    html.replace("%PASS%",             mqttSettings.pass);
    html.replace("%CLIENTID%",         mqttSettings.clientId);
    html.replace("%BASE%",             mqttSettings.base);
    html.replace("%MQTT_ENABLED%",     mqttSettings.enabled ? "checked" : "");
    html.replace("%MQTT_STATUS%",      !mqttSettings.enabled ? "Deaktiviert" : connected ? "Verbunden" : "Nicht verbunden");
    html.replace("%MQTT_STATUS_CLASS%",!mqttSettings.enabled ? "status-warn" : connected ? "status-ok" : "status-error");
    server.send(200, "text/html; charset=UTF-8", html);
}

void handleSaveMqtt()
{
    mqttSettings.enabled = server.hasArg("enabled");
    if (!mqttSettings.enabled) mqttClient.disconnect();
    strncpy(mqttSettings.host,     server.arg("host").c_str(),     39); mqttSettings.host[39]     = '\0';
    mqttSettings.port = server.arg("port").toInt();
    strncpy(mqttSettings.user,     server.arg("user").c_str(),     31); mqttSettings.user[31]     = '\0';
    strncpy(mqttSettings.pass,     server.arg("pass").c_str(),     31); mqttSettings.pass[31]     = '\0';
    strncpy(mqttSettings.clientId, server.arg("clientId").c_str(), 31); mqttSettings.clientId[31] = '\0';
    strncpy(mqttSettings.base,     server.arg("base").c_str(),     31); mqttSettings.base[31]     = '\0';
    saveMqttSettings();
    mqttClient.disconnect();
    mqttSetup();
    server.sendHeader("Location", "/mqtt");
    server.send(303);
}

// ==================================================
// KALIBRIERUNG
// ==================================================
void handleCalibration()
{
    String html = renderThemeHead("Kalibrierung");
    html += R"rawliteral(
<div class="header"><h3>🎯 Kalibrierung</h3></div>
<div class="container">
  <div class="card">
    <div class="card-title">🧰 Einlernen</div>
    <p style="font-size:14px;color:var(--muted);">Fahre die Klappe in die gewünschte Endposition und starte den Einlernmodus.</p>
    <button class="btn-open" onclick="startLearn()">Einlernmodus starten</button>
    <div id="learnStatus" style="margin-top:12px;font-size:14px;"></div>
  </div>
  <div class="card">
    <div class="card-title">🎚 Manuelle Steuerung</div>
    <div style="display:flex;gap:10px;">
      <button class="btn-open"  style="flex:1;" onclick="motorUp()">⬆ Hoch</button>
      <button style="flex:1;background:var(--orange);color:white;border:none;border-radius:14px;padding:14px;font-weight:600;cursor:pointer;" onclick="motorStopBtn()">⏹ Stop</button>
      <button class="btn-close" style="flex:1;" onclick="motorDown()">⬇ Runter</button>
    </div>
    <div id="motorStatus" style="margin-top:12px;font-size:14px;"></div>
  </div>
  <div class="card">
    <div class="card-title">📊 Endpositionen</div>
    <div style="display:flex;gap:16px;margin-top:10px;">
      <div style="flex:1;background:var(--bg);padding:14px;border-radius:12px;text-align:center;"><div style="font-size:13px;color:var(--muted);">Offen</div><div id="posOpen" style="font-weight:bold;font-size:18px;">--</div></div>
      <div style="flex:1;background:var(--bg);padding:14px;border-radius:12px;text-align:center;"><div style="font-size:13px;color:var(--muted);">Geschlossen</div><div id="posClose" style="font-weight:bold;font-size:18px;">--</div></div>
    </div>
  </div>
</div>
<script>
function motorUp()     { fetch("/motor/up");   document.getElementById("motorStatus").innerHTML="⬆ Motor läuft hoch..."; }
function motorDown()   { fetch("/motor/down"); document.getElementById("motorStatus").innerHTML="⬇ Motor läuft runter..."; }
function motorStopBtn(){ fetch("/motor/stop"); document.getElementById("motorStatus").innerHTML="⏹ Motor gestoppt."; }
function startLearn()  { document.getElementById("learnStatus").innerHTML="⏳ Wird gestartet..."; fetch("/learn-start",{method:"POST"}).then(r=>{document.getElementById("learnStatus").innerHTML=r.ok?"✅ Einlernmodus aktiv.":"❌ Fehler.";}); }
function updatePositions(){ fetch("/calib-status").then(r=>r.json()).then(d=>{document.getElementById("posOpen").innerHTML=d.open;document.getElementById("posClose").innerHTML=d.close;}).catch(()=>{}); }
setInterval(updatePositions,2000); updatePositions();
</script>
)rawliteral";
    html += renderFooter();
    server.send(200, "text/html; charset=UTF-8", html);
}

void handleLearn()
{
    if (learningActive)            { server.send(200, "text/plain", "Learning already active"); return; }
    if (motorState != MOTOR_STOPPED){ server.send(409, "text/plain", "Motor running"); return; }
    learningActive   = true;
    learningOpenDone = false;
    learnStartTime   = millis();
    motorReason      = "Einlernen";
    startMotorOpen(30000);  // 30s Timeout – Taster drücken wenn oben
    addLog("Einlernen gestartet – fahre Richtung OPEN");
    server.send(200, "text/plain", "Learning started");
}

void handleLearnPage()
{
    server.send(200, "text/html; charset=UTF-8", R"rawliteral(
<!DOCTYPE html><html lang="de"><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Einlernen</title>
<style>body{font-family:Arial,sans-serif;background:#f2f4f7;margin:0;padding:20px;}.card{background:white;padding:20px;border-radius:12px;box-shadow:0 2px 6px rgba(0,0,0,0.1);text-align:center;}button{width:100%;padding:14px;font-size:18px;border:none;border-radius:8px;background:#4CAF50;color:white;cursor:pointer;}</style>
</head><body>
<div class="card">
  <h2>Einlernen starten?</h2>
  <p>Die Klappe wird geöffnet.<br>Drücke den Taster, um die Endlagen zu speichern.</p>
  <button onclick="fetch('/learn').then(()=>location.href='/')">Ja, Einlernen starten</button>
  <br><br><a href="/">Abbrechen</a>
</div>
</body></html>
)rawliteral");
}

// ==================================================
// LOGBUCH
// ==================================================
void handleLogbook()
{
    String html = renderThemeHead("Logbuch");
    html += R"rawliteral(
<div class="header"><h3>📜 Logbuch</h3></div>
<div class="container">
  <div class="card">
    <div class="card-title">Ereignisse</div>
    <div id="logContainer" class="log-container">%LOG_ENTRIES%</div>
    <button class="btn-close" onclick="clearLog()">🗑 Log löschen</button>
    <button class="btn-close" onclick="location.href='/log/download'" style="background:var(--blue,#3b82f6);margin-top:8px;">⬇️ Log herunterladen</button>
  </div>
</div>
<style>
.log-container{max-height:320px;overflow-y:auto;margin-top:10px;margin-bottom:14px;padding-right:4px;}
.log-entry{padding:10px 12px;margin-bottom:8px;border-radius:12px;background:var(--bg);font-size:14px;line-height:1.4;border-left:4px solid var(--green);}
.log-entry.error{border-left:4px solid var(--red);}.log-entry.warn{border-left:4px solid var(--orange);}
.btn-close{background:var(--red);}
</style>
<script>
document.getElementById("logContainer").scrollTop=document.getElementById("logContainer").scrollHeight;
function clearLog(){ fetch("/log/clear",{method:"POST"}).then(()=>location.reload()); }
</script>
)rawliteral";
    html += renderFooter();
    html.replace("%LOG_ENTRIES%", buildLogHTML());
    server.send(200, "text/html; charset=UTF-8", html);
}

// ==================================================
// RGB FARBE & HELLIGKEIT
// ==================================================
void handleRgb()
{
    String html = renderThemeHead("Lichtfarbe");
    html += R"rawliteral(
<div class="header"><h3>🎨 Lichtfarbe & Helligkeit</h3></div>
<div class="container">
  <div class="card">
    <div class="card-title">Locklicht Farbe</div>

    <!-- Vorschau -->
    <div id="preview" style="width:100%;height:60px;border-radius:12px;margin-bottom:18px;
      background:rgb(%PREV_R%,%PREV_G%,%PREV_B%);transition:background .2s;
      border:1px solid var(--border);"></div>

    <!-- Colorpicker -->
    <div class="form-row">
      <label>Farbe wählen</label>
      <input type="color" id="colorPicker" value="%HEX_COLOR%"
        style="width:100%;height:44px;border:none;border-radius:10px;cursor:pointer;background:none;"
        oninput="onColorPick()">
    </div>

    <!-- Manuelle RGB Eingabe -->
    <div style="display:flex;gap:10px;margin-bottom:14px;">
      <div style="flex:1;">
        <label style="font-size:12px;color:var(--muted);display:block;margin-bottom:4px;">R</label>
        <input type="number" id="rVal" min="0" max="255" value="%R%"
          style="width:100%;padding:8px;background:var(--bg);border:1px solid var(--border);
          border-radius:8px;color:var(--text);font-size:14px;" oninput="onRgbInput()">
      </div>
      <div style="flex:1;">
        <label style="font-size:12px;color:var(--muted);display:block;margin-bottom:4px;">G</label>
        <input type="number" id="gVal" min="0" max="255" value="%G%"
          style="width:100%;padding:8px;background:var(--bg);border:1px solid var(--border);
          border-radius:8px;color:var(--text);font-size:14px;" oninput="onRgbInput()">
      </div>
      <div style="flex:1;">
        <label style="font-size:12px;color:var(--muted);display:block;margin-bottom:4px;">B</label>
        <input type="number" id="bVal" min="0" max="255" value="%B%"
          style="width:100%;padding:8px;background:var(--bg);border:1px solid var(--border);
          border-radius:8px;color:var(--text);font-size:14px;" oninput="onRgbInput()">
      </div>
      <div style="flex:1;">
        <label style="font-size:12px;color:var(--muted);display:block;margin-bottom:4px;">W</label>
        <input type="number" id="wVal" min="0" max="255" value="%W%"
          style="width:100%;padding:8px;background:var(--bg);border:1px solid var(--border);
          border-radius:8px;color:var(--text);font-size:14px;" oninput="onRgbInput()">
      </div>
    </div>

    <!-- Helligkeit -->
    <div class="form-row">
      <label style="display:flex;justify-content:space-between;">
        <span>Helligkeit</span>
        <span id="brLabel">%BR_PCT%%</span>
      </label>
      <input type="range" id="brightness" min="1" max="255" value="%BR%"
        style="width:100%;accent-color:var(--green);margin-top:6px;"
        oninput="onBrInput()">
    </div>

    <!-- Schnellfarben -->
    <div style="margin-top:6px;margin-bottom:16px;">
      <div style="font-size:12px;color:var(--muted);margin-bottom:8px;">Schnellfarben</div>
      <div style="display:flex;gap:8px;flex-wrap:wrap;">
        <button onclick="setColor(255,197,143)" style="width:36px;height:36px;border-radius:8px;border:none;cursor:pointer;background:rgb(255,197,143);" title="Warm-Weiß 2700K"></button>
        <button onclick="setColor(255,255,220)" style="width:36px;height:36px;border-radius:8px;border:none;cursor:pointer;background:rgb(255,255,220);" title="Kalt-Weiß 5000K"></button>
        <button onclick="setColor(255,80,0)"    style="width:36px;height:36px;border-radius:8px;border:none;cursor:pointer;background:rgb(255,80,0);"   title="Orange"></button>
        <button onclick="setColor(255,0,0)"     style="width:36px;height:36px;border-radius:8px;border:none;cursor:pointer;background:rgb(255,0,0);"     title="Rot"></button>
        <button onclick="setColor(0,255,0)"     style="width:36px;height:36px;border-radius:8px;border:none;cursor:pointer;background:rgb(0,255,0);"     title="Grün"></button>
        <button onclick="setColor(0,80,255)"    style="width:36px;height:36px;border-radius:8px;border:none;cursor:pointer;background:rgb(0,80,255);"    title="Blau"></button>
      </div>
    </div>

    <button onclick="saveRgb()" style="width:100%;padding:12px;background:var(--green);
      color:#fff;border:none;border-radius:12px;font-size:15px;font-weight:600;cursor:pointer;">
      💾 Speichern
    </button>
    <div id="saveMsg" style="display:none;color:var(--green);font-size:13px;
      text-align:center;margin-top:10px;">✅ Gespeichert – aktiv beim nächsten Locklicht</div>

    <div style="margin-top:14px;padding:12px;background:var(--bg);border-radius:10px;
      font-size:12px;color:var(--muted);line-height:1.7;">
      💡 Die Farbe gilt für das <strong>Locklicht</strong> (vor Öffnung/Schließung).<br>
      Der Rot-Testmodus bleibt immer rot.
    </div>
  </div>
</div>
<style>
.form-row{margin-bottom:14px;}
.form-row label{display:block;font-size:13px;color:var(--muted);margin-bottom:6px;}
.form-row input[type=text],.form-row input[type=number]{
  width:100%;padding:10px 12px;background:var(--bg);border:1px solid var(--border);
  border-radius:10px;color:var(--text);font-size:14px;}
</style>
<script>
function toHex(v){return v.toString(16).padStart(2,'0');}
function fromHex(h){return parseInt(h,16);}
function clamp(v){return Math.max(0,Math.min(255,parseInt(v)||0));}

function updatePreview(){
  const r=clamp(document.getElementById('rVal').value);
  const g=clamp(document.getElementById('gVal').value);
  const b=clamp(document.getElementById('bVal').value);
  document.getElementById('preview').style.background=`rgb(${r},${g},${b})`;
  document.getElementById('colorPicker').value='#'+toHex(r)+toHex(g)+toHex(b);
}
function onColorPick(){
  const hex=document.getElementById('colorPicker').value;
  document.getElementById('rVal').value=fromHex(hex.slice(1,3));
  document.getElementById('gVal').value=fromHex(hex.slice(3,5));
  document.getElementById('bVal').value=fromHex(hex.slice(5,7));
  updatePreview();
}
function onRgbInput(){updatePreview();}
function onBrInput(){
  const v=document.getElementById('brightness').value;
  document.getElementById('brLabel').textContent=Math.round(v/255*100)+'%';
}
function setColor(r,g,b,w=0){
  document.getElementById('rVal').value=r;
  document.getElementById('gVal').value=g;
  document.getElementById('bVal').value=b;
  document.getElementById('wVal').value=w;
  updatePreview();
}
function saveRgb(){
  const r=clamp(document.getElementById('rVal').value);
  const g=clamp(document.getElementById('gVal').value);
  const b=clamp(document.getElementById('bVal').value);
  const br=document.getElementById('brightness').value;
  const w=clamp(document.getElementById('wVal').value);
  fetch('/save-rgb',{method:'POST',body:new URLSearchParams({r,g,b,w,br})})
    .then(()=>{
      const m=document.getElementById('saveMsg');
      m.style.display='block';
      setTimeout(()=>m.style.display='none',3000);
    });
}
// Helligkeit Label initial setzen
document.getElementById('brLabel').textContent=
  Math.round(document.getElementById('brightness').value/255*100)+'%';
</script>
)rawliteral";

    html += renderFooter();

    // Hex-Farbe berechnen
    char hex[8];
    snprintf(hex, sizeof(hex), "#%02x%02x%02x", rgbColorR, rgbColorG, rgbColorB);

    html.replace("%R%",       String(rgbColorR));
    html.replace("%W%",       String(rgbColorW));
    html.replace("%G%",       String(rgbColorG));
    html.replace("%B%",       String(rgbColorB));
    html.replace("%BR%",      String(rgbBrightness));
    html.replace("%BR_PCT%",  String((int)(rgbBrightness / 2.55f)));
    html.replace("%HEX_COLOR%", String(hex));
    html.replace("%PREV_R%",  String(rgbColorR));
    html.replace("%PREV_G%",  String(rgbColorG));
    html.replace("%PREV_B%",  String(rgbColorB));

    server.send(200, "text/html; charset=UTF-8", html);
}
