// ==================================================
// Hühnerklappe – Relais ESP32-C3
//
// Features:
//   • ESP-NOW: empfängt Befehle von der Hühnerklappe
//   • Web-UI:  manuelles Schalten + Status (Port 80)
//   • MQTT:    Status publizieren + Befehle empfangen
//   • Heartbeat: alle 30s an Klappe
//
// SETUP:
//   1. WIFI_SSID / WIFI_PASSWORD eintragen
//   2. MQTT optional über Web-UI konfigurieren
//   3. Board: ESP32C3 Dev Module
//      USB CDC on Boot: Enabled
//   4. Flashen → seriellen Monitor → MAC ablesen
//   5. In Klappe: Erweitert → ESP-NOW → MAC eintragen
// ==================================================

#include <WiFi.h>
#include <WebServer.h>
#include <esp_now.h>
#include <PubSubClient.h>
#include <Preferences.h>

// --------------------------------------------------
// KONFIGURATION
// --------------------------------------------------
#define WIFI_SSID        ""
#define WIFI_PASSWORD    ""

#define RELAY_PIN         10
#define RELAY_ACTIVE_LOW  true

#define MQTT_TOPIC_STATE  "huehner/relais/state"
#define MQTT_TOPIC_CMD    "huehner/relais/cmd"
#define HEARTBEAT_MS      30000UL

// --------------------------------------------------
// Paketstruktur (identisch mit relay.h der Klappe)
// --------------------------------------------------
struct RelayPacket { uint32_t magic; uint8_t command; };
#define RELAY_MAGIC     0xC10534u
#define CMD_ON          1
#define CMD_OFF         0
#define CMD_HEARTBEAT   2

// --------------------------------------------------
// Globale Variablen
// --------------------------------------------------
static WebServer    server(80);
static WiFiClient   wifiClient;
static PubSubClient mqtt(wifiClient);
static Preferences  prefs;

static uint8_t  BCAST[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static bool     relayOn   = false;
static String   lastSrc   = "–";
static unsigned long lastHB   = 0;
static unsigned long mqttRetry = 0;

static String mqttHost = "";
static int    mqttPort = 1883;
static String mqttUser = "";
static String mqttPass = "";

// --------------------------------------------------
// Relais schalten
// --------------------------------------------------
void setRelay(bool on, const String &src)
{
    relayOn = on; lastSrc = src;
    digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? (on?LOW:HIGH) : (on?HIGH:LOW));
    Serial.printf("[Relais] %s via %s\n", on?"AN":"AUS", src.c_str());
    if (mqtt.connected()) mqtt.publish(MQTT_TOPIC_STATE, on?"ON":"OFF", true);
}

// --------------------------------------------------
// Heartbeat
// --------------------------------------------------
void sendHeartbeat()
{
    RelayPacket p{RELAY_MAGIC, CMD_HEARTBEAT};
    esp_now_send(BCAST, (uint8_t*)&p, sizeof(p));
}

// --------------------------------------------------
// ESP-NOW Empfang
// --------------------------------------------------
void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (len != sizeof(RelayPacket)) return;
    RelayPacket p; memcpy(&p, data, sizeof(p));
    if (p.magic != RELAY_MAGIC) return;
    if      (p.command == CMD_ON)  setRelay(true,  "Klappe");
    else if (p.command == CMD_OFF) setRelay(false, "Klappe");
}

// --------------------------------------------------
// MQTT
// --------------------------------------------------
void mqttCallback(char *topic, byte *payload, unsigned int len)
{
    String msg; for (unsigned int i=0;i<len;i++) msg+=(char)payload[i];
    msg.trim(); msg.toUpperCase();
    if (msg=="ON"||msg=="1")  setRelay(true,  "MQTT");
    if (msg=="OFF"||msg=="0") setRelay(false, "MQTT");
}

void mqttConnect()
{
    if (mqttHost.isEmpty()||mqtt.connected()) return;
    if (millis()-mqttRetry < 10000) return;
    mqttRetry = millis();
    mqtt.setServer(mqttHost.c_str(), mqttPort);
    mqtt.setCallback(mqttCallback);
    String id = "relais-" + WiFi.macAddress(); id.replace(":","");
    if (mqtt.connect(id.c_str(), mqttUser.c_str(), mqttPass.c_str())) {
        Serial.println("[MQTT] Verbunden");
        mqtt.subscribe(MQTT_TOPIC_CMD);
        mqtt.publish(MQTT_TOPIC_STATE, relayOn?"ON":"OFF", true);
    }
}

