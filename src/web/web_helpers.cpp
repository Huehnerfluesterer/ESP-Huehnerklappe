#include "web.h"
#include "../storage.h"

// ==================================================
// GEMEINSAME HTML-FRAGMENTE
// ==================================================

String renderThemeHead(const String &title)
{
    return String(R"rawliteral(<!DOCTYPE html>
<html lang="de" data-theme=")rawliteral") + uiTheme + R"rawliteral(">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>)rawliteral" + title + R"rawliteral(</title>
<style>
:root { --bg:#f4f6f9; --card:#ffffff; --text:#1f2937; --muted:#6b7280; --green:#22c55e; --red:#ef4444; --orange:#f59e0b; --radius:18px; }
@media (prefers-color-scheme: dark) { :root[data-theme="auto"] { --bg:#0f172a; --card:#1e293b; --text:#f1f5f9; --muted:#94a3b8; } }
:root[data-theme="dark"]  { --bg:#0f172a; --card:#1e293b; --text:#f1f5f9; --muted:#94a3b8; }
:root[data-theme="light"] { --bg:#f4f6f9; --card:#ffffff; --text:#1f2937; --muted:#6b7280; }
body { margin:0; font-family:system-ui,-apple-system; background:var(--bg); color:var(--text); }
.header { text-align:center; padding:26px 0 12px; }
.header h1,.header h2,.header h3 { margin:0; font-weight:700; }
.container { max-width:430px; margin:auto; padding:20px 16px 110px; }
.card { background:var(--card); padding:18px; border-radius:var(--radius); margin-bottom:16px; box-shadow:0 6px 18px rgba(0,0,0,0.06); }
button { width:100%; padding:14px; border:none; border-radius:14px; font-size:16px; font-weight:600; background:var(--green); color:white; margin-top:12px; cursor:pointer; }
button.danger { background:var(--red); }
nav { position:fixed; bottom:18px; left:50%; transform:translateX(-50%); width:100%; max-width:430px; background:var(--card); border-radius:22px; box-shadow:0 10px 25px rgba(0,0,0,0.15); display:flex; padding:6px 0; }
nav a { flex:1; text-align:center; text-decoration:none; color:var(--muted); font-size:13px; font-weight:600; }
</style>
<script>
function toggleTheme(){
  let c=document.documentElement.getAttribute("data-theme");
  let n=c==="auto"?"dark":c==="dark"?"light":"auto";
  fetch("/set-theme",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"theme="+n})
    .then(()=>document.documentElement.setAttribute("data-theme",n));
}
</script>
</head>
<body>
<div class="container">
)rawliteral";
}

String renderFooter()
{
    return R"rawliteral(
</div>
<nav>
  <a href="/">🏠<br>Start</a>
  <a href="/settings">⚙️<br>Einstellungen</a>
  <a href="/advanced">🔧<br>Erweitert</a>
</nav>
</body>
</html>
)rawliteral";
}

// timeToMinutes() ist in logic.cpp definiert und über web.h deklariert
