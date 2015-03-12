// Bee Hive Monitoring System Version 1.1 Code by Jurs - Last update: AndreasVan 2014-03-06
// Micro controller = Arduino UNO and Adafruit Data Logging shield
// Monitoring humidity and temperature in beehive
// this code is public domain, enjoy!


#include <DHT.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

#define LOGWAITTIME 60  // Number of seconds between log entries
#define CHIPSELECT 10   // ChipSelect-Pin SD-Card, (Ethernet-Shield=4)
#define REDLEDPIN A0    // Red LED on A0
#define GREENLEDPIN A1  // Green LED on A1
#define LEDTAKT 900
int greenLEDonTime;
int redLEDonTime;

DHT dhtSensor[]={ // Sensorpins
  DHT(2, DHT22), // Outside
  DHT(3, DHT22), // Inside hive 1
  DHT(4, DHT22), // Inside hive 2  
  DHT(5, DHT22), // Inside Databox
 };

#define NUM_SENS sizeof(dhtSensor)/sizeof(dhtSensor[0])

#define RTC_I2C_ADDRESS 0x68

byte bcdToDec(byte val)  // Auxiliary function for reading / writing the RTC
{ // Convert binary coded decimal to decimal number
  return ( (val/16*10) + (val%16) );
}

boolean inRange(int value, int minValue, int maxValue) // Auxiliary function for reading the RTC
{ // tests an integer value between two limits
  if (value>=minValue && value<=maxValue) return true;
  else return false;
}

boolean rtcReadTime(int &year, int &months, int &days, int &stunden, int &minuten, int &sekunden)
{ // read current time from RTC
  Wire.beginTransmission(RTC_I2C_ADDRESS);
  Wire.write(0);  // Reset the register pointer
  Wire.endTransmission();
  Wire.requestFrom(RTC_I2C_ADDRESS, 7);
  // A few of these need masks because certain bits are control bits
  sekunden    = bcdToDec(Wire.read() & 0x7f);
  minuten     = bcdToDec(Wire.read());
  stunden     = bcdToDec(Wire.read() & 0x3f);  // Need to change this if 12 hour am/pm
  /*wochentag   = */bcdToDec(Wire.read());
  days        = bcdToDec(Wire.read());
  months      = bcdToDec(Wire.read());
  year       = bcdToDec(Wire.read())+2000;  
  if (inRange(days,1,31) && inRange(months,1,12) && inRange(year,2000,2099) && inRange(minuten,0,59) && inRange(stunden,0,59) && inRange(sekunden,0,59)) return true;
  else return false;
}


char* floatFormatKomma(float f, int decimals, char* str)
{ // floating-point - between -999 and 999 format with a decimal point
  // Target buffer str have to be large enough !
  if (abs(f)>999) strcpy(str,"???.?");
  else dtostrf(f,decimals+1,decimals,str);
  char* c=strchr(str,'.');
  if (c!=NULL) c[0]=',';
  return str;
}


boolean timeToLogEntries(int &year, int &months, int &days, int &stunden, int &minuten, int &sekunden)
{ // returns "true" if it's time to sign a record
  static long lastSecond;
  rtcReadTime(year,months,days,stunden,minuten,sekunden);
  long thisSecond=stunden*3600L+minuten*60+sekunden;
  if (lastSecond/LOGWAITTIME != thisSecond/LOGWAITTIME)
  {
    lastSecond=thisSecond;
    return true;
  }
  else return false;
}

void logEntries(int year, int months, int days, int stunden, int minuten, int sekunden)
{ // Sign a record to file
  char str[18];
  digitalWrite(REDLEDPIN,HIGH);   // red LED on
  digitalWrite(GREENLEDPIN,HIGH); // green LED an
  greenLEDonTime=0;     // vorsorglich für den Fehlerfall gesetzt, normal: LEDTAKT/2;  
  redLEDonTime=LEDTAKT; // vorsorglich für den Fehlerfall gesetzt, normal: 0
  
  snprintf(str,sizeof(str),"%04d/%04d_%02d.txt",year,year,months);
  File file=SD.open(str, FILE_WRITE);
  if (!file) Serial.print(F("Error "));
  Serial.print(F("Writing File "));
  Serial.println(str);  // Dateiname mit vollständigem Pfad
  snprintf(str,sizeof(str),"%04d.%02d.%02d ",year, months, days);
  Serial.print(str);
  if (file) file.print(str);
  snprintf(str,sizeof(str),"%02d:%02d\t",stunden,minuten);
  Serial.print(str);
  if (file) file.print(str);
  for(int i=0;i<NUM_SENS;i++)
  {
    float humidity=dhtSensor[i].readHumidity();
    float temperature=dhtSensor[i].readTemperature();
    if (isnan(humidity) || isnan(temperature))
    {
      Serial.print(F("***\t***\t"));
      if (file) file.print(F("***\t***\t"));
    }
    else
    {
      floatFormatKomma(humidity,1,str);
      Serial.print(str);
      if (file) file.print(str);
      Serial.print('\t');
      if (file) file.print('\t');
      floatFormatKomma(temperature,2,str);
      Serial.print(str);
      if (file) file.print(str);
      Serial.print('\t');
      if (file) file.print('\t');
    }
  }
  Serial.println();
  if (file)
  {
    file.println();
    file.close();
    greenLEDonTime=LEDTAKT/2;  // erfolgreich geschrieben, normaler LED-Takt
    redLEDonTime=0; // erfolgreich geschrieben, rote LED soll aus bleiben
  }
}

