# 📡 MQTT Integration

The Smart Chicken Coop Door Controller supports MQTT for monitoring and remote control.

This allows easy integration with systems such as:

* Home Assistant
* Node-RED
* OpenHAB
* custom dashboards

---

# 📂 Base Topic

All MQTT topics use the following base topic:

```
chickencoop/
```

Example:

```
chickencoop/door/state
```

---

# 📊 MQTT Architecture

```mermaid
flowchart LR
Controller[ESP32 Coop Controller]

Controller -->|Publish| StatusTopics[Status Topics]
Controller -->|Subscribe| CommandTopics[Command Topics]

StatusTopics --> HomeAssistant[Home Assistant]
StatusTopics --> NodeRed[Node-RED]

HomeAssistant --> CommandTopics
NodeRed --> CommandTopics
```

---

# 📥 Status Topics (Published)
