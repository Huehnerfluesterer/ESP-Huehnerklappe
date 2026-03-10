#include "web.h"
#include "../storage.h"
#include "../mqtt.h"
#include "../logger.h"
#include <ArduinoJson.h>

// ==================================================
// EINSTELLUNGEN
// ==================================================
void handleSettings()
{
    String html = renderThemeHead("Einstellungen");
    html += R"rawliteral(
<div class="header"><h3>⚙️ Einstellungen</h3></div>
<div class="container">
  <div class="card">
    <div class="card-title">Betriebsmodus</div>
    <div class="tabs">
      <button class="tab-btn active" onclick="showTab('open')">Öffnen</button>
      <button class="tab-btn"        onclick="showTab('close')">Schließen</button>
    </div>
    <div class="switch-row">
      <span>Endschalter verwenden</span>
      <label class="switch">
        <input type="checkbox" id="limitSwitches" %LIMIT_SWITCHES%>
        <span class="slider"></span>
      </label>
    </div><br>
  </div>

  <div class="card" id="openCard">
    <div class="card-title">Öffnungs-Einstellungen</div>
    <form id="openTab">
      <label>Modus</label>
      <select name="openMode" id="openMode" onchange="toggleOpen()">
        <option value="time">Zeit</option>
        <option value="light">Helligkeit</option>
      </select>
      <div id="openTimeField"><label>Öffnungszeit</label><input type="time" name="openTime"></div>
      <div id="openLightField" style="display:none;"><label>Licht-Schwelle (Lux)</label><input type="number" name="openLightThreshold"></div>
      <label>Licht vorher (Min)</label><input type="number" name="lampPreOpen">
      <label>Licht nachher (Min)</label><input type="number" name="lampPostOpen">
      <button type="submit" class="btn-open">Speichern</button>
    </form>
  </div>

  <div class="card" id="closeCard" style="display:none;">
    <div class="card-title">Schließ-Einstellungen</div>
    <form id="closeTab">
      <label>Modus</label>
      <select name="closeMode" id="closeMode" onchange="toggleClose()">
        <option value="time">Zeit</option>
        <option value="light">Helligkeit</option>
      </select>
      <div id="closeTimeField"><label>Schließzeit</label><input type="time" name="closeTime"></div>
      <div id="closeLightField" style="display:none;"><label>Licht-Schwelle (Lux)</label><input type="number" name="closeLightThreshold"></div>
      <label>Licht vorher (Min)</label><input type="number" name="lampPreClose">
      <label>Licht nachher (Min)</label><input type="number" name="lampPostClose">
      <button type="submit" class="btn-close">Speichern</button>
    </form>
  </div>

  <div id="saveMsg" class="card" style="display:none;text-align:center;font-weight:600;"></div>
</div>

<style>
.tabs{display:flex;gap:10px;margin-top:10px;}
.tab-btn{flex:1;padding:8px;border-radius:12px;border:none;font-weight:600;font-size:14px;background:var(--bg);color:var(--text);}
.tab-btn.active{background:var(--green);color:white;}
label{display:block;font-weight:600;margin-top:14px;margin-bottom:6px;font-size:14px;color:var(--muted);}
input,select{width:100%;padding:8px 10px;border-radius:10px;border:1px solid #e5e7eb;font-size:14px;background:var(--card);color:var(--text);height:38px;box-sizing:border-box;}
.switch-row{display:flex;justify-content:space-between;align-items:center;margin:14px 0;}
.switch{position:relative;display:inline-block;width:52px;height:28px;}
.switch input{opacity:0;width:0;height:0;}
.slider{position:absolute;cursor:pointer;inset:0;background-color:#2c3e50;border-radius:34px;transition:0.25s;}
.slider:before{position:absolute;content:"";height:22px;width:22px;left:3px;bottom:3px;background-color:white;border-radius:50%;transition:0.25s;box-shadow:0 2px 6px rgba(0,0,0,0.4);}
input:checked+.slider{background-color:#27ae60;}
input:checked+.slider:before{transform:translateX(24px);}
</style>
<script>
function showTab(w){
  document.getElementById('openCard').style.display  = w==='open'?'block':'none';
  document.getElementById('closeCard').style.display = w==='close'?'block':'none';
  const tabs=document.querySelectorAll('.tab-btn');
  tabs[0].classList.toggle('active',w==='open'); tabs[1].classList.toggle('active',w==='close');
}
function toggleOpen(){ const m=document.getElementById('openMode').value; document.getElementById('openTimeField').style.display=m==='time'?'block':'none'; document.getElementById('openLightField').style.display=m==='light'?'block':'none'; }
function toggleClose(){ const m=document.getElementById('closeMode').value; document.getElementById('closeTimeField').style.display=m==='time'?'block':'none'; document.getElementById('closeLightField').style.display=m==='light'?'block':'none'; }
document.getElementById('openTab').onsubmit=e=>{e.preventDefault();fetch('/save-open',{method:'POST',body:new FormData(e.target)}).then(()=>showMsg("Öffnen gespeichert",true));};
document.getElementById('closeTab').onsubmit=e=>{e.preventDefault();fetch('/save-close',{method:'POST',body:new FormData(e.target)}).then(()=>showMsg("Schließen gespeichert",true));};
function showMsg(text,ok){const m=document.getElementById("saveMsg");m.innerText=text;m.style.background=ok?"rgba(34,197,94,0.15)":"rgba(239,68,68,0.15)";m.style.color=ok?"var(--green)":"var(--red)";m.style.display="block";setTimeout(()=>m.style.display="none",1500);}
document.getElementById('openMode').value="%OPEN_MODE%";
document.querySelector('[name="openTime"]').value="%OPEN_TIME%";
document.querySelector('[name="openLightThreshold"]').value="%OPEN_LIGHT%";
document.querySelector('[name="lampPreOpen"]').value="%LAMP_PRE_OPEN%";
document.querySelector('[name="lampPostOpen"]').value="%LAMP_POST_OPEN%";
document.getElementById('closeMode').value="%CLOSE_MODE%";
document.querySelector('[name="closeTime"]').value="%CLOSE_TIME%";
document.querySelector('[name="closeLightThreshold"]').value="%CLOSE_LIGHT%";
document.querySelector('[name="lampPreClose"]').value="%LAMP_PRE_CLOSE%";
document.querySelector('[name="lampPostClose"]').value="%LAMP_POST_CLOSE%";
showTab('open'); toggleOpen(); toggleClose();
document.getElementById("limitSwitches").addEventListener("change",function(){
  fetch("/set-limit-switches",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"enabled="+(this.checked?1:0)}).then(()=>showMsg("Endschalter gespeichert",true));
});
</script>
)rawliteral";

    html += renderFooter();
    html.replace("%OPEN_MODE%",       openMode);
    html.replace("%OPEN_TIME%",       openTime);
    html.replace("%OPEN_LIGHT%",      String(openLightThreshold));
    html.replace("%LAMP_PRE_OPEN%",   String(lampPreOpen));
    html.replace("%LAMP_POST_OPEN%",  String(lampPostOpen));
    html.replace("%CLOSE_MODE%",      closeMode);
    html.replace("%CLOSE_TIME%",      closeTime);
    html.replace("%CLOSE_LIGHT%",     String(closeLightThreshold));
    html.replace("%LAMP_PRE_CLOSE%",  String(lampPreClose));
    html.replace("%LAMP_POST_CLOSE%", String(lampPostClose));
    html.replace("%LIMIT_SWITCHES%",  useLimitSwitches ? "checked" : "");
    server.send(200, "text/html; charset=UTF-8", html);
}

// ==================================================
// SAVE OPEN
// ==================================================
void handleSaveOpen()
{
    String err;
    JsonDocument doc;
    doc["openMode"]           = server.arg("openMode");
    doc["openTime"]           = server.arg("openTime");
    doc["openLightThreshold"] = server.arg("openLightThreshold").toInt();
    doc["lampPreOpen"]        = server.arg("lampPreOpen").toInt();
    doc["lampPostOpen"]       = server.arg("lampPostOpen").toInt();
    // keep existing close values
    doc["closeMode"]            = closeMode;
    doc["closeTime"]            = closeTime;
    doc["closeLightThreshold"]  = closeLightThreshold;
    doc["lampPreClose"]         = lampPreClose;
    doc["lampPostClose"]        = lampPostClose;

    if (applySettingsFromJson(doc, err))
    {
        addLog("Öffnungs-Einstellungen gespeichert");
        server.send(200, "text/plain", "OK");
    }
    else server.send(400, "text/plain; charset=UTF-8", err);
}

// ==================================================
// SAVE CLOSE
// ==================================================
void handleSaveClose()
{
    String err;
    JsonDocument doc;
    // keep existing open values
    doc["openMode"]             = openMode;
    doc["openTime"]             = openTime;
    doc["openLightThreshold"]   = openLightThreshold;
    doc["lampPreOpen"]          = lampPreOpen;
    doc["lampPostOpen"]         = lampPostOpen;
    doc["closeMode"]            = server.arg("closeMode");
    doc["closeTime"]            = server.arg("closeTime");
    doc["closeLightThreshold"]  = server.arg("closeLightThreshold").toInt();
    doc["lampPreClose"]         = server.arg("lampPreClose").toInt();
    doc["lampPostClose"]        = server.arg("lampPostClose").toInt();

    if (applySettingsFromJson(doc, err))
    {
        addLog("Schließ-Einstellungen gespeichert");
        server.send(200, "text/plain", "OK");
    }
    else server.send(400, "text/plain; charset=UTF-8", err);
}
