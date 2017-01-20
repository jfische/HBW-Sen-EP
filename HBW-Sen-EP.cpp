//*******************************************************************
//
// HBW-Sen-EP.cpp
//
// Homematic Wired Hombrew Hardware
// Arduino Uno als Homematic-Device
// HBW-Sen-EP zum Zaehlen von elektrischen Pulsen (z.B. S0-Schnittstelle)
//
//*******************************************************************


/********************************/
/* Pinbelegung: 				*/
/********************************/

#define BUTTON 8            // Das Bedienelement
#define RS485_TXEN 2        // Transmit-Enable
#define Sen1 A1				      // 14 = A0 als digitaler Eingang
#define Sen2 A0
#define Sen3 A3
#define Sen4 A2
#define Sen5 A4
#define Sen6 A5
#define Sen7 3
#define Sen8 7

#define IDENTIFY_LED 12         // Identify LED --> zusÃ¤tzliche LED fÃ¼r Identifizierung in Verteilung
#define IDENTIFY_EEPROM 0xFF    // Adresse EEPROM Identify 


#define DEBUG_NONE 0   // Kein Debug-Ausgang, RS485 auf Hardware-Serial
#define DEBUG_UNO 1    // Hardware-Serial ist Debug-Ausgang, RS485 per Soft auf pins 5/6
#define DEBUG_UNIV 2   // Hardware-Serial ist RS485, Debug per Soft auf pins 5/6

#define DEBUG_VERSION DEBUG_UNO

#if DEBUG_VERSION == DEBUG_UNO
  #define RS485_RXD 5
  #define RS485_TXD 6
  #define LED 13        // Signal-LED
#elif DEBUG_VERSION == DEBUG_UNIV
  #define DEBUG_RXD 5
  #define DEBUG_TXD 6
  #define LED 4
#else
  #define LED 4         // Signal-LED
#endif

#define POLLING_TIME 10	    // S0-Puls muss laut Definition mindestens 30ms lang sein - Abtastung mit 10ms soll ausreichen

// Do not remove the include below
#include "HBW-Sen-EP.h"

#if DEBUG_VERSION == DEBUG_UNO || DEBUG_VERSION == DEBUG_UNIV
// TODO: Eigene SoftwareSerial
#include <SoftwareSerial.h>
#endif

// debug-Funktion
#include "HMWDebug.h"

// EEPROM
#include <EEPROM.h>

// HM Wired Protokoll
#include "HMWRS485.h"
// default module methods
#include "HMWModule.h"


#include "HMWRegister.h"

#include <TimerOne.h>


// Das folgende Define kann benutzt werden, wenn ueber die
// Kanaele "geloopt" werden soll
// als Define, damit es zentral definiert werden kann, aber keinen (globalen) Speicherplatz braucht
#define CHANNEL_PORTS byte channelPorts[HMW_CONFIG_NUM_COUNTERS] = {Sen1, Sen2, Sen3, Sen4, Sen5, Sen6, Sen7, Sen8};

byte currentPortState[HMW_CONFIG_NUM_COUNTERS];
byte oldPortState[HMW_CONFIG_NUM_COUNTERS];

long currentCount[HMW_CONFIG_NUM_COUNTERS];
long lastSentCount[HMW_CONFIG_NUM_COUNTERS];
long lastSentTime[HMW_CONFIG_NUM_COUNTERS];

long currentWh[HMW_CONFIG_NUM_COUNTERS];
int countInTime[HMW_CONFIG_NUM_COUNTERS];

long lastPortReadTime;

int quotientWh[HMW_CONFIG_NUM_COUNTERS];
byte channelWh;
byte channelInTime;

int alle_x_sekunden=6;
int IntCount = 0;
long countLeistung[HMW_CONFIG_NUM_COUNTERS];
long currentLeistung[HMW_CONFIG_NUM_COUNTERS];

#if DEBUG_VERSION == DEBUG_UNO
SoftwareSerial rs485(RS485_RXD, RS485_TXD); // RX, TX
HMWRS485 hmwrs485(&rs485, RS485_TXEN);
#elif DEBUG_VERSION == DEBUG_UNIV
HMWRS485 hmwrs485(&Serial, RS485_TXEN);
#else
HMWRS485 hmwrs485(&Serial, RS485_TXEN);  // keine Debug-Ausgaben
#endif
HMWModule* hmwmodule;   // wird in setup initialisiert


hmw_config config;


// write config to EEPROM in a hopefully smart way
void writeConfig(){
//    byte* ptr;
//    byte data;
	// EEPROM lesen und schreiben
//	ptr = (byte*)(sensors);
//	for(int address = 0; address < sizeof(sensors[0]) * CHANNEL_IO_COUNT; address++){
//	  hmwmodule->writeEEPROM(address + 0x10, *ptr);
//	  ptr++;
//    };
};




