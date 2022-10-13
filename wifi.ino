#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>

#ifndef STASSID
#define STASSID "Type-P"
#define STAPSK  "6@Z*vW5a"
#endif

const char* ssid     = STASSID;
const char* password = STAPSK;

String wifiSsid="";
String wifiPassword;

String token;

ESP8266WebServer server(80);

void setup(void) {
  Serial.begin(115200);

  WiFi.softAP(ssid, password);

  Serial.print("AP IP address: ");
  IPAddress myIP = WiFi.softAPIP();
  Serial.println(myIP);
  Serial.println(WiFi.softAPIP());
  // Serial.println("HTTP server started");
  apiRoutes();
  server.begin();
}

void loop(void) {
  server.handleClient();
}

// ***************************** Network *********************************

void apiRoutes(){
  server.on("/", HTTP_GET, returnConnection);
  // server.sendHeader(F("Access-Control-Max-Age"), F("900"));
  // server.sendHeader(F("Keep-Alive"), F("timeout=5000"));
  // server.on(F("/token"), HTTP_OPTIONS, setCrossOrigin);
  server.on(F("/token"), HTTP_POST, setToken);
  server.on(F("/scan"),HTTP_GET, getScanNetworks);
  server.on(F("/login"), HTTP_POST, setCredentials);
}

void setCrossOrigin(){
    server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
    server.sendHeader(F("Access-Control-Max-Age"), F("600"));
    server.sendHeader(F("Access-Control-Allow-Methods"), F("PUT,POST,GET,OPTIONS"));
    server.sendHeader(F("Access-Control-Allow-Headers"), F("*"));
    server.sendHeader(F("Access-control-Allow-Credentials"), F("false"));
    // server.send(204);
};

void returnConnection(){
  DynamicJsonDocument doc(1024);
  JsonObject obj = doc.createNestedObject();
  obj["typep"] = true;
  String jsonSerialized;
  serializeJson(obj, jsonSerialized);
  server.send(202, "application/json", jsonSerialized);  
}

void setToken(){
  DynamicJsonDocument doc(1024);
  JsonObject obj = doc.createNestedObject();
  token = server.arg("token");
  Serial.println(server.args());
  Serial.println(token);
    Serial.println(server.argName(0));
  if(token!=""){
    obj["token_paried"] = true;
  }else{
    obj["token_paried"] = false;
  }
  String jsonSerialized;
  serializeJson(obj, jsonSerialized);
  server.send(202, "application/json", jsonSerialized);
}

void getScanNetworks() {
  Serial.println("Scanning WiFi networks...");
  // int n = WiFi.scanNetworksAsync(scanWifi,false);
  WiFi.scanNetworks(true,false);
  
  DynamicJsonDocument doc(1024);
  JsonArray jsonArray = doc.createNestedArray();

  while(WiFi.scanComplete()<0){
    Serial.println(WiFi.scanComplete());
  }
  int n = WiFi.scanComplete();

  Serial.println("scan complete");
  if (n == 0) {
    Serial.println("no networks found");
    JsonObject obj = doc.createNestedObject();
    obj["no_network"] = true;
    jsonArray.add(obj);
  } else {
    Serial.print(n);
    Serial.println(" networks found:");
    
    String ssid;
    int rssi;
    String security;
    JsonObject obj = doc.createNestedObject();
    for (int i = 0; i < n; ++i) {
      ssid = WiFi.SSID(i);
      rssi = WiFi.RSSI(i);
      security = WiFi.encryptionType(i) == WIFI_AP ? "none" : "enabled";
      Serial.print("Name: ");
      Serial.print(ssid);
      Serial.print(" - Strength: ");
      Serial.print(rssi);
      Serial.print(" - Security: ");
      Serial.println(security);

      
      obj["ssid"] = ssid;
      obj["strength"] = rssi;
      obj["security"] = security == "none" ? false : true;
      jsonArray.add(obj);
    }
  }
  String jsonSerialized;
  serializeJson(jsonArray, jsonSerialized);
  server.send(202, "application/json", jsonSerialized);  
}

void setCredentials(){
  String postSsid = server.arg("ssid");
  String postPassword = server.arg("password");
  wifiSsid = postSsid;
  wifiPassword = postPassword;

  WiFi.begin(postSsid, postPassword);
  Serial.println(postSsid);
  Serial.println(postPassword);

  while( WiFi.waitForConnectResult() == WL_IDLE_STATUS){
    Serial.println(WiFi.waitForConnectResult());
  }
  DynamicJsonDocument doc(1024);
  JsonObject obj = doc.createNestedObject();

  if(WiFi.waitForConnectResult()==WL_CONNECTED){
      obj["connected"] = true;
      obj["status"] = "CONNECTED";
  }else if (WiFi.waitForConnectResult()==WL_NO_SSID_AVAIL){
      obj["connected"] = false;
      obj["status"] = "SSID_UNREACHABLE";    
  }else if (WiFi.waitForConnectResult()==WL_CONNECT_FAILED){
      obj["connected"] = false;
      obj["status"] = "CONNECTION_FAILED";     
  }else if (WiFi.waitForConnectResult()==WL_WRONG_PASSWORD){
      obj["connected"] = false;
      obj["status"] = "WRONG_PASSWORD";     
  }else{
      obj["connected"] = false;
      obj["status"] = "TIME_OUT";     
  }
  String jsonSerialized;
  serializeJson(obj, jsonSerialized);
  server.send(202, "application/json", jsonSerialized);  
}
