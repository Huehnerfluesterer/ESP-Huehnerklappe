MQTT Topic Struktur

Damit lässt sich dein System perfekt mit Home Assistant oder Node-RED integrieren.

Basis Topic
chickencoop/
Status Topics
Topic	Beschreibung
chickencoop/door/state	Türstatus
chickencoop/door/action	aktuelle Bewegung
chickencoop/light/state	Stalllicht
chickencoop/sensor/lux	aktueller Luxwert
chickencoop/sensor/status	Sensorstatus
Command Topics
Topic	Funktion
chickencoop/door/open	Tür öffnen
chickencoop/door/close	Tür schließen
chickencoop/light/on	Licht einschalten
chickencoop/light/off	Licht ausschalten
Beispiel MQTT Nachrichten
Tür öffnen
Topic:
chickencoop/door/open

Payload:
1
Tür schließen
Topic:
chickencoop/door/close

Payload:
1
Beispiel Status
Topic:
chickencoop/door/state

Payload:
closed
Home Assistant Beispiel
switch:
  - platform: mqtt
    name: "Chicken Coop Door"
    command_topic: "chickencoop/door/open"
    state_topic: "chickencoop/door/state"