// --------------------------------------------------
// Web-UI HTML
// --------------------------------------------------
static const char HTML[] PROGMEM = R"====(
<!DOCTYPE html><html lang="de"><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Relais</title>
<style>
:root{--bg:#0f1117;--card:#1a1d27;--border:#2a2d3a;--text:#e8eaf0;--muted:#6b7280;
  --green:#22c55e;--red:#ef4444;--orange:#f59e0b;--blue:#3b82f6;}
*{box-sizing:border-box;margin:0;padding:0;}
body{background:var(--bg);color:var(--text);font-family:'Segoe UI',system-ui,sans-serif;
  padding:20px 16px 40px;min-height:100vh;}
.wrap{max-width:400px;margin:0 auto;}
h1{text-align:center;font-size:22px;font-weight:700;padding:20px 0 6px;}
.sub{text-align:center;color:var(--muted);font-size:13px;margin-bottom:20px;}
.card{background:var(--card);border:1px solid var(--border);border-radius:16px;
  padding:20px;margin-bottom:14px;}
.ct{font-size:11px;font-weight:700;text-transform:uppercase;letter-spacing:.8px;
  color:var(--muted);margin-bottom:14px;}
.big-btn{width:100%;padding:22px;border-radius:14px;border:none;font-size:17px;
  font-weight:700;cursor:pointer;transition:.2s;display:flex;align-items:center;
  justify-content:center;gap:10px;}
.big-btn.on{background:rgba(34,197,94,.15);color:var(--green);border:2px solid rgba(34,197,94,.3);}
.big-btn.off{background:rgba(239,68,68,.1);color:var(--red);border:2px solid rgba(239,68,68,.2);}
.big-btn:active{transform:scale(.96);}
.dot{width:11px;height:11px;border-radius:50%;flex-shrink:0;}
.big-btn.on .dot{background:var(--green);box-shadow:0 0 8px var(--green);}
.big-btn.off .dot{background:var(--red);}
.row{display:flex;gap:10px;margin-top:12px;}
.btn{flex:1;padding:11px;border-radius:11px;border:1px solid var(--border);
  background:transparent;color:var(--text);font-size:14px;font-weight:600;cursor:pointer;transition:.15s;}
.btn.on{border-color:rgba(34,197,94,.4);color:var(--green);}
.btn.off{border-color:rgba(239,68,68,.3);color:var(--red);}
.btn:hover{background:var(--border);}
.sr{display:flex;justify-content:space-between;align-items:center;
  padding:8px 0;border-bottom:1px solid var(--border);font-size:14px;}
.sr:last-child{border-bottom:none;}
.sr .lbl{color:var(--muted);}
.badge{padding:3px 9px;border-radius:999px;font-size:12px;font-weight:600;}
.g{background:rgba(34,197,94,.15);color:var(--green);}
.r{background:rgba(239,68,68,.12);color:var(--red);}
.o{background:rgba(245,158,11,.15);color:var(--orange);}
.mono{font-family:monospace;font-size:13px;}
.fi{margin-bottom:12px;}
.fi label{display:block;font-size:12px;color:var(--muted);margin-bottom:5px;}
.fi input{width:100%;padding:10px 12px;background:var(--bg);border:1px solid var(--border);
  border-radius:10px;color:var(--text);font-size:14px;}
.fi input:focus{outline:none;border-color:var(--blue);}
.sv{width:100%;padding:12px;background:var(--blue);color:#fff;border:none;
  border-radius:11px;font-size:14px;font-weight:600;cursor:pointer;margin-top:4px;}
.sv:hover{opacity:.85;}
.ok{display:none;color:var(--green);font-size:13px;text-align:center;margin-top:8px;}
</style></head><body>
<div class="wrap">
<h1>🔌 Relais Steuerung</h1>
<p class="sub">Hühnerklappe – ESP32-C3</p>

<div class="card">
<div class="ct">Relais</div>
<button class="big-btn" id="mb" onclick="toggle()">
  <span class="dot"></span><span id="mt">–</span>
</button>
<div class="row">
  <button class="btn on"  onclick="set(1)">AN</button>
  <button class="btn off" onclick="set(0)">AUS</button>
</div>
</div>

<div class="card">
<div class="ct">Status</div>
<div class="sr"><span class="lbl">Relais</span><span id="st" class="badge">–</span></div>
<div class="sr"><span class="lbl">Quelle</span><span id="src">–</span></div>
<div class="sr"><span class="lbl">WLAN</span><span id="wf" class="badge">–</span></div>
<div class="sr"><span class="lbl">IP</span><span id="ip" class="mono">–</span></div>
<div class="sr"><span class="lbl">MQTT</span><span id="mq" class="badge">–</span></div>
<div class="sr"><span class="lbl">MAC</span><span id="mac" class="mono">–</span></div>
<div class="sr"><span class="lbl">Uptime</span><span id="up">–</span></div>
</div>

<div class="card">
<div class="ct">MQTT Einstellungen</div>
<div class="fi"><label>Broker (Host/IP)</label><input id="mh" placeholder="192.168.1.100"></div>
<div class="fi"><label>Port</label><input id="mp" type="number" value="1883"></div>
<div class="fi"><label>Benutzername</label><input id="mu"></div>
<div class="fi"><label>Passwort</label><input id="mw" type="password"></div>
<button class="sv" onclick="saveMqtt()">Speichern</button>
<div class="ok" id="ok">✅ Gespeichert</div>
</div>
</div>

<script>
function upd(d){
  const on=d.relay;
  document.getElementById('mb').className='big-btn '+(on?'on':'off');
  document.getElementById('mt').textContent=on?'AN – Tippen zum Ausschalten':'AUS – Tippen zum Einschalten';
  let s=document.getElementById('st');
  s.textContent=on?'AN':'AUS'; s.className='badge '+(on?'g':'r');
  document.getElementById('src').textContent=d.source||'–';
  document.getElementById('ip').textContent=d.ip||'–';
  document.getElementById('mac').textContent=d.mac||'–';
  let w=document.getElementById('wf');
  w.textContent=d.wifi?'Verbunden':'Getrennt'; w.className='badge '+(d.wifi?'g':'r');
  let m=document.getElementById('mq');
  m.textContent=d.mqtt?'Verbunden':(d.mqttHost?'Getrennt':'Deaktiviert');
  m.className='badge '+(d.mqtt?'g':(d.mqttHost?'o':''));
  let u=Math.floor(d.uptime/1000),h=Math.floor(u/3600);u%=3600;
  let mn=Math.floor(u/60);u%=60;
  document.getElementById('up').textContent=(h>0?h+'h ':'')+( mn>0?mn+'min ':'')+u+'s';
  if(!document.getElementById('mh').value) document.getElementById('mh').value=d.mqttHost||'';
  if(document.getElementById('mp').value==='1883') document.getElementById('mp').value=d.mqttPort||1883;
  if(!document.getElementById('mu').value) document.getElementById('mu').value=d.mqttUser||'';
}
function fetch2(url,opt){return fetch(url,{cache:'no-store',...opt});}
function poll(){fetch2('/status').then(r=>r.json()).then(upd).catch(()=>{});}
function set(v){fetch2('/relay?state='+v,{method:'POST'}).then(poll);}
function toggle(){fetch2('/relay-toggle',{method:'POST'}).then(poll);}
function saveMqtt(){
  fetch2('/save-mqtt',{method:'POST',body:new URLSearchParams({
    host:document.getElementById('mh').value,
    port:document.getElementById('mp').value,
    user:document.getElementById('mu').value,
    pass:document.getElementById('mw').value
  })}).then(()=>{
    let o=document.getElementById('ok');o.style.display='block';
    setTimeout(()=>o.style.display='none',2500);poll();
  });
}
poll(); setInterval(poll,3000);
</script></body></html>
)====";