void setDefaults(){
  // defaults setzen
  if(config.logging_time == 0xFF) config.logging_time = 20;
  if(config.central_address == 0xFFFFFFFF) config.central_address = 0x00000001;

  for(byte channel = 0; channel < HMW_CONFIG_NUM_COUNTERS; channel++){
    if(config.counters[channel].send_delta_count == 0xFFFF) config.counters[channel].send_delta_count = 1;
    if(config.counters[channel].send_min_interval == 0xFFFF) config.counters[channel].send_min_interval = 10;
    if(config.counters[channel].send_max_interval == 0xFFFF) config.counters[channel].send_max_interval = 150;
    if(config.counters[channel].impulse_pro_einheit == 0xFF) config.counters[channel].impulse_pro_einheit = 1000;
  };
};

// After writing the central Address to the EEPROM, the CCU does
// not trigger a re-read config
unsigned long centralAddressGet() {
  unsigned long result = 0;
  for(int address = 2; address < 6; address++){
	result <<= 8;
	result |= EEPROM.read(address);
  };
  return result;
};


// Klasse fuer Callbacks vom Protokoll
class HMWDevice : public HMWDeviceBase {
  public:

	void setLevel(byte channel,unsigned int level) {
	      return;  // there is nothing to set
	}



	unsigned int getLevel(byte channel, byte command) {
		// everything in the right limits?
		if(channel >= HMW_CONFIG_NUM_COUNTERS) return 0;
		return currentCount[channel];
	};



	void readConfig(){
		byte* ptr;
		// EEPROM lesen
		ptr = (byte*)(&config);
		for(int address = 0; address < sizeof(config); address++){
			*ptr = EEPROM.read(address + 0x01);
			ptr++;
		};

		// defaults setzen, falls nicht sowieso klar
		setDefaults();
	};


};

// The device will be created in setup()
HMWDevice hmwdevice;



void factoryReset() {
  // writes FF into config
//  memset(sensors, 0xFF, sizeof(sensors[0]) * CHANNEL_IO_COUNT);
  // set defaults
//  setDefaults();
EEPROM.write(IDENTIFY_EEPROM,00);
hmwdebug("\n");
hmwdebug("factoryReset\n");
}


void handleButton() {
  // langer Tastendruck (5s) -> LED blinkt hektisch
  // dann innerhalb 10s langer Tastendruck (3s) -> LED geht aus, EEPROM-Reset

//-----------------------------------------------------------------------
  // Erweiterung fÃ¼r Identity LED
  // Identity Status und EEPROM Adresse 255 lesen 
  static byte IDENTY;
  static long last_IDENT_LED_time;
  long now = millis();

  IDENTY = EEPROM.read(IDENTIFY_EEPROM);      //EEPROM Adresse 255 lesen 

  if (IDENTY == 0x00) {
     digitalWrite(IDENTIFY_LED, LOW);
     last_IDENT_LED_time = now;
     } 
  else {
     if(now - last_IDENT_LED_time > 600) { 
      digitalWrite(IDENTIFY_LED,!digitalRead(IDENTIFY_LED));  //Toggel LED
      last_IDENT_LED_time = now;                              // ZÃ¤hler zurÃ¼cksetzen
      }
   }

//-----------------------------------------------------------------------

  static long lastTime = 0;
  static byte status = 0;  // 0: normal, 1: Taste erstes mal gedrÃ¯Â¿Â½ckt, 2: erster langer Druck erkannt
                           // 3: Warte auf zweiten Tastendruck, 4: Taste zweites Mal gedrÃ¯Â¿Â½ckt
                           // 5: zweiter langer Druck erkannt

 // long now = millis();
  boolean buttonState = !digitalRead(BUTTON);


  switch(status) {
    case 0:
      if(buttonState) {status = 1;  hmwdebug(status);}
      lastTime = now;
      break;
    case 1:
      if(buttonState) {   // immer noch gedrueckt
        if(now - lastTime > 5000) {status = 2;   hmwdebug(status);}
      }else{              // nicht mehr gedrueckt
    	if(now - lastTime > 100)   // determine sensors and send announce on short press
    		hmwmodule->broadcastAnnounce(0);
        status = 0;
        hmwdebug(status);
      };
      break;
    case 2:
      if(!buttonState) {  // losgelassen
    	status = 3;
    	hmwdebug(status);
    	lastTime = now;
      };
      break;
    case 3:
      // wait at least 100ms
      if(now - lastTime < 100)
    	break;
      if(buttonState) {   // zweiter Tastendruck
    	status = 4;
    	hmwdebug(status);
    	lastTime = now;
      }else{              // noch nicht gedrueckt
    	if(now - lastTime > 10000) {status = 0;   hmwdebug(status);}   // give up
      };
      break;
    case 4:
      if(now - lastTime < 100) // entprellen
          	break;
      if(buttonState) {   // immer noch gedrueckt
        if(now - lastTime > 3000) {status = 5;  hmwdebug(status);}
      }else{              // nicht mehr gedrÃ¯Â¿Â½ckt
        status = 0;
        hmwdebug(status);
      };
      break;
    case 5:   // zweiter Druck erkannt
      if(!buttonState) {    //erst wenn losgelassen
    	// Factory-Reset          !!!!!!  TODO: Gehoert das ins Modul?
    	factoryReset();

//##################################################################
    	hmwmodule->setNewId();
//##################################################################
    	status = 0;
    	hmwdebug(status);
      }
      break;
  }

  // control LED
  static long lastLEDtime = 0;
  switch(status) {
    case 0:
      digitalWrite(LED, LOW);
      break;
    case 1:
      digitalWrite(LED, HIGH);
      break;
    case 2:
    case 3:
    case 4:
      if(now - lastLEDtime > 100) {  // schnelles Blinken
    	digitalWrite(LED,!digitalRead(LED));
    	lastLEDtime = now;
      };
      break;
    case 5:
      if(now - lastLEDtime > 750) {  // langsames Blinken
       	digitalWrite(LED,!digitalRead(LED));
       	lastLEDtime = now;
      };
  }
};

