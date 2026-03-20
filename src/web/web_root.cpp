#include "web.h"
#include "../storage.h"
#include "../door.h"
#include "../motor.h"
#include "../light.h"
#include "../lux.h"
#include "../system.h"
#include "../logger.h"
#include "../bme.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <RTClib.h>

extern const char   *FW_VERSION;

// ==================================================
// STARTSEITE
// ==================================================
void handleRoot()
{
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="de" data-theme="%THEME%">
<head>
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="default">
<link rel="apple-touch-icon" href="/icon-192.png">
<link rel="manifest" href="/manifest.json">
<meta name="theme-color" content="#4CAF50">
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<title>Hühnerklappe</title>
<style>
:root { --bg:#f4f6f9; --card:#ffffff; --text:#1f2937; --muted:#6b7280; --green:#22c55e; --red:#ef4444; --orange:#f59e0b; --accent:#16a34a; --radius:18px; }
@media (prefers-color-scheme: dark) { :root[data-theme="auto"] { --bg:#0f172a; --card:#1e293b; --text:#f1f5f9; --muted:#94a3b8; } }
:root[data-theme="dark"]  { --bg:#0f172a; --card:#1e293b; --text:#f1f5f9; --muted:#94a3b8; }
:root[data-theme="light"] { --bg:#f4f6f9; --card:#ffffff; --text:#1f2937; --muted:#6b7280; }
body { margin:0; font-family:system-ui,-apple-system,BlinkMacSystemFont,sans-serif; background:var(--bg); color:var(--text); -webkit-font-smoothing:antialiased; }
.header { padding:28px 20px 20px; text-align:center; font-size:20px; font-weight:700; }
.datetime { font-size:15px; color:var(--muted); margin-top:6px; }
.weather  { font-size:14px; color:var(--muted); margin-top:4px; letter-spacing:0.3px; }
.container { max-width:430px; margin:auto; padding:10px 16px 110px; }
.card { background:var(--card); border-radius:var(--radius); padding:18px; margin-bottom:16px; box-shadow:0 6px 18px rgba(0,0,0,0.06),0 2px 6px rgba(0,0,0,0.04); }
.card-title { font-size:16px; font-weight:600; margin-bottom:14px; }
.badge { padding:6px 16px; border-radius:999px; font-weight:600; font-size:14px; }
.open   { background:rgba(34,197,94,0.15); color:var(--green); }
.closed { background:rgba(239,68,68,0.15);  color:var(--red); }
.status-row { display:flex; justify-content:space-between; margin-bottom:10px; font-size:15px; }
.status-row .label { color:var(--muted); }
button { border:none; border-radius:14px; padding:14px; font-size:17px; font-weight:600; cursor:pointer; }
.btn-open  { width:100%; background:var(--green);  color:white; }
.btn-close { width:100%; background:var(--red);    color:white; }
.btn-stop  { width:100%; background:var(--orange); color:white; }
.light-row { display:flex; justify-content:space-between; gap:12px; }
.light-btn { flex:1; height:56px; border-radius:16px; font-size:20px; background:var(--card); box-shadow:inset 0 1px 3px rgba(0,0,0,0.05),0 4px 12px rgba(0,0,0,0.08); }
.light-btn.on { background:var(--green); color:white; box-shadow:0 0 10px rgba(34,197,94,0.6); }
.next-section { font-size:14px; color:var(--muted); line-height:1.4; }
.sys-ok, .sys-error { width:30px; height:30px; border-radius:50%; display:inline-flex; align-items:center; justify-content:center; font-size:16px; }
.sys-ok    { background:rgba(34,197,94,0.15); color:var(--green); }
.sys-error { background:rgba(239,68,68,0.15); color:var(--red); }
nav { position:fixed; bottom:18px; left:50%; transform:translateX(-50%); width:100%; max-width:430px; background:var(--card); border-radius:22px; box-shadow:0 10px 25px rgba(0,0,0,0.15); display:flex; padding:6px 0; }
nav a { flex:1; text-align:center; text-decoration:none; color:var(--muted); font-size:13px; font-weight:600; }
.card-header { display:flex; align-items:center; justify-content:space-between; margin-bottom:10px; }
.sys-link { text-decoration:none; }
</style>
<script>
let inflight = false;
let last = { date:"", time:"", door:"", automatik:"", next:"", light:"", lightState:"" };
async function debounceAction(btn,fn){ if(!btn)return; btn.disabled=true; try{await fn();}finally{setTimeout(()=>btn.disabled=false,250);} }
async function toggleLockLight(){ const b=document.getElementById('lockLightBtn'); await debounceAction(b,async()=>{await fetch('/light',{cache:'no-store'});await update();}); }
async function toggleStallLight(){ const b=document.getElementById('stallLightBtn'); await debounceAction(b,async()=>{await fetch('/stalllight',{cache:'no-store'});await update();}); }
async function toggleDoor(){ const b=document.getElementById('doorBtn'); await debounceAction(b,async()=>{await fetch('/door',{cache:'no-store'});await update();}); }
async function toggleRGB(){ const b=document.getElementById('rgbBtn'); await debounceAction(b,async()=>{await fetch('/rgbred',{cache:'no-store'});await update();}); }
function setDoorButton(label,cls){ const b=document.getElementById('doorBtn'); if(!b)return; b.textContent=label; b.classList.remove('btn-open','btn-close','btn-stop'); b.classList.add(cls); }
document.addEventListener('visibilitychange',()=>{if(document.visibilityState==='visible')update();});
document.addEventListener('DOMContentLoaded',()=>{update();setInterval(update,3000);});
</script>
</head>
<body>
<div class="header"><h3>🐔 Hühnerklappe</h3><div class="datetime"><span id="date">---</span> · <span id="time">--:--</span></div><div id="weather" class="weather" style="display:none;"></div></div>
<div class="container">
  <div class="card">
    <div class="card-header">
      <h3 class="card-title">Türstatus</h3>
      <a href="/systemtest" id="sysStatusLink" class="sys-link"><span id="sysStatus" class="sys-ok">✔</span></a>
    </div>
    <div class="status-row"><span class="label">Stallklappe:</span>   <span id="door"       class="badge closed">---</span></div>
    <div class="status-row"><span class="label">Locklicht:</span>      <span id="lightState" class="badge closed">---</span></div>
    <div class="status-row"><span class="label">Stalllicht:</span>     <span id="stallLight" class="badge closed">---</span></div>
    <div class="status-row"><span class="label">RGB Test:</span>    <span id="rgbred"  class="badge closed">---</span></div>
    <div class="status-row"><span class="label">Betriebsart:</span>    <span id="automatik">---</span></div>
    <div class="status-row"><span class="label">Helligkeit:</span>     <span id="light">---</span></div>
    <div class="next-section"><div class="next-title">Nächste Aktion:</div><div id="next" class="next-value-full">---</div></div>
  </div>
  <div class="card">
    <div class="light-row">
      <button type="button" id="lockLightBtn"  class="light-btn" onclick="toggleLockLight()"  title="Locklicht">🔆</button>
      <button type="button" id="stallLightBtn" class="light-btn" onclick="toggleStallLight()" title="Stalllicht">💡</button>
      <button type="button" id="rgbBtn"     class="light-btn" onclick="toggleRGB()"     title="RGB Test">🔴</button>
    </div>
    <hr>
    <button type="button" id="doorBtn" class="btn-open" onclick="toggleDoor()">Öffnen</button>
  </div>
</div>
<nav>
  <a href="/">🏠<br>Start</a>
  <a href="/settings">⚙️<br>Einstellungen</a>
  <a href="/advanced">🔧<br>Erweitert</a>
</nav>
<script>
async function update(){
  if(inflight)return; inflight=true;
  try{
    const r=await fetch('/status',{cache:'no-store'}); const d=await r.json();
    if(d.date!==last.date){document.getElementById('date').innerText=d.date;last.date=d.date;}
    if(d.time!==last.time){document.getElementById('time').innerText=d.time;last.time=d.time;}
    const wEl=document.getElementById('weather');
    if(wEl){if(d.bmeOk){wEl.textContent='🌡 '+d.bmeTemp+'°C  💧'+d.bmeHumidity+'%  🌬 '+d.bmePressure+' hPa';wEl.style.display='block';}else{wEl.style.display='none';}}
    if(d.door!==last.door){const el=document.getElementById('door');el.innerText=d.door;el.className="badge "+(d.door==="Offen"?"open":"closed");last.door=d.door;}
    const moving=(d.moving==="1"),isOpen=(d.door==="Offen");
    if(moving)      setDoorButton("Stopp","btn-stop");
    else if(isOpen) setDoorButton("Schließen","btn-close");
    else            setDoorButton("Öffnen","btn-open");
    if(d.automatik!==last.automatik){document.getElementById('automatik').innerText=d.automatik;last.automatik=d.automatik;}
    if(d.next!==last.next){document.getElementById('next').innerText=d.next;last.next=d.next;}
    if(d.light!==last.light){document.getElementById('light').innerText=d.light;last.light=d.light;}
    const sys=document.getElementById("sysStatus");
    if(d.sysError==="1"){sys.innerHTML="✖";sys.className="sys-error";}else{sys.innerHTML="✔";sys.className="sys-ok";}
    if(d.lightState!==last.lightState){const ls=document.getElementById('lightState');ls.innerText=d.lightState;ls.className=(d.lightState==="An")?"badge open":"badge closed";last.lightState=d.lightState;}
    const stall=document.getElementById("stallLight"); if(stall&&d.stallLight){stall.innerText=d.stallLight;stall.className="badge "+(d.stallLight==="An"?"open":"closed");}
    const ws=document.getElementById("rgbred");     if(ws&&d.rgbred){ws.innerText=d.rgbred;ws.className="badge "+(d.rgbred==="An"?"open":"closed");}
    const lockBtn=document.getElementById("lockLightBtn"); const stallBtn=document.getElementById("stallLightBtn"); const wsBtn=document.getElementById("rgbBtn");
    if(lockBtn)  lockBtn.classList.toggle("on",  d.lightState==="An");
    if(stallBtn) stallBtn.classList.toggle("on", d.stallLight==="An");
    if(wsBtn)    wsBtn.classList.toggle("on",    d.rgbred==="An");
  }catch(e){}finally{inflight=false;}
}
</script>
</body></html>
)rawliteral";

    html.replace("%THEME%", uiTheme);
    server.send(200, "text/html; charset=UTF-8", html);
}

// ==================================================
// STATUS API
// ==================================================
void handleStatus()
{
    updateSystemHealth();
    DateTime nowDT  = rtcOk ? rtc.now() : DateTime(2000, 1, 1, 0, 0, 0);
    String dateStr  = rtcOk ? (String(nowDT.day()) + "." + String(nowDT.month()) + "." + String(nowDT.year())) : "-";

    String next;
    if (!doorOpen)
        next = (openMode == "time") ? "Öffnet um " + openTime
                                    : "Öffnet automatisch bei zunehmendem Licht (≈ " + String(openLightThreshold) + " lx)";
    else
        next = (closeMode == "time") ? "Schließt um " + closeTime
                                     : "Schließt automatisch bei abnehmendem Licht (≈ " + String(closeLightThreshold) + " lx)";

    String automatik;
    if      (openMode == "light" && closeMode == "light") automatik = "🌞 Lichtautomatik aktiv";
    else if (openMode == "time"  && closeMode == "time")  automatik = "⏰ Zeitautomatik aktiv";
    else                                                   automatik = "🔁 Mischbetrieb";

    JsonDocument doc;
    doc["time"]          = rtcOk ? nowDT.timestamp(DateTime::TIMESTAMP_TIME) : "--:--:--";
    doc["date"]          = dateStr;
    doc["door"]          = doorOpen ? "Offen" : "Geschlossen";
    doc["moving"]        = (motorState != MOTOR_STOPPED) ? "1" : "0";
    doc["doorBool"]      = doorOpen ? "1" : "0";
    doc["automatik"]     = automatik;
    doc["next"]          = next;
    doc["light"]         = (isfinite(lux) && lux >= 0.0f) ? String(lux, 1) + " lx" : "n/a";
    doc["lightFallback"] = (!lightAutomationAvailable && (openMode == "light" || closeMode == "light")) ? "1" : "0";
    doc["lightState"]    = lightActive ? "An" : "Aus";
    doc["stallLight"]    = stallLightActive ? "An" : "Aus";
    doc["manualLight"]   = manualLightActive ? "1" : "0";
    doc["learning"]      = learningActive ? "ja" : "nein";
    doc["fw"]            = FW_VERSION;
    doc["sysError"]      = systemError() ? "1" : "0";
    doc["rgbred"]     = rgbRedActive ? "An" : "Aus";
    doc["bmeOk"]      = bmeOk;
    if (bmeOk) {
        doc["bmeTemp"]     = String(bmeTemp,     1);
        doc["bmeHumidity"] = String(bmeHumidity, 1);
        doc["bmePressure"] = String(bmePressure, 1);
    }

    String out; serializeJson(doc, out);
    server.send(200, "application/json", out);
}
