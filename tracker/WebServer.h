/*
   Copyright (C) 2018 SFini

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
  * @file WebServer.h
  *
  * Webserver Interface to communicate via esp8266 in station or access-point mode. 
  * Works with the SPIFFS (.html, .css, .js).
  */


#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include "Spiffs.h"
#include "HtmlTag.h"

/**
  * My Webserver interface. Works together with .html, .css and .js files from the SPIFFS.
  * Works mostly with static functions because of the server callback functions.
  */
class MyWebServer
{
public:
   static ESP8266WebServer server;    //!< Webserver helper class.
   static IPAddress        ip;        //!< Soft AP ip Address
   static DNSServer        dnsServer; //!< Dns server
   static MyOptions       *myOptions; //!< Reference to the options.
   static MyData          *myData;    //!< Reference to the data.

protected:
   static bool   loadFromSpiffs(String path);
   static String TextToXml(String data);
   static void   AddTableBegin(String &info);
   static void   AddTableTr(String &info);
   static void   AddTableTr(String &info, String name, String value);
   static void   AddTableEnd(String &info);
   static bool   GetOption(String id, String &option);
   static bool   GetOption(String id, long   &option);
   static bool   GetOption(String id, double &option);
   static bool   GetOption(String id, bool   &option);
   static void   AddBr(String &info);
   static void   AddOption(String &info, String id, String name, bool value, bool addBr = true);
   static void   AddOption(String &info, String id, String name, String value, bool addBr = true, bool isPassword = false);

public:
   static void handleRoot();
   static void loadMain();
   static void handleLoadMainInfo();
   static void loadFirmwareUpdate();
   static void loadSettings();
   static void handleLoadSettingsInfo();
   static void handleSaveSettings();
   static void handleLoadInfoInfo();
   static void loadConsole();
   static void handleLoadConsoleInfo();
   static void loadRestart();
   static void handleLoadRestartInfo();
   static void handleNotFound();
   static void handleWebRequests();

public:
   bool isWebServerActive; //!< Is the webserver currently active.
   
public:
   MyWebServer(MyOptions &options, MyData &data);
   ~MyWebServer();

   bool begin();
   void handleClient();
};

/* ******************************************** */

IPAddress        MyWebServer::ip(192, 168, 1, 1);       
DNSServer        MyWebServer::dnsServer;
ESP8266WebServer MyWebServer::server(80);
MyOptions       *MyWebServer::myOptions = NULL;
MyData          *MyWebServer::myData    = NULL;


/** Constructor/Destructor */
MyWebServer::MyWebServer(MyOptions &options, MyData &data)
   : isWebServerActive(false)
{
   myOptions = &options;
   myData    = &data;      
}
MyWebServer::~MyWebServer()
{
   myOptions = NULL;
   myData    = NULL;      
}

/** Starts the Webserver in station and/or ap mode and sets all the callback 
    functions for the specific urls. */
bool MyWebServer::begin()
{
   if (!myOptions || !myData) {
      return false;
   }

   MyDbg("MyWebServer::begin");
   WiFi.mode(WIFI_AP_STA);
   WiFi.softAP("ESP8266AP", "");
   WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0));  
   dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
   dnsServer.start(53, "*", ip);
   myData->softAPIP         = WiFi.softAPIP().toString();
   myData->softAPmacAddress =  WiFi.softAPmacAddress();
   MyDbg("SoftAPIP address: " + myData->softAPIP, true);
   MyDbg("SoftAPIP mac address: " + myData->softAPmacAddress, true);

   WiFi.begin(myOptions->wlanAP.c_str(), myOptions->wlanPassword.c_str());
   for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) { // 10 Sec versuchen
      MyDbg(".", true, false);
      myDelay(500);
   }
   if (WiFi.status() == WL_CONNECTED) {
      myData->stationIP = WiFi.localIP().toString();
      MyDbg("Connected to "        + myOptions->wlanAP, true);
      MyDbg("Station IP address: " + myData->stationIP, true);
   } else { // switch to AP Mode only
      MyDbg((String) "No connection to " + myOptions->wlanAP, true);
      WiFi.disconnect();
      WiFi.mode(WIFI_AP);
   }
   
   server.on("/",                    handleRoot);
   server.on("/Main.html",           loadMain);
   server.on("/MainInfo",            handleLoadMainInfo);
   server.on("/FirmwareUpdate.html", loadFirmwareUpdate);
   server.on("/Settings.html",       loadSettings);
   server.on("/SettingsInfo",        handleLoadSettingsInfo);
   server.on("/SaveSettings",        handleSaveSettings);
   server.on("/InfoInfo",            handleLoadInfoInfo);
   server.on("/Console.html",        loadConsole);
   server.on("/ConsoleInfo",         handleLoadConsoleInfo);
   server.on("/Restart.html",        loadRestart);
   server.on("/RestartInfo",         handleLoadRestartInfo);
   server.onNotFound(handleWebRequests);

   server.begin(); 
   MyDbg("Server listening", true);

   isWebServerActive = true;
   return true;
}