#if DEBUG_VERSION != DEBUG_NONE
void printChannelConf(){
  for(byte channel = 0; channel < HMW_CONFIG_NUM_COUNTERS; channel++) {
	  hmwdebug("Channel     : "); hmwdebug(channel); hmwdebug("\r\n");
	  hmwdebug("\r\n");
  }
}
#endif

// ----------------------------------------------------------------------------------------------
void handleCounter() {
  long now = millis();

   CHANNEL_PORTS
  if ((now - lastPortReadTime) > POLLING_TIME) {

	  for(byte channel = 0; channel < HMW_CONFIG_NUM_COUNTERS; channel++) {
		  currentPortState[channel] = digitalRead(channelPorts[channel]);
		  if (currentPortState[channel] > oldPortState[channel]) {
			currentCount[channel]++;
      countInTime[channel]++;
      countLeistung[channel]++;
		  }
 		  oldPortState[channel] = currentPortState[channel];
  }
	  lastPortReadTime = now;
  }
  // Pruefen, ob wir irgendwas senden muessen

  for(byte channel = 0; channel < HMW_CONFIG_NUM_COUNTERS; channel++) {
	 // do not send before min interval
	 if(config.counters[channel].send_min_interval && now - lastSentTime[channel] < (long)(config.counters[channel].send_min_interval) * 1000)
		  continue;
    if(    (config.counters[channel].send_max_interval && now - lastSentTime[channel] >= (long)(config.counters[channel].send_max_interval) * 1000)
   	 || (config.counters[channel].send_delta_count
   	         && abs( currentCount[channel] - lastSentCount[channel] ) >= (config.counters[channel].send_delta_count))) {

              currentWh[channel] = ((currentCount[channel] * 1000) /config.counters[channel].impulse_pro_einheit);

        // if bus is busy, then we try again in the next round
      if(hmwmodule->sendInfoMessageVeryLong(channel, currentCount[channel], currentWh[channel], countInTime[channel], currentLeistung[channel], centralAddressGet()) != 1) {
            lastSentCount[channel] = currentCount[channel];
            lastSentTime[channel] = now;

            countInTime[channel] = 0;      
      };
    };
  };
}
//            hmwdebug("\r\n");
//            hmwdebug("   Channel     : ");      hmwdebug(channel);
//            hmwdebug("   Impulse Gesamt: "); hmwdebug (currentCount[channel]);
//            hmwdebug("   Wh Gesamt: "); hmwdebug (currentWh[channel]);
//            hmwdebug("   Impulse pro Zeitintervall: "); hmwdebug (countInTime[channel]);
//            hmwdebug("   Leistung: "); hmwdebug (currentLeistung[channel]);
//            hmwdebug("   Impulse pro KWh: "); hmwdebug (config.counters[channel].impulse_pro_einheit);
//            hmwdebug("\r\n");

// -----------------------------------------------------------------------------------------


