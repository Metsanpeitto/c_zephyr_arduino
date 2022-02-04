
#include <SoftwareSerial.h>
#include <Wire.h>   
#include <DS1307new.h>
//#include <OneWire.h> 

#include <serLCD.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>

const int redPin = A1;                                        // RED pin of the LED to analog pin 1 
const int bluePin = 6;                                        // BLUE pin of the LED to PWM pin 6
const int greenPin = 5;                                       // GREEN pin of the LED to PWM pin 5
const int speakerPin = 7;                                     // The piezo electric speaker
int pin = 8;                                                  // LCD data pin
serLCD lcd(pin);                                              // serLCD pin declaration
const int lcdSwitch = 9;                                      // LCD On/Off pin
const int dlSwitch = A2;                                      // DataLogger On/Off pin
const int btSwitch = A3;                                      // BlueTooth On/Off pin
#define uint8 unsigned char 
#define uint16 unsigned int
#define uint32 unsigned long int

#define RxD 4 // Pin that the Bluetooth (BT_TX) will transmit to the Arduino (RxD)
#define TxD 3 // Pin that the Bluetooth (BT_RX) will receive from the Arduino (TxD)
 
#define DEBUG_ENABLED 1

//debug data
String const DEVICE_SERIAL_NUMBER = "-------";

SoftwareSerial blueToothSerial(RxD,TxD);

int numTones = 10;
int tones[] = {261, 277, 294, 311, 330, 349, 370, 392, 415, 440};
//            mid C  C#   D    D#   E    F    F#   G    G#   A

    // ----------------------- Sleep mode--------------------------------
volatile bool awakeFlag = false; 
int wakePin = 3;                                               // Pin used for waking up
int sleepStatus = 0;                                           // Variable to store a request for sleep

   // ------------------------ DataLogger variables ---------------------------------------------------------------------------
#define disk1 0x50                         // Name of 24LC256 eeprom chip
unsigned int address = 0;                  // Number of the page
unsigned int lastAddress = 0;              // Last page writed
byte data = 333;                           // Byte to write / change for a char or string

const byte sensor_port = 0x49;                                // Hexadecimal private address of the Zephyr sensor 
const byte sensor_serial_1 = 0x4E95;                          // First part of the serial number of the Zephyr. The whole number is : 6E294A2
const byte sensor_serial_2 = 0x58FE;                          // Second part of the serial number of the Zephyr 

byte serial_code_1;                                           // Save here the  the first part of the  hexadecimal' word' that Zephyr sends using I2C communication 
byte serial_code_2;                                           // Save here the  the second part of the hexadecimal' word' that Zephyr sends using I2C communication 

bool isSerial = false;                                        // Check if there are new data in the Serial  
int value;                                                    // Store here the raw value of the Zephyr after form a word with the serial_code_1 and serial_code_2 
                                                              // and translate it to decimal
                                                              
bool readingFlag = false;                                     // Indicates if the data saving from Zephyr is going on
bool blow1Flag = true;                                        // Indicates if the data saving from Zephyr that is going on, belongs to the first blow 
bool blow2Flag = false;                                       // Indicates if the data saving from Zephyr is going on, belongs to the second blow 
bool blow3Flag = false;                                       // Indicates if the data saving from Zephyr is going on, belongs to the third blow 
bool blowsDoneFlag = false;                                   // Indicates if the data saving from Zephyr is already acomplish
 
long mStart = 0;                                              // Save the millis at the beginning of a blowing to get the duration of                                                                
long mEnd = 0;                                                // Save the millis at the end of a blowing to get the duration of 
long mTotal = 0;                                              // Save the millis obtained after supress mEnd from mStart  
long mBT1, mBT2, mBT3 = 0;                                    // Save the millis as preassigned blows 
bool mIsStarted = false;

const int numReadings = 4;                                  // The number of integers in the array 
//int readings1[numReadings];                                   // Blow1 values . If value is bigger than 1642 and correspond the first blow .
//int readings2[numReadings];                                   // Blow2 values . If value is bigger than 1642 and correspond the first blow .
//int readings3[numReadings];                                   // Blow3 values . If value is bigger than 1642 and correspond the first blow .
int readIndex = 0;                                            // The index of the current reading

int sampleBlow1[1] = {};                                       // The array of samples,selecting 1 of 16 ( Each second we obtain 32 )
int sampleBlow2[1] = {};                                       // The array of samples,selecting 1 of 16 ( Each second we obtain 32 )
int sampleBlow3[1] = {};                                       // The array of samples,selecting 1 of 16 ( Each second we obtain 32 )
int sampleCounter = 0;                                        // Save the number of samples
int sampleThreshold = 0;                                      // Save here a reference to select the right sample, 1 between 16

int blow1;                                                    // Peak value of the first blow
int blow2;                                                    // Peak value of the second blow
int blow3;                                                    // Peak value of the third blow