/** Handle the http requests. */
void MyWebServer::handleClient()
{
   if (isWebServerActive) {
      server.handleClient();
      dnsServer.processNextRequest();  
   }
}

/** Helper HTML text conversation function for special character. */
String MyWebServer::TextToXml(String data)
{
   data.replace(F("&"), F("%26"));
   data.replace(F("<"), F("%3C"));
   data.replace(F(">"), F("%3E"));
   return data;
} 

/** Helper function to start a HTML table. */
void MyWebServer::AddTableBegin(String &info)
{
   info += "<table style='width:100%'>";
}

/** Helper function to write one HTML row with no data. */
void MyWebServer::AddTableTr(String &info)
{
   info += "<tr><th></th><td>&nbsp;</td></tr>";
}

/** Helper function to add one HTML table row line with data. */
void MyWebServer::AddTableTr(String &info, String name, String value)
{
   if (value != "") {
      info += "<tr>";
      info += "<th>" + TextToXml(name)  + "</th>";
      info += "<td>" + TextToXml(value) + "</td>";
      info += "</tr>";
   }
}
  
/** Helper function to add one HTML table end element. */
void MyWebServer::AddTableEnd(String &info)
{
   info += "</table>";
}

/** Reads one string option from the url args. */
bool MyWebServer::GetOption(String id, String &option)
{
   if (server.hasArg(id)) {
      option = server.arg(id);
      return true;
   }
   return false;
}

/** Reads one long option from the URL args. */
bool MyWebServer::GetOption(String id, long &option)
{
   String opt = server.arg(id);

   if (opt != "") {
      option = atoi(opt.c_str());
      return true;
   }
   return false;
}

/** Reads one double option from the URL args. */
bool MyWebServer::GetOption(String id, double &option)
{
   String opt = server.arg(id);

   if (opt != "") {
      option = atof(opt.c_str());
      return true;
   }
   return false;
}

/** Reads one bool option from the URL args. */
bool MyWebServer::GetOption(String id, bool &option)
{
   String opt = server.arg(id);

   option = false;
   if (opt != "") {
      if (opt == "on") {
         option = true;
      }
      return true;
   }
   return false;
}

/** Add a HTML br element. */
void MyWebServer::AddBr(String &info)
{
   info += "<br />";
}

/** Add one string input option field to the HTML source. */
void MyWebServer::AddOption(String &info, String id, String name, String value, bool addBr /* = true */, bool isPassword /* = false */)
{
   info += "<b>" + TextToXml(name) + "</b>";
   info += "<input id='" + id + "' name='" + id + "' ";
   if (isPassword) {
      info += " type='password' ";
   }
   info += "placeholder='' value='" + TextToXml(value) + "'>";
   if (addBr) {
      info += "<br />";
   }
}

/** Add one bool input option field to the HTML source. */
void MyWebServer::AddOption(String &info, String id, String name, bool value, bool addBr /* = true */)
{
   info += "<input style='width:auto;' id='" + id + "' name='" + id + "' type='checkbox' " + 
           (String) (value ? " checked" : "") + "><b>" + TextToXml(name) + "</b>";
   if (addBr) {
      info += "<br />";
   }
}

