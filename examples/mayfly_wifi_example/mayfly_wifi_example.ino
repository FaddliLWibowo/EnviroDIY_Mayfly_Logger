/**************************************************************************
mayfly_wifi_example.ino
Written By:  Jeff Horsburgh (jeff.horsburgh@usu.edu)
Creation Date: 6/3/2016
Development Environment: Arduino 1.6.9
Hardware Platform: Stroud Water Resources Mayfly Arduino Datalogger
Radio Module: XBee S6b WiFi module.

This sketch is an example of posting data to SparkFun's data stream server 
(http://data.sparkfun.com) using a Mayfly Arduino board and an XBee Wifi 
module. As a quick example, it uses the temperature values from the Mayfly's 
real time clock and POSTs them to a stream at http://data.sparkfun.com. This
sketch could easily be modified to post any sensor measurements to a stream
at data.sparkfun.com that has been configured to accept them.

This sketch was adapted from Jim Lindblom's example at:

https://learn.sparkfun.com/tutorials/internet-datalogging-with-arduino-and-xbee-wifi

Assumptions:
1. The XBee WiFi module has must be configured correctly to connect to the 
wireless network prior to running this sketch.
2. A data stream must be created at data.sparkfun.com to accept the data
from any sensors (in this example, only temperature is used). Replace the 
public and private keys below with the ones for your data stream.

DISCLAIMER:
THIS CODE IS PROVIDED "AS IS" - NO WARRANTY IS GIVEN.
**************************************************************************/
#include "Sodaq_DS3231.h"
#include <Wire.h>

// Parameters for the data stream and variables for data.sparkfun.com
String publicKey = "dZgMO0YrdRf3vOnrA8oz";  // Replace with the public key of your stream at data.sparkfun.com
String privateKey = "eE5NM2DkBGskdNB24mr1"; // Replace with the private key of your stream at data.sparkfun.com
String requestString;
String dataString;
float tempVal;

// XBee stuff
const int XB_BAUD = 9600; // XBee BAUD rate (9600 is default)

// Time in ms, where the program will stop waiting for serial data to come in
// Up to 5s may be needed to receive HTTP responses from the data.sparkfun.com server 
#define COMMAND_TIMEOUT 5000 // ms (5000 ms = 5 s)

// Update rate control (rate at which data are sent to server)
// data.sparkfun.com limits you to one update per 10 seconds
const unsigned long UPDATE_RATE = 20000; // 20000ms = 20 seconds
unsigned long lastUpdate = 0; // Keep track of last update time

void setup() {
  // Start the serial connections
  Serial.begin(57600);
  Serial1.begin(XB_BAUD);  // XBee hardware serial connection
  Serial.println("\r\nEnviroDIY Mayfly: Demo Post Data to Website\r\n");
}

void loop() {
  // Get updated values from sensors.
  readSensors();
  
  // Check to see if it is time to post the data to the server
  if (millis() > (lastUpdate + UPDATE_RATE))
  {
    Serial.println("\r\nSending data to data.sparkfun.com...\r\n");
    if (postData())
      Serial.println("\r\nSucessfully sent data to data.sparkfun.com!\r\n");
    else
      Serial.println("\r\nFailed to send data to data.sparkfun.com.\r\n");
    lastUpdate = millis();
  }
  
  // In the meantime, print data to the serial monitor:
  readSensors(); // Get updated values from sensors
  Serial.print(millis()); // Timestamp
  Serial.print(": ");
  Serial.println(String(tempVal) + " Degrees F");
  delay(1000);
}

// This function makes an HTTP connection to the data.sparkfun.com server and POSTs data
// The data.sparkfun.com server timestamps the data with the current server time at which
// the data are received.
int postData() {
  Serial1.flush();  // Make sure there is nothing still going out on the serial port
  readSensors(); // Get updated values from the sensors
  
  // Create the body of the POST request
  dataString = String("temperature=") + String(tempVal);

  // Create the full POST request
  requestString = "POST /input/" + publicKey + " HTTP/1.1\r\n";
  requestString += "Host: data.sparkfun.com\r\n";
  requestString += "Phant-Private-Key: " + privateKey + "\r\n";
  requestString += "Connection: close\r\n";
  requestString += "Cache-Control: no-cache\r\n";
  requestString += "Content-Type: application/x-www-form-urlencoded\r\n";
  requestString += "Content-Length: " + String(dataString.length()) + "\r\n";
  requestString += "\r\n";
  requestString += dataString;

  // Print the POST requst string to the serial port so I can look at it
  Serial.print(requestString + "\r\n");
  Serial.println();
  
  // Send the POST request string to the XBee serial port
  Serial1.print(requestString);
  // Wait for the transmission of outgoing serial data to complete
  Serial1.flush();

  // Add a brief delay for at least the first 12 characters of the HTTP response to come back
  int timeout = COMMAND_TIMEOUT;
  while ((timeout-- > 0) && (Serial1.available() < 12))
  {
    delay(1);
  }
  
  // Process the HTTP response
  char response[12];
  if ((Serial1.available() > 0) && (timeout > 0))
  {
    // Theres already a response from the server. Read the first 12 characters.
    for (int i=0; i<12; i++) {
      response[i] = Serial1.read();
    }
    
    // Read the rest of the response to the end to clear it out
    while (Serial1.available() > 0)
      Serial1.read();
      
    // Check the response to see if it was successful
    if (memcmp(response, "HTTP/1.1 200", 12) == 0) 
    {
      // The first 12 characters of the response indicate "HTTP/1.1 200" which is success
      return 1;
    }
    else 
    {
      Serial.println(response); // There was a Non-200 response from the server
      return 0;
    }
  }
  else // Otherwise timeout, no response from server
  {
    Serial.println("There is no server response.");
    return 0;
  }
}

// readSensors() updates global variables with the current sensor measurements
void readSensors()
{
  // Get the temperature from the Mayfly's real time clock and convert to Farenheit
  rtc.convertTemperature();  //convert current temperature into registers
  tempVal = rtc.getTemperature();
  tempVal = (tempVal * 9.0/5.0) + 32.0; // Convert to farenheit  
}
