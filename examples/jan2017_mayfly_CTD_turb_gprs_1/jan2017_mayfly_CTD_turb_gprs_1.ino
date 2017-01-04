#include <Wire.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <SPI.h>
#include <SD.h>
#include <SDI12_Mod.h>
#include <GPRSbee.h>
#include <Adafruit_ADS1015.h>

//SODAQ  libraries
#include <RTCTimer.h>
#include <Sodaq_DS3231.h>
#include <Sodaq_PcInt_Mod.h>

String targetURL;
#define APN "apn.konekt.io"

#define READ_DELAY 1

//RTC Timer
RTCTimer timer;

String dataRec = "";
int currentminute;
long currentepochtime = 0;
float boardtemp = 0.0;
int testtimer = 0;   
int testminute = 2;

int batteryPin = A6;    // select the input pin for the potentiometer
int batterysenseValue = 0;  // variable to store the value coming from the sensor
float batteryvoltage;
Adafruit_ADS1115 ads;     /* Use this for the 16-bit version */

char CTDaddress = '1';      //for one sensor on channel '1' 
float CTDtempC, CTDdepthmm, CTDcond;
float lowturbidity, highturbidity;   //variables to hold the calculated NTU values

#define DATAPIN 7         // change to the proper pin for sdi-12 data pin, pin 7 on shield 3.0
int SwitchedPower = 22;    // sensor power is pin 22 on Mayfly
SDI12 mySDI12(DATAPIN); 

#define GPRSBEE_PWRPIN  23  //DTR
#define XBEECTS_PIN     19   //CTS

//RTC Interrupt pin
#define RTC_PIN A7
#define RTC_INT_PERIOD EveryMinute

#define SD_SS_PIN 12

//The data log file
#define FILE_NAME "SL0xxLog.txt"

//Data header
#define LOGGERNAME "SL0xx - Mayfly CTD & Turbidity Logger"
#define DATA_HEADER "DateTime_EST,TZ-Offset,Loggertime,BoardTemp,Battery_V,CTD_Depth_mm,CTD_temp_DegC,CTD_cond_dS/m,Turb_low_NTU,Turb_high_NTU"
     

