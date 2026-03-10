#include "web.h"
#include "../storage.h"
#include "../motor.h"
#include "../door.h"
#include "../light.h"
#include "../lux.h"
#include "../system.h"
#include "../logger.h"
#include "../mqtt.h"
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
    <button onclick="location.href='/calibration'" class="btn-open">🎯 Kalibrierung</button>
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
</script>
)rawliteral";
    html += renderFooter();
    html.replace("%FW_VERSION%", FW_VERSION);
    html.replace("%RSSI%",       String(WiFi.RSSI()));
    html.replace("%FREE_HEAP%",  String(ESP.getFreeHeap() / 1024));
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
evaluateHeap();evaluateRSSI();evaluateMQTT();
fetch("/systemtest-status",{cache:"no-store"}).then(r=>r.json()).then(d=>{evaluateRTC(d.rtcOk===1||d.rtcOk===true,d.rtcStatus);setLuxStatus(!!d.bhOk,Number(d.lux));}).catch(()=>{});
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
    html.replace("%PORT%",             String(mqttSettings.port));
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
    startMotorOpen(20000);
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