/*                                                            
 * 0xCCA5 = POSACK
 * 0xCC99 = BadCommand
 * 0xCC9A = BadParam
 * 0xCC9B = Failure
 * 0xCC90 = BadCheckSum
 * 0xCCBB = Busy
 */
   
boolean stringComplete = false;                                // Have we received all the data from the PC?
String  incomingByte ;                                         // For incoming serial data
String  inputString;                                           // Storage the full string
String  readingString = "";                                    // Data to log.Format Ex: dd.mm.yyyy|hh.mm.ss|blow_1|blow_2|blow_3|

String dayS = "";
String monthS = "";
String yearS = "";
String hourS = "";
String minuteS = "";                                           // Rtc strings where to save the values of the integer that RTC sends to us. Used in Lcd and datalogger
String secondS = "";
String dateS = "";
String timeS = "";

String bar = "|";                                              // Character to use when creating the datalogger string format 
String dot = ".";

String blow_1S = "1111";
String blow_2S = "2222";                                       // Strings for the blow data
String blow_3S = "3333";

int thisSecond;
int thisMinute;
int thisHour;      
int thisDay;                                                   // Integers to receive RTC ( Real Time Clock ) DS1307 data
int thisMonth;                            
int thisYear;

int i = 1 ;                                                    // Use it to access the different commands of the ENVx32 datalogger

long countDown ;    
long countDelay = 5000;                                        // Use it to ' testing ' mode
long lastActivity;                                             // Use it to compare with inactivityDelay and start sleep mode
long inactivityDelay = 20000;                                  // Use it as refference of the time to wait for an input before go sleep mode , 20 seconds
                                                             
uint16_t startAddr = 0x0000;                                   // Start address to store in the NV-RAM (CLOCK variable)
uint16_t lastAddr;                                             // New address for storing in NV-RAM    (CLOCK variable)
uint16_t TimeIsSet = 0xaa55;                                   // Helper that time must not set again  (CLOCK variable)

                                                               // Variables that will change:
int ledState = HIGH;                                           // The current state of the output pin
int buttonState = LOW;                                         // The current reading from the input pin
int lastButtonState = LOW;                                     // The previous reading from the input pin
boolean functionStart = LOW;
boolean bluetooth = false;

 //Commands for Fruxin device
    String FRUXIN_COMMAND_GET_DEVICE_SERIAL    = "gs";
    String FRUXIN_COMMAND_GET_BATTERY          = "gb";
    String FRUXIN_COMMAND_GET_DATA_SIZE        = "gds";
    String FRUXIN_COMMAND_GET_DATA             = "gd";
    String FRUXIN_COMMAND_SYNC_START           = "ss";
    String FRUXIN_COMMAND_SYNC_FINISH          = "sf";
    int dataSize;                                   
    int battery;
           
void redLed() 
   {
     analogWrite(redPin, 255);
     analogWrite(greenPin, 0);
     analogWrite(bluePin, 0);
    } 
    
void greenLed() 
   {
     analogWrite(redPin, 0);
     analogWrite(greenPin, 255);
     analogWrite(bluePin, 0);
    } 

void blueLed() 
   {
     analogWrite(redPin, 0);
     analogWrite(greenPin, 0);
     analogWrite(bluePin, 255);
    }

void noLed() 
   {
     analogWrite(redPin, 0);
     analogWrite(greenPin, 0);
     analogWrite(bluePin, 0);
    }   

void startTransmission()
  {  
     Wire.beginTransmission(sensor_port);                  // Transmit to device #73 (0x49)    device address is specified in datasheet                             
     Wire.write(0x01);                                     // Sends value byte   
     Wire.endTransmission();                               // Stop transmitting 
     Serial.println("\nstartTransmission()");
     delay(100);
   }

void readSerial()                                          // Read from Zephyr by i2c . Searchs for the Serial Number
{
  if (!isSerial)
   {
    serial_code_1 = Wire.read();
    Serial.print(serial_code_1,HEX); 
    delay(10);
    serial_code_2 = Wire.read();
    Serial.print(serial_code_2,HEX); 
    delay(35);                                              // This is the warm-up time  ,after it readings should be right    
    
  if (serial_code_2 == sensor_serial_2)
   {
      isSerial = true;
      Serial.println("\nisSerial = true;");
     }    
    Serial.println("");    
  }
}


void readData()                                            // Communicates with Zephyr . Searches for sensors readings/values
{
  if(isSerial)
  {  
     Wire.beginTransmission(sensor_port);                  // transmit to device #73 (0x49)    device address is specified in datasheet                             
     Wire.write(0);                                        // read mode  
     Wire.endTransmission();                               // stop transmitting     
     byte _data = Wire.read();
     delay(10);
     byte _data1 = Wire.read();
     delay(10);
     value = word(_data,_data1);                           // Combines the two byte in a word     
  }
}