// --------------------------------------------------
// Web-Routen
// --------------------------------------------------
void handleRoot()      { server.send_P(200,"text/html; charset=UTF-8",HTML); }

void handleStatus()
{
    String j="{";
    j+="\"relay\":"+(String)(relayOn?"true":"false")+",";
    j+="\"source\":\""+lastSrc+"\",";
    j+="\"wifi\":"+(String)(WiFi.status()==WL_CONNECTED?"true":"false")+",";
    j+="\"ip\":\""+WiFi.localIP().toString()+"\",";
    j+="\"mac\":\""+WiFi.macAddress()+"\",";
    j+="\"mqtt\":"+(String)(mqtt.connected()?"true":"false")+",";
    j+="\"mqttHost\":\""+mqttHost+"\",";
    j+="\"mqttPort\":"+String(mqttPort)+",";
    j+="\"mqttUser\":\""+mqttUser+"\",";
    j+="\"uptime\":"+String(millis());
    j+="}";
    server.send(200,"application/json",j);
}

void handleSet()    { String s=server.arg("state"); if(s=="1")setRelay(true,"Web"); else if(s=="0")setRelay(false,"Web"); server.send(200,"text/plain","OK"); }
void handleToggle() { setRelay(!relayOn,"Web"); server.send(200,"text/plain","OK"); }