/** Helper function to load a file from the SPIFFS. */
bool MyWebServer::loadFromSpiffs(String path)
{
   bool ret = false;
   
   String dataType = "text/plain";
   if(path.endsWith("/")) path += "index.htm";
   
   if(path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
   else if(path.endsWith(".html")) dataType = "text/html";
   else if(path.endsWith(".htm")) dataType = "text/html";
   else if(path.endsWith(".css")) dataType = "text/css";
   else if(path.endsWith(".js")) dataType = "application/javascript";
   else if(path.endsWith(".png")) dataType = "image/png";
   else if(path.endsWith(".gif")) dataType = "image/gif";
   else if(path.endsWith(".jpg")) dataType = "image/jpeg";
   else if(path.endsWith(".ico")) dataType = "image/x-icon";
   else if(path.endsWith(".xml")) dataType = "text/xml";
   else if(path.endsWith(".pdf")) dataType = "application/pdf";
   else if(path.endsWith(".zip")) dataType = "application/zip";
   
   File dataFile = SPIFFS.open(path.c_str(), "r");
   if (dataFile) {
      if (server.hasArg("download")) {
         dataType = "application/octet-stream";
      }
      if (server.streamFile(dataFile, dataType) == dataFile.size()) {
         ret = true;
      }
      dataFile.close();
   }
   return ret;
}

/** Redirect a root call to the Main.html site. */
void MyWebServer::handleRoot()
{
   server.sendHeader("Location", "Main.html", true);
   server.send(302, "text/plain", "");
}

/** Sends the Main.html to the client. */
void MyWebServer::loadMain()
{
   if (loadFromSpiffs("/Main.html")) {
      return;
   }
   handleNotFound();
}

/** Handle the ajax call of the Main.html detail information. */
void MyWebServer::handleLoadMainInfo()
{
   if (!myOptions || !myData) {
      return;
   }
   
   String info;
   String onOff = server.arg("o");

   if (onOff == "1") {
      myOptions->gsmPower = !myOptions->gsmPower;
      myOptions->save();
   }

   AddTableBegin(info);
   if (myData->status != "") {
      AddTableTr(info, "Status", myData->status);
   }
   AddTableTr(info, "Battery",     String(myData->voltage,     1) + " V");
   AddTableTr(info, "Temperature", String(myData->temperature, 1) + " °C");
   AddTableTr(info, "Humidity",    String(myData->humidity,    1) + " %");
   AddTableTr(info, "Pressure",    String(myData->temperature, 1) + " hPa");
   if (myData->status != "") {
      AddTableTr(info, "Modem Info", myData->modemInfo);
      AddTableTr(info, "Longitude",  myData->longitude);
      AddTableTr(info, "Latitude",   myData->latitude);
      AddTableTr(info, "Altitude",   myData->altitude);
      AddTableTr(info, "Satellites", myData->satellites);
   }
   if (myData->secondsToDeepSleep >= 0) {
      AddTableTr(info, "Power saving in ", String(myData->secondsToDeepSleep) + " Seconds");
   }
   AddTableEnd(info);
   
   info +=
      "<table style='width:100%'>"
         "<tr>"
            "<td style='width:100%'>"
               "<div style='text-align:center;font-weight:bold;font-size:62px'>" 
                  + (String) (myOptions->gsmPower ? "ON" : "OFF") +
               "</div>"
            "</td>"
         "</tr>"
      "</table>";
   
   server.send(200,"text/html", info);
}

/** Load the FirmwareUpdate page after starting OTA. */
void MyWebServer::loadFirmwareUpdate()
{
   if (!myOptions || !myData) {
      return;
   }
   if (!myData->isOtaActive) {
      SetupOTA();
      ArduinoOTA.begin();
      myData->isOtaActive = true;
   }

   if (loadFromSpiffs("/FirmwareUpdate.html")) {
      return;
   }
   handleNotFound();
}

/** Load the Settings page. */
void MyWebServer::loadSettings()
{
   if (loadFromSpiffs("/Settings.html")) {
      return;
   }
   handleNotFound();
}

/** Sends the ajax settings detail information to the client. */
void MyWebServer::handleLoadSettingsInfo()
{
   if (!myOptions || !myData) {
      return;
   }
   
   String info;

   MyDbg("LoadSettings", true);
   AddOption(info, "wlanAP",                 "WLAN SSID",                         myOptions->wlanAP);
   AddOption(info, "wlanPassword",           "WLAN Password",                     myOptions->wlanPassword, true, true);
   AddOption(info, "gprsAP",                 "GPRS AP",                           myOptions->gprsAP);
   AddOption(info, "isDebugActive",          "Debug Active",                      myOptions->isDebugActive);
   
   AddOption(info, "bme280CheckIntervalSec", "Temperature check every (Seconds)", String(myOptions->bme280CheckIntervalSec));
   AddOption(info, "isGsmEnabled",           "GSM Enabled",                       myOptions->isGsmEnabled);
   AddOption(info, "isGpsEnabled",           "GPS Enabled",                       myOptions->isGpsEnabled);
   AddOption(info, "gpsCheckIntervalSec",    "GPS check every (Seconds)",         String(myOptions->gpsCheckIntervalSec));
   AddOption(info, "phoneNumber",            "Information send to",               myOptions->phoneNumber);
   AddOption(info, "smsCheckIntervalSec",    "SMS check every (Seconds)",         String(myOptions->smsCheckIntervalSec));
   {
      HtmlTag fieldset(info, "fieldset");
      {
         HtmlTag legend(info, "legend");
         
         AddOption(info, "isDeepSleepEnabled", "Power saving mode active", myOptions->isDeepSleepEnabled, false);
      }
      AddOption(info, "powerSaveModeVoltage",  "Power saving mode under (Volt)", String(myOptions->powerSaveModeVoltage, 1));
      AddOption(info, "powerCheckIntervalSec", "Check power every (Seconds)",    String(myOptions->powerCheckIntervalSec));
      
      AddOption(info, "wakeTimeSec",          "Active time (Seconds)",           String(myOptions->wakeTimeSec));
      AddOption(info, "deepSleepTimeSec",     "DeepSleep time (Seconds)",        String(myOptions->deepSleepTimeSec));
   }
   AddBr(info);
   {
      HtmlTag fieldset(info, "fieldset");
      {
         HtmlTag legend(info, "legend");
         
         AddOption(info, "isMqttEnabled", "MQTT Active", myOptions->isMqttEnabled, false);
      }
      AddOption(info, "mqttName",                  "MQTT Name",                             myOptions->mqttName);
      AddOption(info, "mqttServer",                "MQTT Server",                           myOptions->mqttServer);
      AddOption(info, "mqttPort",                  "MQTT Port",                             String(myOptions->mqttPort));
      AddOption(info, "mqttUser",                  "MQTT User",                             myOptions->mqttUser);
      AddOption(info, "mqttPassword",              "MQTT Password",                         myOptions->mqttPassword, true, true);
      AddOption(info, "mqttReconnectIntervalSec",  "MQTT Reconnect every (Seconds)",        String(myOptions->mqttReconnectIntervalSec));
      AddOption(info, "mqttSendOnMoveEverySec",    "MQTT Send on moving every (Seconds)",   String(myOptions->mqttSendOnMoveEverySec));
      AddOption(info, "mqttSendOnNonMoveEverySec", "MQTT Send on standing every (Seconds)", String(myOptions->mqttSendOnNonMoveEverySec), false);
   }

   server.send(200,"text/html", info);
}

/** Reads all the options from the url and save them to the SPIFFS. */
void MyWebServer::handleSaveSettings()
{
   if (!myOptions || !myData) {
      return;
   }
   
   MyDbg("SaveSettings", true);
   GetOption("gprsAP",                    myOptions->gprsAP);
   GetOption("wlanAP",                    myOptions->wlanAP);
   GetOption("wlanPassword",              myOptions->wlanPassword);
   GetOption("isDebugActive",             myOptions->isDebugActive);
   GetOption("bme280CheckIntervalSec",    myOptions->bme280CheckIntervalSec);
   GetOption("isGsmEnabled",              myOptions->isGsmEnabled);
   GetOption("phoneNumber",               myOptions->phoneNumber);
   GetOption("smsCheckIntervalSec",       myOptions->smsCheckIntervalSec);
   GetOption("isGpsEnabled",              myOptions->isGpsEnabled);
   GetOption("gpsCheckIntervalSec",       myOptions->gpsCheckIntervalSec);
   GetOption("isDeepSleepEnabled",        myOptions->isDeepSleepEnabled);
   GetOption("powerSaveModeVoltage",      myOptions->powerSaveModeVoltage);
   GetOption("powerCheckIntervalSec",     myOptions->powerCheckIntervalSec);
   GetOption("wakeTimeSec",               myOptions->wakeTimeSec);
   GetOption("deepSleepTimeSec",          myOptions->deepSleepTimeSec);
   GetOption("isMqttEnabled",             myOptions->isMqttEnabled);
   GetOption("mqttName",                  myOptions->mqttName);
   GetOption("mqttServer",                myOptions->mqttServer);
   GetOption("mqttPort",                  myOptions->mqttPort);
   GetOption("mqttUser",                  myOptions->mqttUser);
   GetOption("mqttPassword",              myOptions->mqttPassword);
   GetOption("mqttReconnectIntervalSec",  myOptions->mqttReconnectIntervalSec);
   GetOption("mqttSendOnMoveEverySec",    myOptions->mqttSendOnMoveEverySec);
   GetOption("mqttSendOnNonMoveEverySec", myOptions->mqttSendOnNonMoveEverySec);

   myOptions->save();

   if (false /* reboot */) {
      myData->restartInfo = 
         "<b>Settings saved</b>";
      loadRestart();
   }
   loadMain();
}

/** Load the detail info part via ajax call. */
void MyWebServer::handleLoadInfoInfo()
{
   if (!myOptions || !myData) {
      return;
   }
   
   String info;
   String ssidRssi = (String) myOptions->wlanAP + " (" + WifiGetRssiAsQuality(WiFi.RSSI()) + "%)";

   AddTableBegin(info);
   if (myData->status != "") {
      AddTableTr(info, "Status",               myData->status);
      AddTableTr(info);
   }
   if (myData->isOtaActive) {
      AddTableTr(info, "OTA",                  "Active");
      AddTableTr(info);
   }
   if (ssidRssi != "" || myData->softAPIP || myData->softAPmacAddress != "" || myData->stationIP != "") {
      AddTableTr(info, "AP1 SSID (RSSI)",      ssidRssi);
      AddTableTr(info, "AP IP",                myData->softAPIP);
      AddTableTr(info, "Locale IP",            myData->stationIP);
      AddTableTr(info, "MAC Address",          myData->softAPmacAddress);
      AddTableTr(info);
   }
   if (myData->modemInfo     != "" || myData->modemIP != ""      || myData->imei        != "" || myData->cop != "" || 
       myData->signalQuality != "" || myData->batteryLevel != "" || myData->batteryVolt != "") {
      AddTableTr(info, "Modem Info",           myData->modemInfo); 
      AddTableTr(info, "Modem IP",             myData->modemIP);
      AddTableTr(info, "IMEI",                 myData->imei);
      AddTableTr(info, "COP",                  myData->cop);
      AddTableTr(info, "Signal Quality",       myData->signalQuality);
      AddTableTr(info, "Battery Level",        myData->batteryLevel);
      AddTableTr(info, "Battery Volt",         myData->batteryVolt);
      AddTableTr(info);
   }
   if (myData->longitude  != "" || myData->latitude != "" || myData->altitude != "" || myData->kmph    != "" || 
       myData->satellites != "" || myData->course   != "" || myData->gpsDate  != "" || myData->gpsTime != "") {
      AddTableTr(info, "Longitude",            myData->longitude);
      AddTableTr(info, "Latitude",             myData->latitude);
      AddTableTr(info, "Altitude",             myData->altitude);
      AddTableTr(info, "Km/h",                 myData->kmph);
      AddTableTr(info, "Satellite",            myData->satellites);
      AddTableTr(info, "Course",               myData->course);
      AddTableTr(info, "GPS Datum",            myData->gpsDate);
      AddTableTr(info, "GPS Time",             myData->gpsTime);
      AddTableTr(info);
   }
   if (myData->isMoving || myData->movingDistance != 0.0) {
      AddTableTr(info, "Moving",               myData->isMoving ? "Yes" : "No");
      AddTableTr(info, "Distance (m)",         String(myData->movingDistance, 2));
      AddTableTr(info);
   }
   AddTableTr(info, "ESP Chip ID",            String(ESP.getChipId()));
   AddTableTr(info, "Flash Chip ID",          String(ESP.getFlashChipId()));
   AddTableTr(info, "Real Flash Memory",      String(ESP.getFlashChipRealSize() / 1024) + " kB");
   AddTableTr(info, "Total Flash Memory",     String(ESP.getFlashChipSize()     / 1024) + " kB");
   AddTableTr(info, "Used Flash Memory",      String(ESP.getSketchSize()        / 1024) + " kB");
   AddTableTr(info, "Free Sketch Memory",     String(ESP.getFreeSketchSpace()   / 1024) + " kB");
   AddTableTr(info, "Free Heap Memory",       String(ESP.getFreeHeap()          / 1024) + " kB");
   AddTableEnd(info);
   
   server.send(200,"text/html", info);
}

/** Load the console page */
void MyWebServer::loadConsole()
{
   if (loadFromSpiffs("/Console.html")) {
      if (server.hasArg("clear")) {
         myData->logInfos.removeAll();
      }
      return;
   }
   handleNotFound();
}

/** Handle the Console ajax calls to get AT commands and show the result of the calls or debug informations. */
void MyWebServer::handleLoadConsoleInfo()
{
   if (!myOptions || !myData) {
      return;
   }
   
   String sendData;
   String cmd      = server.arg("c1");
   String startIdx = server.arg("c2");

   if (server.hasArg("c1")) {
      MyDbg(cmd, true);
      myData->consoleCmds.addTail(cmd);
   }
   sendData = 
      "<r>"
         "<i>" + String(myData->logInfos.count()) + "</i>"
         "<j>1</j>"
         "<l>";
            for (int i = atoi(startIdx.c_str()); i < myData->logInfos.count(); i++) {
               sendData += TextToXml(myData->logInfos.getAt(i));
               sendData += '\n';
            }
   sendData += 
         "</l>"
      "</r>";
      
   server.send(200,"text/xml", sendData.c_str());
}

/** Load the restart page. */
void MyWebServer::loadRestart()
{
   if (loadFromSpiffs("/Restart.html")) {
      MyDbg("Load File /Restart.html", true);
      myDelay(2000);
      MyDbg("Restart", true);
      ESP.restart();
      return;
   }
   handleNotFound();
}

/** Handle the restart ajax call of the restart reason. */
void MyWebServer::handleLoadRestartInfo()
{
   if (!myOptions || !myData) {
      return;
   }
   
   server.send(200,"text/html", myData->restartInfo);
   myData->restartInfo = "";
}

/** Handle if the url could not be found. */
void MyWebServer::handleNotFound()
{
   if (!myOptions || !myData) {
      return;
   }
   
   String message = "File Not Found\n\n";
   
   message += "URI: ";
   message += server.uri();
   message += "\nMethod: ";
   message += (server.method() == HTTP_GET)?"GET":"POST";
   message += "\nArguments: ";
   message += server.args();
   message += "\n";
   for (uint8_t i=0; i<server.args(); i++){
      message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
   }
   server.send(404, "text/plain", message);
   MyDbg(message, true);
}

/** Default for an unknown web request on not found. */
void MyWebServer::handleWebRequests()
{
   if (loadFromSpiffs(server.uri())) {
      return;
   }
   handleNotFound();
}