unsigned long now(){   // Sekundenzähler
  static unsigned long prevMillis;
  static unsigned long sysTime;
  while( millis() - prevMillis >= 1000){      
    sysTime++;
    prevMillis += 1000;	
  }
  return sysTime;
}


void blinkLEDs()
{
  static unsigned long prevMillis;
  long diff=millis() - prevMillis;
  while( diff >= LEDTAKT){
    prevMillis += LEDTAKT;
    diff -= LEDTAKT;
  }
  if (diff>=greenLEDonTime)
    digitalWrite(GREENLEDPIN,LOW);
  else
    digitalWrite(GREENLEDPIN,HIGH);
  if (diff>=LEDTAKT-redLEDonTime)  
    digitalWrite(REDLEDPIN,HIGH);
  else  
    digitalWrite(REDLEDPIN,LOW);
}

void handleSerialCommand()
{ // Funktion zum Debuggen: Gibt die aktuelle Datei auf Serial aus
  char str[18];
  byte buf[64];
  if (!Serial.available()) return;
  int year,months,days,stunden,minuten,sekunden;
  rtcReadTime(year,months,days,stunden,minuten,sekunden);
  snprintf(str,sizeof(str),"%04d/%04d_%02d.txt",year,year,months);
  Serial.println();
  File file=SD.open(str, FILE_READ);
  if (!file) Serial.print("Error ");
  Serial.print("Reading File ");
  Serial.println(str);
  unsigned long remainingBytes=file.size();
  while (remainingBytes>0)
  {
    int availBytes=sizeof(buf);
    if (remainingBytes<sizeof(buf)) availBytes=remainingBytes;
    file.read(buf,availBytes);
    remainingBytes-= availBytes;
    Serial.write(buf,availBytes);
  }
  Serial.println();
  Serial.println();
  file.close();
  while (Serial.available()) Serial.read(); // Clear Serial input buffer
}

void finalErrorBlink() // Endlosschleife mit blinkenden LEDs
{ // wird im Fehlerfall von der setup-Funktion aufgerufen
  while(1)   blinkLEDs();
}

void setup() {
  pinMode(REDLEDPIN,OUTPUT);
  digitalWrite(REDLEDPIN,HIGH); // Funktionskontrolle rote LED
  pinMode(GREENLEDPIN,OUTPUT);
  digitalWrite(GREENLEDPIN,HIGH); // Funktionskontrolle grüne LED
  greenLEDonTime=LEDTAKT/2; // Normales grünes LED-Blinken halber Takt
  redLEDonTime=0; // Normal kein rotes LED-Blinken

  Serial.begin(9600); 
  Serial.print(F("Free RAM: "));Serial.println(FreeRam());
  Wire.begin();
  Serial.println(F("DHT22-Sensoren initialisieren..."));
  for (int i=0;i<NUM_SENS;i++) dhtSensor[i].begin();
  Serial.print(F("SD-Karte initialisieren... "));
  pinMode(SS,OUTPUT);
  pinMode(CHIPSELECT, OUTPUT);
  delay(1);
  if (SD.begin(CHIPSELECT)) Serial.println(F(" OK"));
  else
  {
    Serial.println(F("Kartenfehler oder SD-Karte nicht vorhanden"));
    greenLEDonTime=0;
    redLEDonTime=LEDTAKT;
    finalErrorBlink();
  }
  int year,months,days,stunden,minuten,sekunden;
  if (rtcReadTime(year,months,days,stunden,minuten,sekunden))
  {
    char str[8];
    snprintf(str,sizeof(str),"%04d",year); // Verzeichnisname = yearszahl
    if (!SD.exists(str)) SD.mkdir(str);  // legt das yearsverzeichnis an, falls es nicht existiert
    if (!SD.exists(str))
    {
      Serial.print(F("Could not create directory name "));
      Serial.println(str);
      greenLEDonTime=0;
      redLEDonTime=LEDTAKT/2;
      finalErrorBlink();
    }
    snprintf(str,sizeof(str),"%04d",year+1); // Verzeichnisname = yearszahl
    if (!SD.exists(str)) SD.mkdir(str);  // legt das nächste yearsverzeichnis an, falls es nicht existiert
  }
  else
  {
    Serial.println(F("RTC-Error"));
    greenLEDonTime=0;
    redLEDonTime=LEDTAKT/4;
    finalErrorBlink();
  }
  delay(1000);
}


unsigned long sekunde;
void loop() {
  if (now()!=sekunde)
  {
    sekunde=now();
    int year,months,days,stunden,minuten,sekunden;
    if (timeToLogEntries(year, months, days, stunden, minuten, sekunden))
      logEntries(year, months, days, stunden, minuten, sekunden);
  }
  blinkLEDs();
  handleSerialCommand();
}