void error()                                                                      // If value is bigger than 20000 probably is because was a bad reading,in this 
   {                                                                              // case an error function it's gonna run,warning the user with red light , beeps  
     redLed();                                                                    // from the piezo electric, LCD and Serial texts . 
     lcd.clear();
     lcd.print("     ERROR ");
     delay(1000);
     noLed();
     lcd.clear();
     delay(500); 
     redLed();
     lcd.print("     ERROR ");
     delay(1000);
     lcd.clear();
     lcd.print("    Try again ");
     noLed();
     blow1Flag = true;                                                            // Every variable it's reseted,letting all ready to a new serie of blow readings
     blow2Flag = false;
     blow3Flag = false;
     readIndex = 0;
     value = 0;
     blow1 = 0;
     blow2 = 0;
     blow3 = 0;
     blow_1S = ("");
     blow_2S = ("");
     blow_3S = ("");
     Serial.println("ERROR");
     Serial.println("Try again");
    }
     

void saveBlowData()
   {
 if (value > 1653)                                                                   // It's gonna start if there are a positive variation in 'value' 
  {         
    if ( value > 20000 ) { error();}   
    else 
     {
       Serial.print("The value higher than 1653 is "); 
       Serial.println(value);                              
       greenLed();
       tone(7, 440, 10);
       delay(10);                                                                    // turn off tone function for pin 6:                     
       noTone(3);   
     
      if (readingFlag == true)                                                       // True if reading mode On               
         {
          Serial.println("Reading flag true "); 
          if (blow1Flag == true )                                                    // First of the arrays where to save the values
            { 
             Serial.print("Readin index at the beginning =  "); 
             Serial.println(readIndex); 
             if (readIndex == 0)                                                     // If its the first value to save is the maximun until now,save it as blow1  
              { 
                 Serial.print("in blow1,readIndex = 0  ");                 
                 blow1 = value;
                 mStart = millis();                                                   // Start counting the time to take the samples       
                 sampleBlow1[0] = blow1;                                              // The first integer of the array of samples
                 sampleCounter ++ ;                                                   // Increase the counter of saved samples 
                 sampleThreshold = sampleThreshold + 16 ;                             // Increase 16 the threshold until save a new sample 
                 readIndex = 1;                                                         // Advance to the next position in the array:                 
                 Serial.print("Readin index at blow1 / readinIndex = 0");
                 Serial.println(readIndex);                                   
                 }
                         
             if ( readIndex == sampleThreshold )                                      // The number of readings matchs 
              {
                 readIndex++;                                                         // Advance to the next position in the array:              
                 Serial.print("Readin index at  blow1 / sapleThreshold = 0");
                 Serial.println(readIndex);                  
                 sampleBlow1[sampleCounter] = blow1;                                  // Samplecounter says where each sample is stored                                
                 sampleCounter++;                                                     // Increase one the number of saved samples
                 sampleThreshold+16;                                                  // Increase 16 the number of samples that wait before save again
               }                
                                                                   
             if (value > blow1)  
              {                  
                 blow1 = value;
                 readIndex++;                                                         // Advance to the next position in the array:             
                 Serial.print("Readin index at blow1 / readinIndex = 0");
                 Serial.println(readIndex);   
                }                                                                     // Each value bigger than last maximun is saved as blow1                      
                 
                 //readings1[readIndex] = value;                                        // Readings1 holds all the values of the first blow                                                         
                 //readIndex++;                                                         // Advance to the next position in the array:                 
                 Serial.print("Readin index at the end of blow1 =  "); 
                 Serial.println(readIndex);        
        } else 
        

          if (blow2Flag == true ) 
           {              
             if (readIndex == 0) 
              {  blow2 = value;
                 mStart = millis();
                 sampleBlow1[0] = blow1;                                              // The first integer of the array of samples
                 sampleCounter ++ ;                                                   // Increase the counter of saved samples 
                 sampleThreshold = sampleThreshold + 16 ;                             // Increase 16 the threshold until save a new sample
                 readIndex = 1;                                                         // Advance to the next position in the array:                 

                }
                         
             if ( readIndex == sampleThreshold )                                      // The number of readings matchs 
              {
                 readIndex++;  
                 sampleBlow2[sampleCounter] = blow2;                                  // Samplecounter says where each sample is stored                                
                 sampleCounter++;                                                     // Increase one the number of saved samples
                 sampleThreshold+16;                                                  // Increase 16 the number of samples that wait before save again
                }              
              
             
             if (value > blow2)  
              {  
                 readIndex++; 
                 blow2 = value; 
                 }                 
                                                              
                 // readings2[readIndex] = value;                                                            
                 // readIndex = readIndex + 1; 
              } else   
               
          if  (blow3Flag == true ) 
           {   
              if (readIndex == 0) 
               { blow3 = value; 
                 mStart = millis();
                 sampleBlow3[0] = blow1;                                              // The first integer of the array of samples
                 sampleCounter ++ ;                                                   // Increase the counter of saved samples 
                 sampleThreshold = sampleThreshold + 16 ;                             // Increase 16 the threshold until save a new sample
                 readIndex = 1;            
                 }
                         
              if ( readIndex == sampleThreshold )                                      // The number of readings matchs 
              { 
                 readIndex++; 
                 sampleBlow1[sampleCounter] = blow1;                                  // Samplecounter says where each sample is stored                                
                 sampleCounter++;                                                     // Increase one the number of saved samples
                 sampleThreshold+16;                                                  // Increase 16 the number of samples that wait before save again
                
                }  
                        
              if (value > blow3)
               {                 
                 readIndex++;  
                 blow3 = value; 
               }                                             
             
                // readings3[readIndex] = value;  
                //readIndex = readIndex + 1;                                                     
              }
                                                                                     // If we're at the end of the array...
          if (readIndex >= numReadings) 
           {                                                                         // ...wrap around to the beginning:
              readIndex = 0;
            }
             Serial.print("Read Index at the end = ");
             Serial.println(readIndex);
       } 
     }         
   } else                                // When values falls bellow 1640 a blow reading is done ,then let ready readIndex to next 
        {        
          Serial.println("Less than 1653 !!");
          Serial.print("Read Index (less than 1653)  = ");
          Serial.println(readIndex);
          
          if    ( readIndex > 1 )                                               
           {                   
                  Serial.print("Read index out = ");
                  Serial.println(readIndex);
                   
              if (blow1Flag == true ) 
               {  
                  mEnd = millis();                                                 // mEnd saves the millis when the blow has gave a value lower than the threshold
                  mTotal = (mEnd - mStart) ;                                       // mTotal is the time between the record starts and ends
                  mEnd = 0;                                                        // Reset to 0 the values and ready for next blow recording
                  mStart = 0;
                  mBT1 = mTotal;                                                   // mBT1 , milliseconds for blow 1 
                  
                  Serial.print(sampleCounter);
                  Serial.println(" samples");
                  Serial.print("last value was ");
                  Serial.println(value);
                  blow1Flag = false; 
                  blow2Flag = true;                                                // If value lowest than threshold change flags to next blow reading 
                  Serial.println();
                  Serial.println("Blow 1 done !");                  
                  Serial.print(mBT1);                                  
                  Serial.println(" Milliseconds ");
                  Serial.print("The peak of the last blow was  ");
                  Serial.println(blow1);                                    
                  Serial.println("Ready for the next record,please blow ");
                  Serial.println("    ");                  
                  lcd.clear();
                  lcd.print("Blow 1 done");
                  lcd.setCursor(0,2);
                  lcd.print("Peak was");
                  lcd.setCursor(9,2);
                  lcd.print(blow1);
                  delay(2000);
                  lcd.noDisplay();
                  delay(500);                  
                  lcd.display();
                  delay(2000);
                  lcd.clear();
                  lcd.print(mBT1);
                  lcd.print("    Ms");
                  delay(2000);
                  lcd.clear();
                  lcd.print("Waiting new blow");
                  noLed();
                  greenLed();
                  readIndex = 0;
                  lastActivity = millis();                  
                }
             
              if ( (blow2Flag == true ) && ( readIndex != 0 ) )
               { 
                  mEnd = millis();                                                 // mEnd saves the millis when the blow has gave a value lower than the threshold
                  mTotal = (mEnd - mStart) ;                                       // mTotal is the time between the record starts and ends
                  mEnd = 0;                                                        // Reset to 0 the values and ready for next blow recording
                  mStart = 0;
                  mBT2 = mTotal;                                                   // mBT1 , milliseconds for blow 2 
                  Serial.print("last value was ");
                  Serial.println(value);
                  blow2Flag = false; 
                  blow3Flag = true;
                  Serial.println("Blow 2 done !");
                  Serial.print(mBT2);
                  Serial.println(" Milliseconds ");
                  Serial.print("The peak of the last blow was ");
                  Serial.println(blow2);
                  Serial.println("Ready for the next record,please blow ");
                  Serial.println("    ");               

                  lcd.clear();
                  lcd.print("Blow 2 done");
                  lcd.setCursor(0,2);
                  lcd.print("Peak was");
                  lcd.setCursor(9,2);
                  lcd.print(blow2);
                  delay(2000);                  
                  lcd.noDisplay();
                  delay(500);                  
                  lcd.display();
                  delay(2000);
                  lcd.clear();
                  lcd.print(mBT2);
                  lcd.print("    Ms");
                  delay(2000);
                  lcd.clear();
                  lcd.print("Waiting new blow");                 
                  noLed();
                  greenLed();
                  readIndex = 0;
                  lastActivity = millis();
                 }
             
              if ( (blow3Flag == true ) && ( readIndex != 0 ) )                                    // If value falls bellow threshold after blow 3 stop the reading mode
               { 
                  mEnd = millis();                                                 // mEnd saves the millis when the blow has gave a value lower than the threshold
                  mTotal = (mEnd - mStart) ;                                       // mTotal is the time between the record starts and ends
                  mEnd = 0;                                                        // Reset to 0 the values and ready for next blow recording
                  mStart = 0;
                  mBT3 = mTotal;                                                   // mBT1 , milliseconds for blow 3 
                  
                  Serial.print("last value was ");
                  Serial.println(value);
                  blow3Flag = false; 
                  blow1Flag = true;                                                                // Let ready to start in blow1 next time
                  blowsDoneFlag = true;
                  Serial.println("Blow 3 done !");
                  Serial.print(mBT3);
                  Serial.print("0");
                  Serial.println(" Milliseconds ");
                  Serial.print("The peak of the last blow was ");
                  Serial.println(blow3);
                  Serial.println("Ready for the next record,please blow ");
                  Serial.println("    ");
                  
                  lcd.clear();
                  lcd.print("Blow 3 done");
                  lcd.setCursor(0,2);
                  lcd.print("Peak was");
                  lcd.setCursor(9,2);
                  lcd.print(blow1);
                  delay(2000);
                  lcd.noDisplay();
                  delay(500);                  
                  lcd.display();
                  delay(2000);
                  lcd.clear();
                  lcd.print(mBT3);
                  lcd.print("    Ms");
                  delay(2000);
                  lcd.clear();
                  lcd.print("  Record ended");  
                  delay(1000); 
                  lcd.clear();
                  lcd.print("  Record ended");  
                  delay(1000);
                  lcd.clear();                    
                  noLed();
                  greenLed();
                  readIndex = 0;
                  lastActivity = millis();
                  readingFlag = false;
                  blowsDoneFlag = true;
                  Serial.println("Quit reading mode"); 
                }  
              }  
           }
        }


