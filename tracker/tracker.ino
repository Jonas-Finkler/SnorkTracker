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
  * @file tracker.ino 
  *
  * Main file with setup() and loop() functions
  */


#include <SoftwareSerial.h>
#include <ArduinoOTA.h>
#include <FS.h>

#include "Config.h"
#define USE_CONFIG_OVERRIDE //!< Switch to use ConfigOverride
#ifdef USE_CONFIG_OVERRIDE
  #include "ConfigOverride.h"
#endif

#include "Debug.h"
#include "StringList.h"
#include "Utils.h"
#include "Options.h"
#include "Data.h"
#include "DeepSleep.h"
#include "WebServer.h"
#include "GsmPower.h"
#include "GsmGps.h"
#include "SmsCmd.h"
#include "Mqtt.h"
#include "BME280.h"


#define     PIN_TX        14                               //!< Transmit-pin to the sim808
#define     PIN_RX        12                               //!< receive-pin to the sim808
#define     PIN_POWER     0                                //!< power on/off to DC-DC LM2596
#define     PIN_BME_POWER 2                                //!< power pin to the BME280 module
#define     ANALOG_FACTOR 0.03                             //!< Factor to the analog voltage divider

MyOptions   myOptions;                                     //!< The global options.
MyData      myData;                                        //!< The global collected data.
MyDeepSleep myDeepSleep(myOptions, myData);                //!< Helper class for deep sleeps.
MyWebServer myWebServer(myOptions, myData);                //!< The Webserver
MyGsmPower  myGsmPower(PIN_POWER);                         //!< Helper class to switch on/off the sim808 power.
MyGsmGps    myGsmGps(myOptions, myData, PIN_RX, PIN_TX);   //!< sim808 gsm/gps communication class.
MySmsCmd    mySmsCmd(myGsmGps, myOptions, myData);         //!< sms controller class for the sms handling.
MyMqtt      myMqtt(myGsmGps, myOptions, myData);           //!< Helper class for the mqtt communication.
MyBME280    myBME280(myOptions, myData, PIN_BME_POWER);    //!< Helper class for the BME280 sensor communication.

bool        gsmHasPower = false;                           //!< Is the DC-DC modul switched on?
bool        isStarting  = false;                           //!< Are we in a starting process?
bool        isStopping  = false;                           //!< Are we in a stopping process?

/** Overwritten Debug Function 
  * It logs all the debug calls to the console string-list
  * And call a refresh of the webserver for not blocking the system.
  */
void myDebugInfo(String info, bool fromWebserver, bool newline)
{
   static bool lastNewLine = true;
   
   if (newline || lastNewLine != newline) {
      String secs = String(millis() / 1000) + ": ";

      myData.logInfos.addTail(secs + info);
      if (!lastNewLine) {
         Serial.println("");
      }
      Serial.println(secs + info);
   } else {
      String tmp = myData.logInfos.removeTail();
      myData.logInfos.addTail(tmp + info);
      Serial.print(info);
   }
   lastNewLine = newline;

   if (!fromWebserver) {
      myWebServer.handleClient();   
      myWebServer.handleClient();   
      myWebServer.handleClient();   
   } 
}

/** Overwritten delay loop for refreshing the webserver on waiting processes. */
void myDelayLoop()
{
   myWebServer.handleClient();   
   myWebServer.handleClient();   
   myWebServer.handleClient(); 
   delay(0);
   yield();
}

/** Helper Function to read the power supply voltage from the voltage divider. */
void readVoltage(bool dbg = false)
{
   myData.voltage = ANALOG_FACTOR * analogRead(A0); // Volt
   if (dbg) {
      MyDbg("Voltage: " + String(myData.voltage, 1));
   }
}

/** Main setup function. This is also called after every deep sleep. 
  * Do the initialization of every sub-component. */
void setup() 
{
   Serial.begin(115200); 
   MyDbg("Start ESP8266...");

   myGsmPower.begin();
   SPIFFS.begin();
   myOptions.load();
   readVoltage(true);
   myDeepSleep.begin();
   
   myWebServer.begin();
   myMqtt.begin();
   mySmsCmd.begin();
   myBME280.begin();
}

/** Main loop function.
  * Read the power supply voltage.
  * Checks for console inputs and send them to the SIM808 modul.
  * Checks for OTA activities.
  * Start or stop the gsm and/or gps activities.
  * Check for receiving sms and process them.
  * Checks the gps and send the overall information to a mqtt server if needed.
  * Starts the deep sleep mode if needed.
  */
void loop() 
{
   readVoltage();
   myBME280.readValues();

   if (!myData.consoleCmds.isEmpty()) {
      String cmd = myData.consoleCmds.removeHead();
      
      myGsmGps.sendAT(cmd);
   }
   
   myWebServer.handleClient();
   
   if (myData.isOtaActive) {
      ArduinoOTA.handle();    
   }

   if (myOptions.gsmPower && !gsmHasPower) {
      if (!isStarting && !isStopping) {
         isStarting = true;
         myGsmPower.on(); 
         gsmHasPower = true;
         if (!myGsmGps.begin()) {
            myOptions.gsmPower = false;
         }
         isStarting = false;
      }
   }
   if (!myOptions.gsmPower && gsmHasPower) {
      if (!isStarting && !isStopping) {
         isStopping = true;
         myGsmGps.stop();
         myGsmPower.off();
         gsmHasPower = false;
         isStopping  = false;   
      }
   }
   if (gsmHasPower && !isStarting && !isStopping) {
      myGsmGps.handleClient();
      mySmsCmd.handleClient();
      if (myOptions.isMqttEnabled) {
         myMqtt.handleClient();
      }
   }

   if (myDeepSleep.haveToSleep()) {
      if (myGsmGps.isGsmActive) {
         myGsmGps.stop();
      }
      myGsmPower.off();
      myDeepSleep.sleep();
   }
   
   yield();
   delay(10); 
}