void handleSaveMqtt()
{
    mqttHost=server.arg("host");
    mqttPort=server.arg("port").toInt(); if(!mqttPort)mqttPort=1883;
    mqttUser=server.arg("user");
    mqttPass=server.arg("pass");
    prefs.begin("relay",false);
    prefs.putString("mh",mqttHost); prefs.putInt("mp",mqttPort);
    prefs.putString("mu",mqttUser); prefs.putString("mw",mqttPass);
    prefs.end();
    if(mqtt.connected())mqtt.disconnect(); mqttRetry=0;
    server.send(200,"text/plain","OK");
}

// --------------------------------------------------
// SETUP
// --------------------------------------------------
void setup()
{
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n🔌 Relais ESP32-C3");
    Serial.println("==================");

    pinMode(RELAY_PIN, OUTPUT);
    setRelay(false, "Boot");
    Serial.printf("Relais-Pin: GPIO%d\n", RELAY_PIN);

    prefs.begin("relay",true);
    mqttHost=prefs.getString("mh",""); mqttPort=prefs.getInt("mp",1883);
    mqttUser=prefs.getString("mu",""); mqttPass=prefs.getString("mw","");
    prefs.end();

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.printf("WLAN: %s ...\n", WIFI_SSID);
    unsigned long t0=millis();
    while(WiFi.status()!=WL_CONNECTED && millis()-t0<10000){delay(250);Serial.print(".");}
    if(WiFi.status()==WL_CONNECTED)
        Serial.printf("\nIP: %s  Kanal: %d\n",WiFi.localIP().toString().c_str(),WiFi.channel());
    else
        Serial.println("\nWLAN Fehler");
    Serial.printf("MAC: %s\n",WiFi.macAddress().c_str());
    Serial.println("→ MAC in Klappe unter Erweitert → ESP-NOW eintragen!");

    server.on("/",            HTTP_GET,  handleRoot);
    server.on("/status",      HTTP_GET,  handleStatus);
    server.on("/relay",       HTTP_POST, handleSet);
    server.on("/relay-toggle",HTTP_POST, handleToggle);
    server.on("/save-mqtt",   HTTP_POST, handleSaveMqtt);
    server.begin();
    Serial.printf("Web-UI: http://%s\n", WiFi.localIP().toString().c_str());

    if(esp_now_init()!=ESP_OK){Serial.println("ESP-NOW Fehler");delay(3000);ESP.restart();}
    esp_now_register_recv_cb(onReceive);
    esp_now_peer_info_t peer={};
    memcpy(peer.peer_addr,BCAST,6); peer.channel=0; peer.encrypt=false;
    esp_now_add_peer(&peer);
    Serial.println("ESP-NOW bereit\n");

    sendHeartbeat();
}

// --------------------------------------------------
// LOOP
// --------------------------------------------------
void loop()
{
    server.handleClient();
    mqtt.loop();
    if(!mqttHost.isEmpty()) mqttConnect();
    if(millis()-lastHB>=HEARTBEAT_MS){ lastHB=millis(); sendHeartbeat(); }
}
