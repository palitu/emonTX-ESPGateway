#include <PubSubClient.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

//Used to test input during development
#include <Ticker.h>
Ticker ticker;

//varaibles to store each data set
const int DATASLOTS = 13;
String values[DATASLOTS]; 

// Setup MQTT Details.
byte mqtt_server[] = { 10, 1, 1, 2 };
WiFiClient emonTx_ESPGateway;
PubSubClient client(emonTx_ESPGateway);

long lastMsg = 0;
char msg[50];
char cnodeId[10];
char cdata[10];
int value = 0;


//#include "config.h"
//contains
const char* emoncmsKey = "whateveryourkeyis...";


//emoncoms
const char* host = "emoncms.org";


String inputString = "";         // a string to hold incoming data
boolean stringComplete = false;  // whether the string is complete
String topic = "";

String c1 = "0";
String c2 = "0";
String c3 = "0";
String v = "0";

void serialEvent();
bool isInt(String str);
void sendToEmonCMS(String nodeId, String data);

String getValue(String data, char separator, int index)
{
    int maxIndex = data.length()-1;
    int j=0;
    String chunkVal = "";

    for(int i=0; i<=maxIndex && j<=index; i++)
    {

      if(data[i]==separator)
      {
        j++;

        if(j>index)
        {
          chunkVal.trim();
          return chunkVal;
        }    

        chunkVal = "";    
      }  
      else {
        chunkVal.concat(data[i]);
      }
    }  
}

// used to test when no appropriate data is being recieved.
void injectData() {
  inputString = "10 1163.90 243.00 12376.00 0.00 220.22";
  stringComplete = true;
  Serial.println("injected");
}

void setup() {
  //ticker.attach(2, injectData);
  
  // make sure the serial baudrate is the same as on your serial sending device
  Serial.begin(9600);

  WiFiManager wifi;
  
  wifi.setTimeout(120); //so if it restarts and router is not yet online, it keeps rebooting and retrying to connect
  if(!wifi.autoConnect()) {
    Serial.println("failed to connect and hit timeout");

    delay(1000);
    //reset and try again
    ESP.reset();
    delay(5000);
  } 

  // Initiate MQTT client
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  inputString.reserve(200);

/*---------Start OTA Code---------------------*/
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);
   ArduinoOTA.setHostname("EmonTx-ESPGateway");

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
/*-----------End OTA code---------------------*/

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  String subString;
  int spacePos;

  ArduinoOTA.handle();

  //MQTT Connection Handling
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  //MQTT Heartbeat
  long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    ++value;
    snprintf (msg, 75, "hello world #%ld", value);
    Serial.print("Publish message: ");
    Serial.println(msg);
    
    client.publish("outTopic", msg); 
  }
  
  // check serial line for new information
  serialEvent(); 


  //  Process the serial data, post to emonCMS and publish to MQTT
  if (stringComplete) {
    String serialData = inputString;
    spacePos = serialData.indexOf(" ");
    if (spacePos != -1) {
      //found a space, try to extract nodeId
      String nodeId = serialData.substring(0, spacePos);
      Serial.println(nodeId);
      if (isInt(nodeId)) {
        Serial.println("number");
        String data = serialData.substring(spacePos + 1);
        data.replace(" ", ",");

        delay(0);
        
        sendToEmonCMS(nodeId, data);
        
        delay(0);

        
        //Chop up the serial data into individual entities, and publish to MQTT
        for (int x = 0; x <= DATASLOTS; x++) {
          subString = getValues(serialData, ' ', x);
          
          if (subString != "error")  {
              values[x] = subString;
              //Dynamic posting, by order of recieved data
              //MQTTPost("emonTx/Serial", values[x]);  
          }
          else {
              x = DATASLOTS + 1;
          }
        }

        // Publish data with appropriate topics.
        MQTTPost("emonTx/" + nodeId + "/CT1", values[1] );
        MQTTPost("emonTx/" + nodeId + "/CT2", values[2] );
        MQTTPost("emonTx/" + nodeId + "/CT3", values[3] );
        MQTTPost("emonTx/" + nodeId + "/CT4", values[4] );
        MQTTPost("emonTx/" + nodeId + "/Vrms", values[5] );  


        //Reset the serial inputs
        stringComplete = false;
        inputString = "";
        
      }
    }
  }
}

// function to simplify publishing to MQTT - basically just typecasts the strings as char
void MQTTPost (String topic, String data) {
  client.publish( (char*) topic.c_str(), (char*) data.c_str());
}

/*
  SerialEvent occurs whenever a new data comes in the
  hardware serial RX.  This routine is run between each
  time loop() runs, so using delay inside loop can delay
  response.  Multiple bytes of data may be available.
*/
void serialEvent() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n') {
      stringComplete = true;
      return;
    } else {
      inputString += inChar;
    }
  }
}

//check if string is all digits
bool isInt(String str) {
  for (byte i = 0; i < str.length(); i++)
  {
    if (!isDigit(str.charAt(i))) {
      return false;
    }
  }
  return true;
}

// Post data to EmonCMS
void sendToEmonCMS(String nodeId, String data) {
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }

  // We now create a URI for the request
  String url = "/input/post.json?node=";
  url += nodeId;
  url += "&apikey=";
  url += emoncmsKey;
  url += "&csv=";
  url += data;

  Serial.println(url);

  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");
  delay(10);

  // Read all the lines of the reply from server
  while (client.available()) {
    String line = client.readStringUntil('\r');
    //Serial.print(line);
  }

}


/*-----------MQTT Functions---------------------*/
  void reconnect() {
    // Loop until we're reconnected
    while (!client.connected()) {
      Serial.print("Attempting MQTT connection...");
      // Attempt to connect
      if (client.connect("ESP8266Client")) {
        Serial.println("connected");

        
        // Once connected, publish an announcement...
        client.publish("emonTx-Gateway", "Connected to MQTT server");
        
        // ... and resubscribe to appropriate Topics
        client.subscribe("emonTx-Gateway");

        
      } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        // Wait 5 seconds before retrying
        delay(5000);
      }
    }
  }


  void callback(char* chartopic, byte* payload, unsigned int length) {
    char p[length + 1];
    memcpy(p, payload, length);
    p[length] = NULL;
    String message(p);
    String topic(chartopic);
    
    
    Serial.print("Message arrived [");
    Serial.print(chartopic);
    Serial.print("] ");
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println();
  }
/*-----------End MQTT Functions---------------------*/

// Chop serial input based on a seperator charactex and index number
String getValues(String data, char separator, int index) {
 int found = 0;
  int strIndex[] = {
0, -1  };
  int maxIndex = data.length()-1;
  for(int i=0; i<=maxIndex && found<=index; i++){
  if(data.charAt(i)==separator || i==maxIndex){
  found++;
  strIndex[0] = strIndex[1]+1;
  strIndex[1] = (i == maxIndex) ? i+1 : i;
  }
 }
  return found>index ? data.substring(strIndex[0], strIndex[1]) : "error";
}

