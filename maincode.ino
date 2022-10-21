#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <FirebaseESP8266.h>
// Provide the token generation process info.
#include <addons/TokenHelper.h>

// Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>
#include "definitions.h"

const char* ssid     = STASSID;
const char* password = STAPSK;

String wifiSsid="";
String wifiPassword;

ESP8266WebServer server(80);

// Define Firebase Data object
FirebaseData stream;
FirebaseData fbdo;

FirebaseJson dataToApp;
FirebaseJson dataFromApp;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;

long currentSec=0;

// ############################################################## Recycle Data #####################################################

unsigned int timeDefined=0;

void setup(void) {
  Serial.begin(115200);

  WiFi.softAP(ssid, password);

  Serial.print("AP IP address: ");
  IPAddress myIP = WiFi.softAPIP();
  Serial.println(myIP);
  Serial.println(WiFi.softAPIP());
  // Serial.println("HTTP server started");
  apiRoutes();
  
  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the user sign in credentials */
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

  /* In case the certificate data was used  */
  config.cert.data = rootCACert;

  // Or custom set the root certificate for each FirebaseData object
  fbdo.setCert(rootCACert);

  Firebase.reconnectWiFi(true);
  Firebase.setDoubleDigits(5);
  Firebase.setStreamCallback(stream, streamCallback, streamTimeoutCallback);
  Firebase.begin(&config, &auth);

  server.begin();
}

// ######################################################### LOOP ##########################################################


void loop(void) {
  server.handleClient();
  if(Firebase.ready() && (millis() - sendDataPrevMillis > 1000 || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();
    currentSec++;
  }
}

// ####################################################### FUNCTIONS ########################################################

// ********************************************************* Network **************************************************************

void apiRoutes(){
  server.on("/", HTTP_GET, returnConnection);
  server.on(F("/scan"),HTTP_GET, getScanNetworks);
  server.on(F("/login"), HTTP_POST, setCredentials);
  server.on(F("/firebase"),HTTP_GET, getFirebaseConnection);
}

void returnConnection(){
  DynamicJsonDocument doc(1024);
  JsonObject obj = doc.createNestedObject();
  obj["typep"] = true;
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
      connectToFirebase();
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

void getFirebaseConnection(){
  DynamicJsonDocument doc(1024);
  JsonObject obj = doc.createNestedObject();
  dataToApp.add("func", "OVEN_CONNECTED");
  dataToApp.add("val", true);
  if(Firebase.ready()){    
    if(Firebase.setJSON(fbdo, PATH_TO_APP, dataToApp)){    
      obj["firebase_connected"] = true;
    }else{
      obj["firebase_connected"] = false;
      // connectToFirebase();
    }
  }else{
      obj["firebase_connected"] = false;
  }
  dataToApp.clear();
  String jsonSerialized;
  serializeJson(obj, jsonSerialized);
  server.send(202, "application/json", jsonSerialized);  
}

// ******************************************************* FIREBASE ************************************************************

void connectToFirebase(){  
  stream.setBSSLBufferSize(2048 /* Rx in bytes, 512 - 16384 */, 512 /* Tx in bytes, 512 - 16384 */);
  if (!Firebase.beginStream(stream, PATH_FROM_APP)){
    Serial.printf("sream begin error, %s\n\n", stream.errorReason().c_str());
  }else{
    Serial.println("stream ready");
  }
}

void streamCallback(StreamData data)
{
  Serial.printf("value:");
  Serial.printf(data.payload().c_str());
  dataFromApp = data.payload();
}

void streamTimeoutCallback(bool timeout)
{
  if (timeout){
    
    Serial.println("stream timed out, resuming...\n");
  }

  if (!stream.httpConnected()){

    Serial.printf("error code: %d, reason: %s\n\n", stream.httpCode(), stream.errorReason().c_str());
  }
}