void setup() 
{
  //Initialise the serial connection
  Serial.begin(57600);
  Serial1.begin(57600);
  rtc.begin();
  delay(100);
  mySDI12.begin(); 
  delay(100);
  pinMode(8, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(SwitchedPower, OUTPUT);
  digitalWrite(SwitchedPower, LOW);
  

  pinMode(23, OUTPUT);    // Bee socket DTR pin
  digitalWrite(23, LOW);   // on GPRSbee v6, setting this high turns on the GPRSbee.  leave it high to keep GPRSbee on.  

  gprsbee.init(Serial1, XBEECTS_PIN, GPRSBEE_PWRPIN);
  //Comment out the next line when used with GPRSbee Rev.4
  gprsbee.setPowerSwitchedOnOff(true);
  
  ads.begin();       //begin adafruit ADS1015

  greenred4flash();   //blink the LEDs to show the board is on

  setupLogFile();

  //Setup timer events
  setupTimer();
  
  //Setup sleep mode
  setupSleep();
  
  //Make first call
  Serial.println("Power On, running: SL081_mayfly_CTD_turb_gprs_1.ino");
  //showTime(getNow());
  

}

void loop() 
{
   
  //Update the timer 
  timer.update();
  
  if(currentminute % testminute == 0)
     {   //Serial.println("Multiple of x!!!   Initiating sensor reading and logging data to SDcard....");
          
          digitalWrite(8, HIGH);  
          dataRec = createDataRecord();
    
          digitalWrite(SwitchedPower, HIGH);
          delay(1000);
          CTDMeasurement(CTDaddress);  
          analogturbidity();
          digitalWrite(SwitchedPower, LOW);          
                     
          //Save the data record to the log file
          logData(dataRec);
 
   
          //Echo the data to the serial connection
          Serial.println();
          Serial.print("Data Record: ");
          Serial.println(dataRec);      
   
          assembleURL();
   
          delay(100);
          digitalWrite(23, HIGH);
          delay(1000);
        
          sendviaGPRS();
   
          delay(1000);

          digitalWrite(23, LOW);    
        
          delay(500);
   
          String dataRec = "";   
     
          digitalWrite(8, LOW);
          delay(100);
          testtimer++;       
     }
      if(testtimer >= 5)
      {testminute = 5;
      }
      
     delay(200);
     //Sleep          
     systemSleep();
}

void showTime(uint32_t ts)
{
  //Retrieve and display the current date/time
  String dateTime = getDateTime();
//  Serial.println(dateTime);
}

void setupTimer()
{
  
    //Schedule the wakeup every minute
  timer.every(READ_DELAY, showTime);
  
  //Instruct the RTCTimer how to get the current time reading
  timer.setNowCallback(getNow);


}

void wakeISR()
{
  //Leave this blank
}

void setupSleep()
{
  pinMode(RTC_PIN, INPUT_PULLUP);
  PcInt::attachInterrupt(RTC_PIN, wakeISR);

  //Setup the RTC in interrupt mode
  rtc.enableInterrupts(RTC_INT_PERIOD);
  
  //Set the sleep mode
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
}

void systemSleep()
{
  //This method handles any sensor specific sleep setup
  sensorsSleep();
  
  //Wait until the serial ports have finished transmitting
  Serial.flush();
  Serial1.flush();
  
  //The next timed interrupt will not be sent until this is cleared
  rtc.clearINTStatus();
    
  //Disable ADC
  ADCSRA &= ~_BV(ADEN);
  
  //Sleep time
  noInterrupts();
  sleep_enable();
  interrupts();
  sleep_cpu();
  sleep_disable();
 
  //Enbale ADC
  ADCSRA |= _BV(ADEN);
  
  //This method handles any sensor specific wake setup
 // sensorsWake();
}

void sensorsSleep()
{
  //Add any code which your sensors require before sleep
}

//void sensorsWake()
//{
//  //Add any code which your sensors require after waking
//}

String getDateTime()
{
  String dateTimeStr;
  
  //Create a DateTime object from the current time
  DateTime dt(rtc.makeDateTime(rtc.now().getEpoch()));

  currentepochtime = (dt.get());    //Unix time in seconds 

  currentminute = (dt.minute());
  //Convert it to a String
  dt.addToString(dateTimeStr); 
  return dateTimeStr;  
}

uint32_t getNow()
{
  currentepochtime = rtc.now().getEpoch();
  return currentepochtime;
}

void greenred4flash()
{
  for (int i=1; i <= 4; i++){
  digitalWrite(8, HIGH);   
  digitalWrite(9, LOW);
  delay(50);
  digitalWrite(8, LOW);
  digitalWrite(9, HIGH);
  delay(50);
  }
  digitalWrite(9, LOW);
}

void setupLogFile()
{
  //Initialise the SD card
  if (!SD.begin(SD_SS_PIN))
  {
    Serial.println("Error: SD card failed to initialise or is missing.");
    //Hang
  //  while (true); 
  }
  
  //Check if the file already exists
  bool oldFile = SD.exists(FILE_NAME);  
  
  //Open the file in write mode
  File logFile = SD.open(FILE_NAME, FILE_WRITE);
  
  //Add header information if the file did not already exist
  if (!oldFile)
  {
    logFile.println(LOGGERNAME);
    logFile.println(DATA_HEADER);
  }
  
  //Close the file to save it
  logFile.close();  
}


void logData(String rec)
{
  //Re-open the file
  File logFile = SD.open(FILE_NAME, FILE_WRITE);
  
  //Write the CSV data
  logFile.println(rec);
  
  //Close the file to save it
  logFile.close();  
}

String createDataRecord()
{
  //Create a String type data record in csv format
  //TimeDate, Loggertime,Temp_DS, Diff1, Diff2, boardtemp
  String data = getDateTime();
  data += ",-5,";   //adds UTC-timezone offset (5 hours is the offset between UTC and EST)
  
    
    rtc.convertTemperature();          //convert current temperature into registers
    boardtemp = rtc.getTemperature(); //Read temperature sensor value
    
    batterysenseValue = analogRead(batteryPin);
    batteryvoltage = (3.3/1023.) * 1.47 * batterysenseValue;
    
    data += currentepochtime;
    data += ",";

    addFloatToString(data, boardtemp, 3, 1);    //float   
    data += ",";  
    addFloatToString(data, batteryvoltage, 4, 2);

  
//  Serial.print("Data Record: ");
//  Serial.println(data);
  return data;
}


static void addFloatToString(String & str, float val, char width, unsigned char precision)
{
  char buffer[10];
  dtostrf(val, width, precision, buffer);
  str += buffer;
}



void CTDMeasurement(char i){    //averages 6 readings in this one loop
  CTDdepthmm = 0;
  CTDtempC = 0;
  CTDcond = 0;
   
  for (int j = 0; j < 6; j++){
  
  String command = ""; 
  command += i;
  command += "M!"; // SDI-12 measurement command format  [address]['M'][!]
  mySDI12.sendCommand(command); 
  delay(500); // wait a while
  mySDI12.flush(); // we don't care about what it sends back

  command = "";
  command += i;
  command += "D0!"; // SDI-12 command to get data [address][D][dataOption][!]
  mySDI12.sendCommand(command);
  delay(500); 
     if(mySDI12.available() > 0){
        float junk = mySDI12.parseFloat();
        int x = mySDI12.parseInt();
        float y = mySDI12.parseFloat();
        int z = mySDI12.parseInt();

      CTDdepthmm += x;
      CTDtempC += y;
      CTDcond += z;
     }
  
  mySDI12.flush(); 
     }     // end of averaging loop
     
      CTDdepthmm /= 6.0 ;
      CTDtempC /= 6.0;
      CTDcond /= 6.0;
      
      
      dataRec += ",";
      addFloatToString(dataRec, CTDdepthmm, 3, 1);
      dataRec += ",";
      addFloatToString(dataRec, CTDtempC, 3, 1);
      dataRec += ",";      
      addFloatToString(dataRec, CTDcond, 3, 1);
      //dataRec += ",";
     
}  //end of CTDMeasurement



void analogturbidity()     // function that takes reading from analog OBS3+ turbidity sensor
{ 
 int16_t adc0, adc1; //  adc2, adc3;      //tells which channels are to be read
 
  adc0 = ads.readADC_SingleEnded(0);
  adc1 = ads.readADC_SingleEnded(1);
 
//now convert bits into millivolts
 float lowvoltage = (adc0 * 3.3)/17585.0;   
 float highvoltage = (adc1 * 3.3)/17585.0;
 
  //calibration information below if only for instrument SN# S9743
 lowturbidity =  (4.6641 * square (lowvoltage)) + (92.512 * lowvoltage) - 0.38548;
 highturbidity = (53.845 * square (highvoltage)) + (383.18 * highvoltage) - 1.3555;
 
      dataRec += ",";
      addFloatToString(dataRec, lowturbidity, 3, 1);
      dataRec += ",";
      addFloatToString(dataRec, highturbidity, 3, 1);

}




void assembleURL()
{
    targetURL = "";
    targetURL = "http://somewebsite.com/somescript.php?";
    targetURL += "LoggerID=SL0xx&Loggertime=";
    targetURL += currentepochtime;
    targetURL += "&CTDdepth=";
    addFloatToString(targetURL, CTDdepthmm, 3, 1);    //float   
    targetURL += "&CTDtemp=";
    addFloatToString(targetURL, CTDtempC, 3, 1);    //float   
    targetURL += "&CTDcond=";
    addFloatToString(targetURL, CTDcond, 3, 1);    //float  
    targetURL += "&TurbLow=";
    addFloatToString(targetURL, lowturbidity, 3, 1);    //float   
    targetURL += "&TurbHigh=";
    addFloatToString(targetURL, highturbidity, 3, 1);   //float
    targetURL += "&BoardTemp=";    
    addFloatToString(targetURL, boardtemp, 3, 1);     //float 
    targetURL += "&Battery=";    
    addFloatToString(targetURL, batteryvoltage, 4, 2);     //float 
  
}


void sendviaGPRS()
{
  char buffer[10];
  if (gprsbee.doHTTPGET(APN, targetURL.c_str(), buffer, sizeof(buffer))) {
  }
}