void calculateFlow(int value)
   { 
     float fullScaleFlow = 1;                                                           // With 0x49 the flow range is 300 Standard liter per minute
     float flow = fullScaleFlow * ((value/16384) - 0.1) / 0.8;  
   //Serial.println("\ncalculateFlow()");
   //Serial.print("Flow is ");
     Serial.println(flow); 
     Serial.println("");  
    }

 /////////////////////////////////////////////////// --- ---  RTC-DS1307 Functions & Commands  --- ---   \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

 
 void showClock()                                        // Refresh and shows the rtc data       
      {       
        RTC.getTime();                                   // Refresh the data 
        thisYear = (int)RTC.year;                       
        thisMonth = (int)RTC.month;
        thisDay = (int)RTC.day;
        thisHour = (int)RTC.hour;                        // Read as integers thos values and save them in ints
        thisMinute =(int)RTC.minute;  
        thisSecond =(int)RTC.second;
        dateS = "";
        timeS = "";                                      // Strings where to locate the parts of the string format for the ENVx32
        readingString  = "";
        dateS += thisDay;
        dateS += "/" ; 
        dateS += thisMonth;
        dateS += "/" ;
        dateS += thisYear;
        dateS += "/" ;
        timeS += thisHour;                               // Building the strings
        timeS += ":";
        timeS += thisMinute;
        timeS += ":";
        timeS += thisSecond;
        readingString += dateS;
        readingString += timeS;
     
    Serial.print("thisHour/thisMinute : ");              // Shows the time
    if  ((thisHour < 10) && (thisMinute < 10)) 
     {
        Serial.print("0");
        Serial.print(thisHour);
        Serial.print(":");
        Serial.print("0");
        Serial.println(thisMinute);      
        Serial.print("Clock is  ");
        Serial.print(RTC.hour,DEC);
        Serial.print(":");
        Serial.println(RTC.minute,DEC);      
      }

     if ((thisHour < 10) && (thisMinute >= 10)) 
      {
        Serial.print("0");
        Serial.print(thisHour);
        Serial.print(":");        
        Serial.println(thisMinute);      
        Serial.print("Clock is  ");
        Serial.print(RTC.hour,DEC);
        Serial.print(":");
        Serial.println(RTC.minute,DEC);        
      }

     if ((thisHour >= 10) && (thisMinute >= 10)) 
      {
        Serial.print(thisHour);
        Serial.print(":");        
        Serial.println(thisMinute);      
        Serial.print("Clock is  ");
        Serial.print(RTC.hour,DEC);
        Serial.print(":");
        Serial.println(RTC.minute,DEC);        
       }       
          
     lcd.clear();     
    }                                  


     
 void writeEEPROM(int deviceaddress, unsigned int eeaddress, byte data )        // Write string *date | time | blow1 | blow2 | blow3 into the dataLogger       
{
  blueLed();
  String dataString = readingString;
  Serial.print("Write :");
  Serial.println(dataString);
  Wire.beginTransmission(deviceaddress);
  Wire.write((int)(eeaddress >> 8));   // MSB
  Wire.write((int)(eeaddress & 0xFF)); // LSB
  Wire.write(data);
  Wire.endTransmission();
  Serial.println("Data logged");  
  lcd.clear();
  lcd.print("Data logged");
  delay(1000);
  lcd.clear();
  noLed();    
  delay(5);
}
                         
