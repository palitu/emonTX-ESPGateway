# emonTX-ESPGateway
A program to ease the integration of EmonTx (i use the SMD Shield on a 3.3V leonardo) with the rest of the world.  Flash to device via arduino, either via serial or ArduinoOTA.

It currently connects to:
 - EmonCMS (defaults to emoncms.org)
 - MQTT (topic: emonTx/nodeId/CT1-4 and Vrms)
  
It has the following features:
 - Wifi Manager, use your phone to set SSID and password
 - OTA programming in Arduino 1.6+
 - MQTT "Serial Debugging" under "emonTx/Serial" topic.
 - Heartbeat

Please feel free to improve the code/project - there is plenty of scope to do this better.

#Future Plans
Ultimately i want this to be an RFM -> MQTT bridge for emonTx.  I am currenlty looking for a decent library for the RFMs on the ESP8266's.  Let me know if you see one.

#thanks
This is based off of the following repository: https://github.com/tzapu/emonTXSerialGateway