void IntTimer1()
{
  if (IntCount < 50)      //5 Minuten
   {
      IntCount++;
//      hmwdebug("Zaehler: "); hmwdebug(IntCount); hmwdebug("\r\n");
   }
   else
   {
      IntCount = 0;    
      for(byte channelInt = 0; channelInt < HMW_CONFIG_NUM_COUNTERS; channelInt++) {


              currentLeistung[channelInt] = ((countLeistung[channelInt] * 12) / (config.counters[channelInt].impulse_pro_einheit / 1000));

           countLeistung[channelInt] = 0;
//      hmwdebug("Leistung: "); hmwdebug(currentLeistung[channelInt]); hmwdebug("\r\n");
      };
   };
}


// -----------------------------------------------------------------------------------------



void setup()
{
#if DEBUG_VERSION == DEBUG_UNO
	pinMode(RS485_RXD, INPUT);
	pinMode(RS485_TXD, OUTPUT);
#elif DEBUG_VERSION == DEBUG_UNIV
	pinMode(DEBUG_RXD, INPUT);
	pinMode(DEBUG_TXD, OUTPUT);
#endif
	pinMode(RS485_TXEN, OUTPUT);
	digitalWrite(RS485_TXEN, LOW);

	pinMode(BUTTON, INPUT_PULLUP);
	pinMode(LED, OUTPUT);

#if DEBUG_VERSION == DEBUG_UNO
   hmwdebugstream = &Serial;
   Serial.begin(19200);
   rs485.begin(19200);    // RS485 via SoftwareSerial
#elif DEBUG_VERSION == DEBUG_UNIV
   SoftwareSerial* debugSerial = new SoftwareSerial(DEBUG_RXD, DEBUG_TXD);
   debugSerial->begin(19200);
   hmwdebugstream = debugSerial;
   Serial.begin(19200, SERIAL_8E1);
#else
   Serial.begin(19200, SERIAL_8E1);
#endif



   // config aus EEPROM lesen
   hmwdevice.readConfig();

   // set input/output
   // Input Pins arbeiten mit PULLUP, d.h. muessen per Taster
   // auf Masse gezogen werden
   long int now = millis();

   CHANNEL_PORTS
   for (byte i = 0; i < HMW_CONFIG_NUM_COUNTERS; i++) {
     pinMode(channelPorts[i],INPUT_PULLUP);
     lastSentTime[i] = now;
   }

   lastPortReadTime = now;

  //-------------------------------------------------------------

  pinMode(IDENTIFY_LED, OUTPUT);      // Identify LED

  static long last_IDENT_LED_time;
//  long now = millis();
  last_IDENT_LED_time = now;
  // Identity Status und EEPROM lesen 
  static byte IDENTY;
  IDENTY = EEPROM.read(IDENTIFY_EEPROM);

//------------------------------------------------------------- Interupt Initialisieren
Timer1.initialize(alle_x_sekunden*1000000);
Timer1.attachInterrupt(IntTimer1);
//-------------------------------------------------------------

  if (IDENTY == 0x00) {
     digitalWrite(IDENTIFY_LED, LOW);
     last_IDENT_LED_time = now;
     } 
  else {
     if(now - last_IDENT_LED_time > 600) {        // schnelles Blinken
      digitalWrite(IDENTIFY_LED,!digitalRead(IDENTIFY_LED));  //Toggel LED
      last_IDENT_LED_time = now;
      }
   }
  //-------------------------------------------------------------

	// device type: 0x84
   // TODO: Modultyp irgendwo als define
 	 hmwmodule = new HMWModule(&hmwdevice, &hmwrs485, 0x84);

    hmwdebug("\n");
    hmwdebug("HBW-Sen-EP  - 8-Fach S0 interface\n");
    hmwdebug("\n");
    
#if DEBUG_VERSION != DEBUG_NONE
	printChannelConf();
#endif
}


// broadcast announce message once at the beginning
// this might need to "wait" until the bus is free
void handleBroadcastAnnounce() {
  static boolean announced = false;
  // avoid sending broadcast in the first second
  // we don't care for the overflow as the announce
  // is sent after 40 days anyway (hopefully)
  if(millis() < 1000) return;
  if(announced) return;
  // send methods return 0 if everything is ok
  announced = (hmwmodule->broadcastAnnounce(0) == 0);
}


// The loop function is called in an endless loop
void loop()
{
	// Daten empfangen und alles, was zur Kommunikationsschicht gehÃ¯Â¿Â½rt
	// processEvent vom Modul wird als Callback aufgerufen
	hmwrs485.loop();

// send announce message, if not done yet
   handleBroadcastAnnounce();
	
	// Bedienung ueber Button
	handleButton();

	// Verarbeitung der S0-Schnittstelle
	handleCounter();

};