byte readEEPROM(int deviceaddress, unsigned int eeaddress )                  // Function to read from eeprom
{
  byte rdata = 0xFF;
  Serial.println("read Eeprom");
  Wire.beginTransmission(deviceaddress);
  Wire.write((int)(eeaddress >> 8));   // MSB
  Wire.write((int)(eeaddress & 0xFF)); // LSB
  Wire.endTransmission();
 
  Wire.requestFrom(deviceaddress,1);
 
  if (Wire.available()) rdata = Wire.read();
  Serial.print("rdata = ");
  Serial.println(rdata);
  return rdata;  
} 

 void eraseEEPROM(int deviceaddress, unsigned int eeaddress, byte data )        // Erase the whole eeprom
{ redLed();
  
  Serial.print("Erasing :");
  lcd.clear();
  lcd.print("Erasing");
  for (int i = 0;i < 64 ; i++) 
  {
     eeaddress = i;
     Wire.beginTransmission(deviceaddress);
     Wire.write((int)(eeaddress >> 8));   // MSB
     Wire.write((int)(eeaddress & 0xFF)); // LSB
     Wire.write(0xff);
     Wire.endTransmission();
     delay(5);
    }

  Serial.println("Data erased");  
  lcd.clear();
  lcd.print("Data erased");
  delay(1000);
  lcd.clear();
  noLed();    
  
}
  

void sleepNow()                                                                  // Here we put the arduino to sleep
   {    
    Serial.println("In sleepNow function");
    noLed(); 
    //set_sleep_mode(SLEEP_MODE_PWR_SAVE);                                       // Sleep mode is set here
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);                                         // sleep mode is set here  
    sleep_enable();                                                              // Enables the sleep bit in the mcucr register
    attachInterrupt(digitalPinToInterrupt(3),wakeUpNow, LOW);                    // Use interrupt 0 (pin 2) and run function                                                           
                               
      Serial.println("Sleeping now");
      delay(1000);   
      digitalWrite(dlSwitch,LOW);                                             // Turns On the ENVx32                                (TASK step 1)
      delay(200);     
      digitalWrite(btSwitch,LOW);                                               // Turns On the Lcd
      delay(200);
      readIndex = 0;    
      value = 0;
      blow1 = 0;
      blow2 = 0;
      blow3 = 0;
      blow_1S = ("");
      blow_2S = ("");
      blow_3S = ("");
      readingString = ("");        
          
      sleep_mode();
        // THE PROGRAM CONTINUES FROM HERE AFTER WAKING UP    
      //sleep_disable();                                                       
      //detachInterrupt(digitalPinToInterrupt(2));                                // Disables interrupt 0 on pin 2 so the
      //noInterrupts();                                                           // wakeUpNow code will not be executed
                                                                                  // during normal running time.  
      Serial.println("Awake again");                                                                      
  }


void wakeUpNow()
   {                                                          
     detachInterrupt(digitalPinToInterrupt(3));                                  // Disables interrupt 0 on pin 2 so the
     noInterrupts();
     sleep_disable();        
     //Serial.println("wakeUpNow function");
     awakeFlag = true;     
    }

void buildString()                                                               // Builds the string with  *date | time | blow1 | blow2 | blow3
   {         
     blow_1S = ((String)blow1);                                                  // The values stored as int (or long) must be transformed to string                                
     blow_2S = ((String)blow2);
     blow_3S = ((String)blow3);
     readingString = "";                                                         // Reinitialize the string data                           
     readingString += "*";                                                       // The string must start with ' * '
     readingString += dateS;                                                     // The string with the date 
     readingString += bar;                                                       // Adds the character ' | '
     readingString += timeS;                                                     // The string with the hour,minute,second
     readingString += bar;                                                       
     readingString += blow_1S;                                                   // Adds the string where the value of blow 1 
     readingString += bar;
     readingString += blow_2S;
     readingString += bar;
     readingString += blow_3S;          
     Serial.print("The data to log in -> ");
     Serial.println(readingString);
     lastActivity = millis();   
   }  
   
void lcdon() 
{
    digitalWrite(lcdSwitch,HIGH);
    Serial.print("LCD screen on");
    lcd.clear();
    lcd.print("LCD screen on");   
    delay(1000); 
  }

void lcdoff() 
{
    digitalWrite(lcdSwitch,LOW);
    Serial.print("LCD screen off");
    lcd.clear();
    lcd.print("LCD screen off");  
    delay(1000);   
 }
 
 void speakerTest() 
     {
      tone(7, 440, 200);
      delay(200);                                                 // turn off tone function for pin 6:                     
      noTone(7);
      tone(7, 640, 200);
      delay(200);                                                 // turn off tone function for pin 6:                     
      noTone(7);
      delay(250); 
      }

 //setup the bluetooth shield
void setupBlueToothConnection()
{
  blueToothSerial.begin(38400); //Set BluetoothBee BaudRate to default baud rate 38400
   blueToothSerial.print("\r\n+AT ROLE=0\r\n"); //set the bluetooth work in slave mode
   blueToothSerial.print("\r\n+AT NAME=Fruxin\r\n"); //set the bluetooth 
   blueToothSerial.print("\r\n+AT PARI=1\r\n"); // Permit Paired device to connect me
  // blueToothSerial.print("\r\n+STAUTO=0\r\n"); // Auto-connection should be forbidden here
   delay(2000); // This delay is required.  
}


void setup()
 {
        
  pinMode(redPin, OUTPUT);                                    // Set the redLed pwm pin as output
  pinMode(greenPin, OUTPUT);                                  // Set the green  pwm pin as output
  pinMode(bluePin, OUTPUT);                                   // Set the blue pwm pin as output
  pinMode(speakerPin,OUTPUT);                                 // Set the button pin as output
  pinMode(lcdSwitch,OUTPUT);                                  // Set the lcd switch pin as output
  pinMode(btSwitch,OUTPUT);                                   // Set the bluetothSwitch pin as an output
  pinMode(dlSwitch,OUTPUT);                                   // Set the dataLoggerSwitch pin as an output
  pinMode(wakePin, INPUT);                                    // Set the wake up pin as an input to read 
  pinMode(RxD, INPUT);                                        // Setup the Arduino to receive INPUT from the bluetooth shield on Digital Pin 6
  pinMode(TxD, OUTPUT);                                       // Setup the Arduino to send data (OUTPUT) to the bluetooth shield on Digital Pin 7
  
  Serial.begin(9600);  
  Serial.println("- Setup -"); 
  digitalWrite(lcdSwitch,HIGH);                               // Test the lcd  
  delay(3000);
  Wire.begin(); 
  delay(1000);
  lcd.clear();
  lcd.print("  -  Setup  -  ");
  delay(1000);
  lcd.clear();
  lcd.print("  Press button");
  lcd.setCursor(0,2);
  lcd.print("    to start"); 
  delay(1000);
  lcd.clear(); 
  delay(1000);
  
  tone(7, 440, 200);
  delay(200);                                                 // turn off tone function for pin 6:                     
  noTone(7);
  tone(7, 640, 200);
  delay(200);                                                 // turn off tone function for pin 6:                     
  noTone(7);
  delay(250); 

  blueLed();
  lastActivity = millis();
  
  //writeEEPROM(disk1, address, 555);                           // Write in the eeprom
  //Serial.println(readEEPROM(disk1, address), DEC);            // Read and print from the eeprom
 

                                                                 // Initialize all the readings to 0
for (int thisReading = 0; thisReading < numReadings; thisReading++) 
  {
    /* readings1[thisReading] = 0;
     readings2[thisReading] = 0;
     readings3[thisReading] = 0;
    */}                                                    
  
    startTransmission();                                          // Start i2c communication with the Zephyr sensor       

     // ------------------------------------- Wake up ------ Reading mode ----------------------
  /* Enable an interrupt. In the function call
   * attachInterrupt(A, B, C)
   * A   can be either 0 or 1 for interrupts on pin 2 or 3.  
   *
   * B   Name of a function you want to execute while in interrupt A.
   *
   * C   Trigger mode of the interrupt pin. can be:
   *             LOW        a low level trigger
   *             CHANGE     a change in level trigger
   *             RISING     a rising edge of a level trigger
   *             FALLING    a falling edge of a level trigger
   *
   * In all but the IDLE sleep modes only LOW can be used.
   */     
    // sei();                                                                              // Enable global interrupts
    //  EIMSK |= (1 << INT0);                                                              // Enable external interrupt INT0
                                                               
    attachInterrupt(digitalPinToInterrupt(2),wakeUpNow, FALLING);                          // Use interrupt 1 (pin 3) and run function
                                                                                           // WakeUpNow when pin 2 gets LOW   
    //attachInterrupt(0,wakeUpNow, FALLING);                                               // Use interrupt 0 (pin 2) and run function     
   // sleepNow();

 /////////////////////////////////////////                        RTC   DS1307  configuration                            \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\   
                 
  /* PLEASE NOTICE: WE HAVE MADE AN ADDRESS SHIFT FOR THE NV-RAM!!
                  NV-RAM ADDRESS 0x08 HAS TO ADDRESSED WITH ADDRESS 0x00=0
                  TO AVOID OVERWRITING THE CLOCK REGISTERS IN CASE OF
                  ERRORS IN YOUR CODE. SO THE LAST ADDRESS IS 0x38=56!
*/
  RTC.setRAM(0, (uint8_t *)&startAddr, sizeof(uint16_t));        // Store startAddr in NV-RAM address 0x08   
/*
   Uncomment the next 2 lines if you want to SET the clock
   Comment them out if the clock is set.
   DON'T ASK ME WHY: YOU MUST UPLOAD THE CODE TWICE TO LET HIM WORK
   AFTER SETTING THE CLOCK ONCE.
*/
  // TimeIsSet = 0xffff;
  // RTC.setRAM(54, (uint8_t *)&TimeIsSet, sizeof(uint16_t));  
/*
  Control the clock.
  Clock will only be set if NV-RAM Address does not contain 0xaa.
  DS1307 should have a battery backup.
*/
  RTC.getRAM(54, (uint8_t *)&TimeIsSet, sizeof(uint16_t));
  if (TimeIsSet != 0xaa55)
  {
    RTC.stopClock();        
    RTC.fillByYMD(2016,2,3);
    RTC.fillByHMS(15,30,0);    
    RTC.setTime();
    TimeIsSet = 0xaa55;
    RTC.setRAM(54, (uint8_t *)&TimeIsSet, sizeof(uint16_t));
    RTC.startClock();
  }
  else
  {
    RTC.getTime();
  } 
/*
   Control Register for SQW pin which can be used as an interrupt.
*/
  RTC.ctrl = 0x00;                                                // 0x00=disable SQW pin, 0x10=1Hz,
                                                                  // 0x11=4096Hz, 0x12=8192Hz, 0x13=32768Hz
  RTC.setCTRL();
  uint8_t MESZ;
  MESZ = RTC.isMEZSummerTime();
  
  noLed();
  showClock();  
  readingFlag = true;
  sleepNow();   
 }


/////////////////////////////                                              LOOP                                                                 /////////////////////////////////

 void loop()
    { 
      if ( awakeFlag == true ) 
       {   Serial.begin(9600);          
           Serial.println("In the loop");       
           greenLed();             
           digitalWrite(lcdSwitch,HIGH);                                         // Turns On the Lcd
           delay(200);    
           readingFlag = true;                                                   
           lastActivity = millis();
/*           
String L1 = "L1";                                              
String L0 = "L0";  
String D = "D";
String R0 = "R0"; 
String R1 = "R1";                                              // Zephyr Commands ( Explained bellow in their respectives functions )
String E = "E";
String M = "M" ;
String I = "I" ;  
  */        
           Serial.println(" Waiting user ");
           lcd.print("- Waiting user -"); 
           setupBlueToothConnection() ;                        
           awakeFlag = false;      
          }
      
      if ( readingFlag == true ) 
      {    
              Wire.requestFrom(sensor_port, 2);                                  // ( Request from address 0x49 , 2 bytes )                  
       while (Wire.available())
           {                                                                         
               readSerial();                                                     //  Request the Zephyr's Serial number and receives it in a 'word' of two hex values in 2 bytes                                     
               readData();                                                       //  Read the HEX word and hold it as an integer named 'value' 
               saveBlowData();
              }    
          } else 
                {  
                  if ( blowsDoneFlag == true )                                   // After record the values of three blows ... 
                    {      
                       showClock();                                              // Refresh time                       
                       buildString();                                            // Build the string *date | time | blow1 | blow2 | blow3
                      // writeData();                                            // Write the string in the memory                                          
                       blowsDoneFlag = false;                                    // Set the flag down and ready to start blow1   
                       bluetooth = true;                                         // Bluetooth must to send the data yet              
                       noLed();                                                              
                       Serial.println("Blows done,restart reading interrupt");
                       if (bluetooth == false)                                   // Bluetoot has done his task ,then allow sleep mode 
                       {
                        sleepNow();                                             
                       }                      
                      }
             }
    if ( bluetooth == true )   // 
        {
          
          char readChar; 

    if(blueToothSerial.available()) //check if there's any data sent from the remote bluetooth shield
    { 
       readChar = blueToothSerial.read();
       Serial.print(readChar); // Print the character received to the Serial Monitor (#debug it)

       //debug: send Fruxin serial data when request is made!
       if(readChar == 'gs')
       {
        //This will send value obtained (recvChar) to the phone. The value will be displayed on the phone.
        blueToothSerial.print(DEVICE_SERIAL_NUMBER);
        }
        // This will get battery info
       if (readChar == 'gb')  
       {
        blueToothSerial.print(battery);                                            // This feautre is not developed yet
       }
        // This will get the data size
        if (readChar == 'gds')  
       {
        blueToothSerial.print(dataSize);                                                 // Is it not always the same package size??
       }
        if (readChar == 'gd')                                                                // Here we send the data string to bluetooth                                                    
       {
        blueToothSerial.print(data);
        bluetooth = false;
       }
       
       }
       }
      
    }      
     
