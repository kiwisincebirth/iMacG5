// BOF preprocessor bug prevent - insert me on top of your arduino-code
// See: http://subethasoftware.com/2013/04/09/arduino-compiler-problem-with-ifdefs-solved/
#if 1
__asm volatile ("nop");
#endif

/**
 * System Management Controller Kiwi's iMac G5
 * 
 * Description : Provides System management function for iMac G5, providing 
 *   fuctionality for Power Management for G5 internl PSU, and Intel NUC Motherboard. 
 *   Also provides control of Inverter Brightnes and Fan Speed Control, and permits 
 *   control via USB for applications that wish to control these functions 
 *   e.g. screen brightness.
 *
 * This application also has code to support capacative touch sensors as used 
 * by MacTester in his excellent iMac creations
 *
 * See : http://www.tonymacx86.com/imac-mods/107859-kiwis-next-project-imac-g5.html
 * Source Repository : https://github.com/kiwisincebirth/iMacG5/tree/master/SourceCode
 * Platform : Arduino Leonardo ATMega32U4
 * Author : KiwiSinceBirth
 * Contributions : MacTester (capacitance)
 * Written : July 2014
 */

//#define DEBUG // enables debig mode
#define LEGACYBOARD // supports the legacy fan control LM317, proto board  
#define COMMANDABLE // has all commands not just brightness
//#define CAPACITIVE // support for cap touch sensors
#define TEMPERATURE // temperature intput for fans

#include <DebugUtils.h>
#include <PWMFrequency.h>
#include <SimpleTimer.h>
#include <FiniteStateMachine.h>

#include <EEPROMex.h>
#include <EEPROMVar.h>
// NOTE: Please remove the "_EEPROMEX_DEBUG" 
// #define in the source code of the library

#ifdef CAPACITIVE
#include <CapacitiveSensorDue.h>
#endif

#ifdef TEMPERATURE
#include <OneWire.h>
#include <DallasTemperature.h>
#endif

/**
 * The Libraries: DebugUtils, FSM, PWMFrequency, SimpleTimer can be obtained here
 * https://github.com/kiwisincebirth/Arduino/tree/master/Libraries
 *
 * And the following libraries come from here
 * EEPROMEx - https://github.com/thijse/Arduino-Libraries/tree/master/EEPROMEx
 * CapacitiveSensorDue - https://github.com/arduino-libraries/CapacitiveSensor/tree/master/libraries
 * OneWire - http://www.pjrc.com/teensy/td_libs_OneWire.html
 * DallasTemperature - https://github.com/milesburton/Arduino-Temperature-Control-Library
 */

// Some Time Constants
#define ZERO 0
#define ONE_MILLIS 1
#define FIVE_MILLIS 5
#define TEN_MILLIS 10
#define FIFTY_MILLIS 50
#define ONE_HUNDRED_MILLIS 100
#define TWO_HUNDRED_MILLIS 200
#define THREE_HUNDRED_MILLIS 300
#define FIVE_HUNDRED_MILLIS 500
#define SEVEN_HUNDRED_MILLIS 700
#define ONE_SECOND 1000
#define TWO_SECOND 2000
#define FIVE_SECOND 5000
#define TEN_SECOND 10000
#define THIRTY_SECOND 30000
#define ONE_MINUTE 60000
#define TWO_MINUTE 120000
#define FIVE_MINUTE 300000
#define TEN_MINUTE 600000

// Some ASCII Constancts
#define CR 13
#define LF 10
#define TAB '\t'
#define STOP ','
#define COMMA ','
#define COLON ':'
#define EQUALS '='
#define ZERO_CHAR '0'

//
// ================
// REVISION HISTORY
// ================
// 1.0 - 26/07/2014 - Initial Stable Feature complete Release
// 1.1 - 31/01/2015 - Added support for RPM Fan Input, future feature
// 1.1 - 31/01/2015 - Simplified EEPROM Code, Long support Only.
// 1.1 - 31/01/2015 - Added addition ifdefs to reduce size of bnary.
// 1.2 - 15/02/2015 - Started to add support for RPM Fan Control
//
// --------------
// Future Futures
// --------------
// Better Capacitance Calibration Routines, that ask user to touch sensor, then measure
// If brightness change by Capacatance report report back to the app, so slider can moved
// System State command SS - FSM State, Power, Inverter, etc
//

// Some important constants
#define SERIAL_BAUD_RATE 9600

//
// ===================
// PIN ASSIGNMENTS
// ===================
//

//
// Digital 0, 1, Shared as serial pins (Serial1)
// Digital 2 - 13, General Purpuse 
// Digital 3, 5, 6, 9, 10, 11, and 13 - PWM
// Digital 11 - 13 Excluded on Pro Micro
// Digital 13 has (not Leo) an LED and resistor attached to it
// Digital 14 - 16, Shared with a SPI bus
// Digital 17 - Doubles and RX LED
// Digital Pins can sink 40ma, recommended 470 ohm resistor
// Analog Inputs: A0-A3, Pro Micro excludes A4, A5
// Analog Inputs: A6-A11 (on digital pins 4, 6, 8, 9, 10, and 12)
// Analog Pins can be used a digital ( refer to them as A0, A1, ...)
//

#ifdef LEGACYBOARD

//
// LEGACY PINS
//

// -------------- POWER DETECTION CONTROL

// Power Switch Control Pins
const byte POWER_SWITCH_PIN_POUT = 3; 
 
// Main PSU Control to de-active ATX PSU
const byte PSU_DEACTIVATE_POUT = 5;

// Power Notification
const byte SLEEP_LED_POUT = 6;

// Pwer State Sense Pins
const byte NUC_POWER_LED_VCC_PIN = 14;
const byte NUC_POWER_LED_GND_PIN = 15;

// --------------------- FAN CONTROL

// Fan Voltage Input Analog Detection
const byte FAN_OUTPUT_AIN = A2; 

// PWM Fan Speed Voltage OUTPUT
const byte FAN_CONTROL_POUT = 9; 

// Digital Pin For Temps
const byte TEMP_PIN = 16;

// -------------------- INVERTER 

// Inverter Brigtness PWM Out
const byte INVERTER_POUT = 10; 

// --------------------- CAP TOUCH

// Up and down capacitive touch broghtness pins
const byte TOUCH_COMMON_PIN_CAP = 2;
const byte TOUCH_DOWN_PIN_CAP = 7;
const byte TOUCH_UP_PIN_CAP = 8;

// -------------------- CHIME

// Chime Output --->>>  HIGH <<<---  Level OUTPUT
const byte CHIME_POUT = 1; // TXO

#else

//
// MODERN PINS
//

// -------------- POWER DETECTION CONTROL

// Power Switch Control Pins
const byte POWER_SWITCH_PIN_POUT = A1; 
 
// Main PSU Control to de-active ATX PSU
const byte PSU_DEACTIVATE_POUT = 4;

// Power Notification
const byte SLEEP_LED_POUT = 3;

// Pwer State Sense Pins
const byte NUC_POWER_LED_VCC_PIN = 14;
const byte NUC_POWER_LED_GND_PIN = 15;

// ---------------------- FAN SPEED PINS

// The Pins For Fan Input
const byte FAN_RPM_PIN[] = { 1, 2, 7 }; 

// Interrupts that fans are attached to.
const byte FAN1_INTERRUPT = 3; 
const byte FAN2_INTERRUPT = 1; 
const byte FAN3_INTERRUPT = 4; 

// --------------------- FAN CONTROL

// Fan Control Outputs
const byte FAN_CONTROL_PWM[] = { 9, 6, 10 } ;

// Digital Pin For Temps
const byte TEMP_PIN = A2;

// -------------------- INVERTER 

// Inverter Brigtness PWM Out
const byte INVERTER_POUT = 5; 

// --------------------- CAP TOUCH

// Up and down capacitive touch broghtness pins
const byte TOUCH_COMMON_PIN_CAP = A0;
const byte TOUCH_DOWN_PIN_CAP = 16;
const byte TOUCH_UP_PIN_CAP = 8;

// -------------------- CHIME

// Chime Output LOW Level OUTPUT
const byte CHIME_POUT = A3;

#endif

//
// ================================================
// EEPROM Locations that configuratyion data is stored
// ==================================================
//

// Location the Current Version ID is stored
const byte EEP_VERSION = 0;

const byte EEP_BRIGHT = 1; // brghtness Setting 0 - 100 %
const byte EEP_CYC_SMC = 2; // SMC Power / Rest Cycles (StartUp)
const byte EEP_CYC_POWER = 3; // CPU Power Cycles (Power Up)
const byte EEP_CYC_SLEEP = 4; // CPU Sleep Cycles (Wake)
const byte EEP_TIM_SYSTEM = 5; // System Uptime
const byte EEP_TIM_LCD = 6; //LCD Power On Time
const byte EEP_TIM_SLEEP = 7; //LCD Power On Time

const byte EEP_RESERVED3 = 8; 
const byte EEP_RESERVED4 = 9; 
const byte EEP_RESERVED5 = 10; 
const byte EEP_RESERVED6 = 11; 
const byte EEP_RESERVED7 = 12; 
const byte EEP_RESERVED8 = 13; 
const byte EEP_RESERVED9 = 14; 
const byte EEP_RESERVEDA = 15; 

const byte EEP_MIN_BRIGHT = 16; // Minimum PWM for Inverter - When Brihhtness is 0%
const byte EEP_MAX_BRIGHT = 17; // Maximum PWM for Inverter - When Brightness is 100%
const byte EEP_BRIGHT_INC = 18; // Number of percent brightness Increased or decreased
const byte EEP_MIN_TEMP = 19; // Fan Control to Map Temp to Voltage
const byte EEP_MAX_TEMP = 20; // Fan Control to Map Temp to Voltage
const byte EEP_MIN_VOLT = 21; // Fan Control to Map Temp to Voltage
const byte EEP_MAX_VOLT = 22; // Fan Control to Map Temp to Voltage
const byte EEP_DN_CAP_THR = 23; // Capacatance Treshold for Down Button
const byte EEP_UP_CAP_THR = 24; // Capacatance Treshold for Up Button
const byte EEP_CAP_SAMPLE = 25; // Number of Samples for Capacatance
const byte EEP_DEPRECATED1 = 26; // The Old Model ID of the project this board implements
const byte EEP_MIN_LED_PWM = 27; // Minimum PWM Value for Front Panel LED Effects
const byte EEP_MAX_LED_PWM = 28; // Maximum PWM Value for Front Panel LED Effects
const byte EEP_INVERTER_STARTUP_DELAY = 29; // milliseconds before starting inverter 
const byte EEP_CAP_CONTROL = 30; // is the capacatace controller enabled 
const byte EEP_FAN_CONTROL = 31; // is the fan temp controller enabled  
const byte EEP_INVERTER_WARM_DELAY = 32; // milliseconds before starting inverter 

//
// Names of Data stored in these locations
//

const char PROGMEM string_RES[] PROGMEM = "Reserved";
const char PROGMEM string_DEP[] PROGMEM = "Deprecated";
const char PROGMEM string_VS[] PROGMEM = "Version-Size";
const char PROGMEM string_P0[] PROGMEM = "Bright-%";
const char PROGMEM string_P1[] PROGMEM = "Cycles-SMC";
const char PROGMEM string_P2[] PROGMEM = "Cycles-Power";
const char PROGMEM string_P3[] PROGMEM = "Cycles-Sleep";
const char PROGMEM string_P4[] PROGMEM = "Time-Power";
const char PROGMEM string_P5[] PROGMEM = "Time-LCD";
const char PROGMEM string_P6[] PROGMEM = "Time-Sleep";

const char PROGMEM string_02[] PROGMEM = "MinBright-PWM"; 
const char PROGMEM string_03[] PROGMEM = "MaxBright-PWM"; 
const char PROGMEM string_04[] PROGMEM = "BrightInc-%";
const char PROGMEM string_05[] PROGMEM = "MinTemp-.01dc"; 
const char PROGMEM string_06[] PROGMEM = "MaxTemp-.01dc";
const char PROGMEM string_07[] PROGMEM = "MinVolt-.01v";
const char PROGMEM string_08[] PROGMEM = "MaxVolt-.01v";
const char PROGMEM string_09[] PROGMEM = "CapDnThresh"; 
const char PROGMEM string_10[] PROGMEM = "CapUpThresh";
const char PROGMEM string_11[] PROGMEM = "CapSamples";
const char PROGMEM string_13[] PROGMEM = "MinLED-PWM";
const char PROGMEM string_14[] PROGMEM = "MaxLED-PWM";  
const char PROGMEM string_15[] PROGMEM = "InvertStartDly";  
const char PROGMEM string_16[] PROGMEM = "CapBrigEnabled"; 
const char PROGMEM string_17[] PROGMEM = "FanTempEnabled"; 
const char PROGMEM string_18[] PROGMEM = "InvertWarmDly"; 

//
// the following must contain all strings from above
//

PGM_P const eepName[] PROGMEM = {
//const char PROGMEM *eepName[] = {  // older 1.0.x
  string_VS, string_P0, string_P1, string_P2, string_P3, string_P4, string_P5, string_P6, 
  string_RES, string_RES, string_RES, string_RES, string_RES, string_RES, string_RES, string_RES, 
  string_02, string_03, string_04, string_05, string_06, string_07, 
  string_08, string_09, string_10, string_11, string_DEP, string_13, 
  string_14, string_15, string_16, string_17, string_18 };

// 
// ========================================
// MAIN ARDUINO SETUP (1x during startup)
// ========================================
//

void setup () {
  
  // ensure basic configuration of user data is carried out.
  checkEepromConfiguration();
}

// 
// ========================================
// MAIN ARDUINO LOOP (Called Repeatidly)
// ========================================
//

void loop () {

  // allow the FSM to interpret and process state transitions
  loopFiniteStateMachine();

  // Let the simple timer run any backgound tasks
  loopSimpleTimer();

  // handles user input command received from serial port 
  loopSerialCommandProcessor();
}

//
// ============================================
// EEPRORM SETUP AND USER CONFIGURATION
// ============================================
//

/**
 * Setup the device if not initialised, flasing the eeprom with defaults, if they dont exist
 */
void checkEepromConfiguration() {

  // The version number holds the number of the highest value stored in eeprom
  // This allows future software upgrades with new eeprom values, to have defaults
  // written to the eeprom without overitting exisitng values that are stored there
  // VERSION = The Numer of (the highest) items that already have values, dont overitte
  
  // Read the current version of the eeprom
  byte currentHighest = eepromRead(EEP_VERSION);
  if ( currentHighest == 255 ) currentHighest = 0;

  // All The Deefault Values
  if ( currentHighest < EEP_INVERTER_WARM_DELAY ) { 
    
    // write default values
    eepromWrite( EEP_BRIGHT, 100 ); // 100% brightness
    eepromWrite( EEP_CYC_SMC, ZERO);   // 
    eepromWrite( EEP_CYC_POWER, ZERO); // 
    eepromWrite( EEP_CYC_SLEEP, ZERO); // 
    eepromWrite( EEP_TIM_SYSTEM, ZERO); // 
    eepromWrite( EEP_TIM_LCD, ZERO); // 
    eepromWrite( EEP_TIM_SLEEP, ZERO); // 
    eepromWrite( EEP_RESERVED3, ZERO); // 
    eepromWrite( EEP_RESERVED4, ZERO); // 
    eepromWrite( EEP_RESERVED5, ZERO); // 
    eepromWrite( EEP_RESERVED6, ZERO); // 
    eepromWrite( EEP_RESERVED7, ZERO); // 
    eepromWrite( EEP_RESERVED8, ZERO); // 
    eepromWrite( EEP_RESERVED9, ZERO); // 
    eepromWrite( EEP_RESERVEDA, ZERO); // 
    eepromWrite( EEP_MIN_BRIGHT, 60 ); // PWM
    eepromWrite( EEP_MAX_BRIGHT, 255 ); // PWM
    eepromWrite( EEP_BRIGHT_INC, 5 ); // Brightness Increme
    eepromWrite( EEP_MIN_TEMP, 0 ); // 100ths of a dreee e.g. 2000 = 20.0 degrees
    eepromWrite( EEP_MAX_TEMP, 2000 ); // 100ths of a dreee
    eepromWrite( EEP_MIN_VOLT, 250 ); // 100ths of a volt e.g. 330 = 3.3V
    eepromWrite( EEP_MAX_VOLT, 550 ); // 100ths of a volt
    eepromWrite( EEP_DN_CAP_THR, 150 ); // Down Threashhold
    eepromWrite( EEP_UP_CAP_THR, 150 ); // Up Threashold
    eepromWrite( EEP_CAP_SAMPLE, 200 ); // Samples
    eepromWrite( EEP_DEPRECATED1, ZERO ); // Was Old Default Model ID
    eepromWrite( EEP_MIN_LED_PWM, 5 ); // Minimum Effects PWM for Front Panel LED
    eepromWrite( EEP_MAX_LED_PWM, 255 ); // Maximum Effects PWM for Front Panel LED
    eepromWrite( EEP_INVERTER_STARTUP_DELAY, 6000 ); // COLD Inverter Startup delay miliseconds
    eepromWrite( EEP_CAP_CONTROL, 0 ); // Maximum Effects PWM for Front Panel LED
    eepromWrite( EEP_FAN_CONTROL, 1 ); // Inverter Startup delay miliseconds
    eepromWrite( EEP_INVERTER_WARM_DELAY, 6000 ); // WARM Inverter Startup delay miliseconds

    // and version number
    currentHighest = EEP_INVERTER_WARM_DELAY;
  } 

  // finally update eprom with Highest numbered item stored in EEPROM
  eepromWrite(EEP_VERSION,currentHighest);
}

//----------------------------- PUBLIC

long eepromRead(byte location) {
  
  // Can write to an invalid location 
  if (isLocationException(location) ) return -1;

  return EEPROM.readLong( eepromLocation(location) );
}

int eepromWrite(byte location, long value) {

  // Can write to an invalid location 
  if (isLocationException(location) ) return -1;

  // the eeprom library protects from wrinting same values
  return EEPROM.updateLong( eepromLocation(location), value );
}

void inline incrementCounter(byte location) {
  addToCounter(location, 1);
}

void addToCounter(byte location,unsigned long value) {
  eepromWrite(location, eepromRead(location)+value);
}

// ------------------ PRIVATE

byte sizeofEeprom() {
  return EEP_INVERTER_WARM_DELAY + 1; // HARD COCDED
}

boolean isLocationException(byte location) {

  if (location >= sizeofEeprom() ) {
    return true;
  }
  return false;
}

boolean isProtectedException(byte location) {
  return location <= 16;
}

int inline eepromLocation(byte location) {
  return location * 4;
}

String eepromName(byte location) {
  static char buffer[20]; 
  if (isLocationException(location) ) return "";
  strcpy_P(buffer, (char*)pgm_read_word(&(eepName[location])) ); 
  return buffer;
}

// -------------------------------

#ifdef COMMANDABLE

/**
 * Process a command from the serial port
 */
void processCommandEeprom(String subCmd, String extraCmd) {
  
  if (subCmd.equals("R")) {
    // EEPROM Read
    eepromReadCommand(extraCmd);
    
  } else if (subCmd.equals("W")) {
    // EEPROM Write EWnnn,xxxxx where n is the address and xxxx is the value
    eepromWriteCommand(extraCmd);
        
  } else if (subCmd.equals("F")) {
    // Reset The eeprom firmware version and reinit.
    eepromInitCommand();
    
  } else {
    Serial.println(F("Eprom Command Unknown: ERn (read), EWn,v (write), EF (factory)"));
  }
}

/**
 * Te Read EEPROM command can be passed an optional argument
 * n - which specifies the location to read, rather than iterating all locations.
 */
void eepromReadCommand(String extraCmd) {

  // if parameter passed then convert
  byte location = extraCmd.toInt();
  if (location>0 && !isLocationException(location) ) {
    eepromReadCommand(location);
  } else {
    for (byte i = 16;i<sizeofEeprom();i++) {
      eepromReadCommand(i);
    }
  }
}

void eepromReadCommand(byte location) {
  
  Serial.print(location);
  Serial.print(TAB);
  
  Serial.print(eepromName(location));
  Serial.print(TAB);
  
  Serial.println(eepromRead(location));
}

/**
 * The Write command has the following syntax
 * EWn,v - e,g EW7,3000  - writes the value of 3000 to address 7
 * Note: Some locations are protected, so cannot be written
 */
void eepromWriteCommand(String extraCmd) {
  
  // look for comma.
  byte index = extraCmd.indexOf(COMMA);
  if (index<=0) {
    // no equal so cannot handle
    Serial.println(F("Syntax Error - Correct Syntax is: EWn,v "));
    Serial.println(F("where n is the address, and v is the value"));
  } else {
    
    // work out the item and the location
    String loc = extraCmd.substring(0,index);
    String val = extraCmd.substring(index+1);
    byte location = loc.toInt();
    long value = val.toInt();
    
    if ( isLocationException(location) || isProtectedException(location) ) {
    //if ( isProtectedException(location) ) {  
      Serial.print(F("Cannot write to address "));
      Serial.println(location);
      return;
    }
    
    // write to eprom obeying protected status
    eepromWrite(location,value);
  } 
}

/**
 * The Flash command is used to re-initialise the firmware
 * Normall this shouldnt be needed. Only use if you need to recover defaults.
 */
void eepromInitCommand() {

    // EPROM Initialise. Resets the Eprom Values to default Values, sugest reboot after this
    eepromWrite(EEP_VERSION,0);
    
    // forces the eeprom to be re-inited
    checkEepromConfiguration();
    
    Serial.println(F("Flashed."));
}

#endif

//
// ============================================
// The main Background Task Scheduler
// ============================================
//

// A general purpose timer, and some misc constants
SimpleTimer timer = SimpleTimer();

// this does the actual work
void loopSimpleTimer() {
  timer.run();
}

//
// =================================
// STATE MACHINE ---
// =================================
//

// Inital Null State for Startup Definition of FSM
State STATE_STARTUP   = State (startupAndTransition);

// States of the Kiwi iMac FSM
State STATE_ACTIVE    = State (enterActiveMode   ,updateActiveMode   ,leaveActiveMode   );
State STATE_SLEEP     = State (enterSleepMode    ,updateSleepMode    ,leaveSleepMode    );
State STATE_POWERDOWN = State (enterPowerDownMode,updatePowerDownMode,leavePowerDownMode);
State STATE_INACTIVE  = State (enterInactiveMode ,updateInactiveMode ,leaveInactiveMode );
State STATE_POWERUP   = State (enterPowerUpMode  ,updatePowerUpMode  ,leavePowerUpMode  );

// Controlling Finitate State Manage, initial Active State
FiniteStateMachine fsm=FiniteStateMachine(STATE_STARTUP);

/**
 * Main Loop that processes the FSM contoller loop
 */
void loopFiniteStateMachine() {
  
  // Main Update Loop for The FSM
  fsm.update();
}

/**
 * Called by the StartUp State, 
 * transitions to next state based on Model ID
 */
void startupAndTransition() {

  // signal the sucessfule startup
  activateFrontPanelFlash(3);
  
  // count the start of the MCU
  incrementCounter(EEP_CYC_SMC);

  // iMac G5 20 - KiwiSinceBirth - Startup
  fsm.transitionTo(STATE_ACTIVE);
}

//
// ACTIVE STATE
//

unsigned long millsOfLastStartup = millis();
//unsigned long millsOfLastWakFromSleep = millis();

void enterActiveMode() {
  
  // DEBUG
  DEBUG_PRINTF("ENTER-ACTIVE");

  // TURN Basics On
  activatePSU(); // Ensure the ATX G5 PSU is active
  activateTempFanControl(); // fan temperatire controller
  
  // If Active State is coming from A BOOTUP state (either COLD or WARM)
  if ( fsm.wasInState(STATE_POWERUP) || fsm.wasInState(STATE_POWERDOWN) ) {
    
    // signal the chime 
    // TODO The Chime delaty could be in EEPROM
    initiateTimedChime(ONE_HUNDRED_MILLIS);
    
    // count the power cycle, and reset the startup time
    incrementCounter(EEP_CYC_POWER);
    millsOfLastStartup = millis();
    
    // read the startup dealy - before powering on the LCD
    int invDelay = eepromRead(EEP_INVERTER_STARTUP_DELAY);

    if ( fsm.wasInState(STATE_POWERDOWN) ) {
      // from warm start takes slightly longer.
      invDelay = eepromRead(EEP_INVERTER_WARM_DELAY);
    }

    DEBUG_PRINTF("Inverter Delay");
    DEBUG_PRINT(invDelay);

    // so that startup hides BIOS bootup messages.
    timer.setTimeout(invDelay,activateInverterAndKillLED);
    
  } else {
    
    // Assume from SLEEP, or RESET of Arduino, turn ON straight away
    activateInverterAndKillLED();
  }
}


/**
 * Activates the inverter with a builtin startup delay
 */
void activateInverterAndKillLED() {
  activateInverter(); // inverter ON
  activateCapTouchBright(); // turn on capcative brightness
  deactivateFrontPanelLED(); // turn off front panel LED.
} 

void updateActiveMode() {
  
  // detect power change, and transition
  if (powerS3()) {
    fsm.transitionTo(STATE_SLEEP);
  } else if (powerS5()) {
    fsm.transitionTo(STATE_POWERDOWN);
  }
}

void leaveActiveMode() {
  
  // Turn off Fans,and LCD
  deactivateInverter();
  deactivateTempFanControl();
  deactivateCapTouchBright();

  // DEBUG
  DEBUG_PRINTF("LEAVE-ACTIVE");
}

//
// SLEEP STATE 
//

unsigned long timeEnteringSleep;

void enterSleepMode() {

  // DEBUG
  DEBUG_PRINTF("ENTER-SLEEP");
  
  timeEnteringSleep = millis();

  activateFrontPanelLEDBreath();
}

void updateSleepMode() {
  
  // detect power change, and transition
  if (powerS0()) {
    fsm.transitionTo(STATE_ACTIVE);
  } else if (powerS5()) {
    fsm.transitionTo(STATE_POWERDOWN);
  }

}

void leaveSleepMode() {

  // DEBUG
  DEBUG_PRINTF("LEAVE-SLEEP");
  
  // count the sleep cycle
  incrementCounter(EEP_CYC_SLEEP);
  addToCounter(EEP_TIM_SLEEP,(millis()-timeEnteringSleep)/1000L);
  
  //millsOfLastWakFromSleep = millis();
}

//
// POWER DOWN STATE - STATE_POWERDOWN
// Occurs When CPU shuts down.
//

void enterPowerDownMode() {

  // DEBUG
  DEBUG_PRINTF("ENTER-PWR-DN");
 
  // Enable 30 second power down led effect
  activateFrontPanelLEDRampDn(THIRTY_SECOND);
  
  // and turn off LCD, and FANS
  deactivateInverter();
  deactivateTempFanControl();
}

//
// POWER DOWN STATE
// After Nuc Turned off but Before main power turned off
//

void updatePowerDownMode() {
  
  if ( fsm.timeInCurrentState() >= THIRTY_SECOND) {
    
    // DEBUG
    DEBUG_PRINTF("Powered Down for 30 Seconds -> Inactive");
  
    // if 30 seconds have been reached Transition to Inactive Mode
    fsm.transitionTo(STATE_INACTIVE);    
    
  } else if (powerS0()) {
    
    // WARM BOOT
    // the user probably turned machine back on, so normal PWR UP.
    fsm.transitionTo(STATE_ACTIVE);

    // With a quick LED Ramp up.
    activateFrontPanelLEDRampUp( FIVE_SECOND );
  }
}

void leavePowerDownMode() {

  // DEBUG
  DEBUG_PRINTF("LEAVE-PWR-DN");
}

//
// INACTIVE STATE
// When Machine is powered Off (ATX in Standby)
//

void enterInactiveMode() {
  
  // DEBUG
  DEBUG_PRINTF("ENTER-INACTIVE");
 
  // Turn everything Off.
  deactivateFrontPanelLED(); 
  deactivatePSU();
}

void updateInactiveMode() {

  // Was a PWr button detected, then power up
  if ( powerButtonDetection() ) {

    // DEBUG
    DEBUG_PRINTF("Front Panel Power Button Detected -> PowerUp");

    fsm.transitionTo(STATE_POWERUP);
  }
}

void leaveInactiveMode() {

  // curtosy showing user action
  activateFrontPanelFlash(1);

  // DEBUG
  DEBUG_PRINTF("LEAVE-INACTIVE");
}

//
// POWER UP STATE 
// Cold Boot From total shutdown
//

// has a counter of events that must occur, a mini FSM
byte powerupeventsequence = 1;

void enterPowerUpMode() {

  // DEBUG
  DEBUG_PRINTF("ENTER-PWR-UP");

  // Make sure PSU is turned back on
  activatePSU();
  
  // wait a short time and press the PWR button, ie wait for pwr to come up
  initiatePowerButtonPress( ONE_SECOND );

  // Ramp the LED slowely up over ten seconds
  activateFrontPanelLEDRampUp( TEN_SECOND );

  // Initialise the Mini FSM for powerup (see below).
  powerupeventsequence = 1;
}

void updatePowerUpMode() {
  
  // if 10 seconds have been reached 
  if ( fsm.timeInCurrentState() >= TEN_SECOND) {
    
    // DEBUG
    DEBUG_PRINTF("Powered Up for 10 Seconds -> Inactive");

    // The NUC failes to power on, so shutdown again.
    fsm.transitionTo(STATE_INACTIVE);    
    
  } else if ( powerupeventsequence == 1 && powerS0() ) {
    
    // The NUC when it boots from COLD, first power up, 
    // it does some extra things, not done from subsequnt WARM boots.
    // We need to track this so FSM doesnt get confused latter on.
    
    // DEBUG
    DEBUG_PRINTF("S0 -> Sequence 2");

    // from a COLD boot the LED flashes ONCE, comes on (S0)
    powerupeventsequence = 2;

  } else if ( powerupeventsequence == 2 && powerS5() ) {

    // DEBUG
    DEBUG_PRINTF("S5 -> Sequence 3");

    // from a COLD boot the LED flashes ONCE, ie then turns off (S5)
    powerupeventsequence = 3;
    
  } else if ( powerupeventsequence == 3 && powerS0() ) {

    // DEBUG
    DEBUG_PRINTF("S0 -> Active");

    // then ther is a second or so delay 
    // when the PWR light comes back on (S0) NUC is active
    // So transition to Normal Active Mode
    fsm.transitionTo(STATE_ACTIVE);
  }

}

void leavePowerUpMode() {
  // DEBUG
  DEBUG_PRINTF("LEAVE-PWR-UP");
}

//
// ===========
// CONTROLLERS
// ===========
//

//
// ---------------------------
// CAPACITIVE TOUCH CONTROLLER - Cap Inputs Brightness Out
// ---------------------------
//

#ifdef CAPACITIVE

int capacitiveTimer = -1;

void activateCapTouchBright() {
  
  // dont activate if disabled in eeprom
  if ( eepromRead(EEP_CAP_CONTROL) == 0 ) return;
  
  if ( capacitiveTimer >=0 ) {
    
    // make sure the Timer Thread is active
    timer.enable(capacitiveTimer);
    
  } else {

    // the sensores are in error, dont enable, protect against constant timeout and using CPU cycles
    if ( getUpSensorReading() < 0 && getUpSensorReading() < 0 ) return;

    // Timer to do a periodic calibrate
    timer.setInterval( TEN_MINUTE, calibrateSensorReading );

    // Timer, which triggers the Touch Sensor Control every 300ms, for brightness
    capacitiveTimer = timer.setInterval(THREE_HUNDRED_MILLIS,touchControl);
  }
  
  calibrateSensorReading();
}

void deactivateCapTouchBright() {
  
  if ( capacitiveTimer >= 0 ) timer.disable(capacitiveTimer);  
}

// PRIVATE ------------------

#ifdef COMMANDABLE

/*
 * Handles processing command from Serial API
 */
void processCommandCapacitive(String subCmd, String extraCmd) {
  
  if (subCmd.equals("C")) {
    processCommandCapacitiveCalibrate();

  } else if (subCmd.equals("A")) {
    activateCapTouchBright();
    Serial.print(F("Capacitive Enabled = "));
    Serial.println(timer.isEnabled(capacitiveTimer));

  } else if (subCmd.equals("D")) {
    deactivateCapTouchBright();
    Serial.print(F("Capacitive Enabled = "));
    Serial.println(timer.isEnabled(capacitiveTimer));

  } else if (subCmd.equals("I")) {
    Serial.print(F("Enabled     = "));
    Serial.println(timer.isEnabled(capacitiveTimer));
    Serial.print(F("Down Sensor = "));
    Serial.println(getDnSensorReading());
    Serial.print(F("Up Sensor   = "));
    Serial.println(getUpSensorReading());
    
  } else {
    Serial.println(F("Capacative Command Unknown: CA (activate), CC (calibrate), CD (deactivate), CI (info)"));
  }
    
}

void processCommandCapacitiveCalibrate() {

  calibrateSensorReading();

  Serial.println(F("Calibrated."));
    
// todo command to interactivly do capcacative calibration
//
// Command
// - Disable Main Timer
// - Create New Timer
// Timer
// 0 - 2000ms
// Serial Mesg "Hands Off"
// 1 - 2000ms
// Serial Mesg "Calibrating"
// Clear Cap minimum values
// Loop and take lots of readings (Left and Right) to establish Min Serial Mesg "..."
// Serial Mesg "Touch Left"
// 2 - 1000ms
// Serial Mesg "Calibrating"
// Loop and take lots of reading (Left) to establish Max Value Serial Mesg "..."
// Write values to eeprom 90% of Maximum
// Serial Mesg "Touch Right"
// 3 - 0ms
// Serial Mesg "Calibrating"
// Loop and take lots of reading (Right) to establish Max Value Serial Mesg "..."
// Write values to eeprom 90% of Maximum
// Serial Mesg "Complete"
// Serial Mesg "New Values x,y replaced old values x1,y1"
// Serial Mesg "You may now use"
// enable main timer.

}

#endif

// Main Touch Sensor Reading
void touchControl() {

  // If the capacitive sensor reads above a certain threshold
  if (isUpSensorReadingOverThreshold()) incInverterBright();
  
  // If the capacitive sensor reads above a certain threshold
  if (isDnSensorReadingOverThreshold()) decInverterBright();
}

#else

void activateCapTouchBright() {}
void deactivateCapTouchBright() {}
void processCommandCapacitive(String subCmd, String extraCmd) {}
  
#endif

//
// -------------------------------------------------
// FAN CONTROLLER - Temp Input controlling Fan output
// --------------------------------------------------
//

int tempFanControlTimer = -1;

/**
 * Activate the controller, and set the fan speed based on Temperature
 */
void activateTempFanControl() {
  
  // dont activate if disabled in eeprom, or the temp sensore arnt working
  if ( eepromRead(EEP_FAN_CONTROL) == 0 || getTempDiff() < 0.0f ) {
   
    // a default voltage
    setFanTargetValue( 3.3f );
    
    return;
  }
  
  if ( tempFanControlTimer >= 0 ) { 
    
    // make sure the Timer Thread is active
    timer.enable(tempFanControlTimer);
    
  } else {
   
    // setup a timer that runs every 500 ms - To Read Temperature
    tempFanControlTimer = timer.setInterval(THIRTY_SECOND,readTempSetTargetVolt);
  }
  
  // set initial voltage
  readTempSetTargetVolt();
}

/**
 * Deactive the controller, and turn the fans off
 */
void deactivateTempFanControl() {

  // disable the timer if it exists
  if (tempFanControlTimer >=0 ) timer.disable(tempFanControlTimer);  

  // and shut the fans down.
  setFanTargetValue( 0.0f ); // sets target volt to be low SHUTS FANS OFF
}

// PRIVATE ------------------

#ifdef COMMANDABLE

/*
 * Handles processing command from Serial API
 */
void processCommandFan(String subCmd, String extraCmd) {
  
  if (subCmd.equals("A")) {
    activateTempFanControl();
    Serial.print(F("Fans Enabled"));
    Serial.println(timer.isEnabled(tempFanControlTimer));

  } else if (subCmd.equals("D")) {
    deactivateTempFanControl();
    Serial.print(F("Fans Set To Min"));
    Serial.println(timer.isEnabled(tempFanControlTimer));

  } else if (subCmd.equals("M")) {
    deactivateTempFanControl();
    Serial.print(F("Fans Set To Max"));
    setFanTargetValue( 12.0f ); // MAXIMUM FANS

/*

  } else if (subCmd.equals("L")) {
    activateTempFanMonitor();
    
  } else if (subCmd.equals("O")) {
    deactivateTempFanMonitor();

*/

  } else {
    Serial.println(F("Fan Command Unknown: FM (max), FA (activate), FD (deactivate), FL (log), FO (log off)"));
  }  
}

#endif

void readTempSetTargetVolt() {
 
  // read the current temperature
  float tempAboveAmbient = getTempDiff();
    
  // Computes the Factor 0 - 1 (with trig rounding) that the temp is on our MIN MAX Scale
  float tempFactor = computeTempFactor(tempAboveAmbient);
    
  float minVolt = (float)eepromRead(EEP_MIN_VOLT) / 100.0f;
  float maxVolt = (float)eepromRead(EEP_MAX_VOLT) / 100.0f;

  // compute the Target Voltage based on the temperature 
  setFanTargetValue ( tempFactor * (maxVolt - minVolt) + minVolt );
}

/**
 * Computes the Factor 0 - 1 that the temp is on our MIN MAX Scale
 * Function ensures temp is not outside correct fange 
 */
float computeTempFactor( float temp ) {
  
  float minTemp = (float)eepromRead(EEP_MIN_TEMP) / 100.0f;
  float maxTemp = (float)eepromRead(EEP_MAX_TEMP) / 100.0f;
  
  if ( temp<=minTemp ) return 0.0f;
  if ( temp>=maxTemp ) return 1.0f;
  
  float tempFactor = ( temp-minTemp ) / ( maxTemp-minTemp ); 
  
  return computeTrigFactor(tempFactor);
}

// PRIVATE ------------------- MONITORING INFO

#ifdef COMMANDABLE

/*
void printFanDetails() {
    Serial.print(getAmbientTemp());
    Serial.print(F("\t"));
    Serial.print(getTempDiff());
    Serial.print(F("\t"));
    Serial.print(getFanTargetVolt());
    Serial.print(F("\t"));
    Serial.print(readVoltage(FAN_OUTPUT_AIN) * 2.0); // Current Voltage
    Serial.print(F("\t"));
    Serial.print(getCurrentPWM()); // Current PWM
    Serial.print(F("\t"));
    Serial.print(getFanRPM1());
    Serial.print(F("\t"));
    Serial.print(getFanRPM2());
    Serial.println();
    
}

int tempFanMonitorTimer = -1;

// Activate the controller, and set the fan speed based on Temperature
void activateTempFanMonitor() {
  
  if ( tempFanMonitorTimer == -1 ) { 
    
    // setup a timer that runs every 500 ms - To Read Temperature
    tempFanMonitorTimer = timer.setInterval(3000,printFanDetails);
  }

  getFanRPM1();
  getFanRPM2();

  // make sure the Timer Thread is active
  timer.enable(tempFanMonitorTimer);    
}

// Deactive the controller, and turn the fans off
void deactivateTempFanMonitor() {

  // disable the timer if it exists
  if (tempFanMonitorTimer >=0 ) timer.disable(tempFanMonitorTimer);  
}

*/

#endif

//
// =======
// INPUTS
// =======
//

//
// -----------------------
// CAPACITIVE TOUCH INPUTS
// -----------------------
//

// while debugging cant afford extra overhead
#ifdef CAPACITIVE

/**
 * Have we got a positive DOWN sensor reading
 */
boolean isDnSensorReadingOverThreshold() {
  return getDnSensorReading() > eepromRead(EEP_DN_CAP_THR);
}

/**
 * Have we got a positive UP sensor reading
 */
boolean isUpSensorReadingOverThreshold() {
  return getUpSensorReading() > eepromRead(EEP_UP_CAP_THR);
}

// 1M resistor between pins 4 & 2, pin 2 is sensor pin
CapacitiveSensorDue dnSensor = CapacitiveSensorDue(TOUCH_COMMON_PIN_CAP,TOUCH_DOWN_PIN_CAP);	

// 1M resistor between pins 4 & 6, pin 6 is sensor pin
CapacitiveSensorDue upSensor = CapacitiveSensorDue(TOUCH_COMMON_PIN_CAP,TOUCH_UP_PIN_CAP);	

/**
 * Read The Down sensor directly
 */
long getDnSensorReading() {
  return dnSensor.read(200);
}

/**
 * Read The UP sensor directly
 */
long getUpSensorReading() {
  return upSensor.read(200);
}

void calibrateSensorReading() {
  upSensor.calibrate();
  dnSensor.calibrate();
}

#else

long getDnSensorReading    () { return 0; }
long getUpSensorReading    () { return 0; }
void calibrateSensorReading() { }

#endif

#ifndef LEGACYBOARD

//
// ----------
// RPM INPUTS
// ----------
//

volatile long fanRotationCount[3];
unsigned long fanTimeStart[3];

void interruptFan1() {
  fanRotationCount[0]++;
}

void interruptFan2() {
  fanRotationCount[1] ++;
}

void interruptFan3() {
  fanRotationCount[2] ++;
}

int getFanRPM( byte fan ) {
  
  static boolean started = false;
  if (!started) {
    
    for ( byte i=0;i<3;i++ ) {
      // Start Up the RPM by setting PIN 
      pinMode(FAN_RPM_PIN[i],INPUT_PULLUP);    
    }
    
    //and attaching interrupt
    attachInterrupt(FAN1_INTERRUPT,interruptFan1,FALLING);
    attachInterrupt(FAN2_INTERRUPT,interruptFan2,FALLING);
    attachInterrupt(FAN3_INTERRUPT,interruptFan3,FALLING);
    
    started = true;
  }

  double ret = fanRotationCount[fan] /2L *1000L * 60L / ( millis() - fanTimeStart[fan] );
  fanRotationCount[fan] = 0;
  fanTimeStart[fan] = millis();
  return ret;
}

#endif

//
// ----------
// TEMP INPUT
// ----------
//

#ifdef TEMPERATURE

float ambient_temp = DEVICE_DISCONNECTED_C;

/**
 * Get the current Main Temperatire
 */
float getTempDiff() { 
  
  // Setup OneWire Dallas Temp Sensors
  static OneWire oneWire(TEMP_PIN);
  static DallasTemperature sensors(&oneWire); 
  static boolean started = false;
  
  if (!started) {
    // Start up the library
    sensors.begin();
    started = true;
  }
  
  sensors.requestTemperatures(); // Send the command to get temperatures
  float temp0 = sensors.getTempCByIndex(0); 
  float temp1 = sensors.getTempCByIndex(1); 
  
  if (temp0==DEVICE_DISCONNECTED_C || temp1==DEVICE_DISCONNECTED_C) return -1;
  
  if (temp0 > temp1) {
    ambient_temp = temp1;
    return temp0 - temp1;
  } else {
    ambient_temp = temp0;
    return temp1 - temp0;
  }
}

/**
 * Get the Ambiant Temp which is based on secondary temp sensor
 * and is levellled ober time, so no big spikes.
 */
float getAmbientTemp() {
  
  return ambient_temp;
}

#else

float getTempDiff()    { return 10; }
float getAmbientTemp() { return 20; }
  
#endif

//
// -----------------------------
// MOTHERBOARD CPU State - INPUT 
// -----------------------------
//

/**
 * Fully Active Mode - LED ON
 */
boolean powerS0() {
  return (powerState()==0); 
}

/**
 * Sleep Mode - LED IS SECOND COLOUR
 */
boolean powerS3() {
  return (powerState()==3); 
}

/**
 * Sleep Mode - LED IS OFF
 */
boolean powerS5() {
  return (powerState()==5); 
}

/**
 * The actual power state is read based on the front panel LED
 * from the mohtrerbaord. It assumes a bi-colour LED which reverses
 * the polarity of the LED, during sleep mode.
 *
 * Note There is a transitional State, when switching states where the LED's are
 * in an indeterminte state, both outputs high and our reading may be wrong. 
 * this is proably motherboard dependant. The solution is to wait for the readings 
 * to settle down (10 ms) before reporting a chnage to the caller
 *
 * 0 - ACTIVE
 * 3 - SLEEP
 * 5 - INACTIVE
 * 99 - UNDEFINED
 *
 * NOTE: In the documentation for Intel NUC MB the LED Wireing
 * is contradictory between LED orenentation and Polatity + / - 
 * The + / - is correct, LED scematic symbol is incorrect
 */
byte powerState() {

  // variable used between calls  
  static byte reportedState = 0; // value we last returned
  static byte lastObservered = 0; // value we last observered
  static unsigned long firstObservervedWhen = 0; // when did we first observe

  // read the raw state from the input pins
  byte justObservered = powerStateRawValue();

  // if the just observered state is different to last observered then 
  if ( justObservered != lastObservered ) {
  
    // then we want to reset our observation  
    lastObservered = justObservered;
    
    // and reset a timer so know when we first observered the value
    firstObservervedWhen = millis();
  }

  // if the last observered value different to last reported
  // and sufficient time has exceeded (say 10ms) 
  if ( lastObservered != reportedState && firstObservervedWhen-millis() > FIFTY_MILLIS ) {
    
    // then we can set the last observered value is probably final so
    reportedState = lastObservered;

    // DEBUG
    DEBUG_PRINTF("POWER-STATE Reported S");
    DEBUG_PRINT(reportedState);
  }
  
  // return the reported state, with 50ms delay to confirm the reading.
  return reportedState;
}

byte powerStateRawValue() {
  
  // Power State Sense Pins
  pinMode(NUC_POWER_LED_VCC_PIN,INPUT_PULLUP);
  pinMode(NUC_POWER_LED_GND_PIN,INPUT_PULLUP);

  // read voltages on two pins
  int POS_Val=digitalRead(NUC_POWER_LED_VCC_PIN);
  int Neg_Val=digitalRead(NUC_POWER_LED_GND_PIN);

  // the state we just observered (default to 0)
  static byte value = 0;
  
  // if the PIN Voltages are correct polarity set S3 State
  if ( POS_Val == HIGH && Neg_Val == LOW  ) value = 0;
  if ( POS_Val == LOW  && Neg_Val == HIGH ) value = 3;
  if ( POS_Val == HIGH && Neg_Val == HIGH ) value = 5;

  // return that state
  return value;
}

//
// -----------------------------------
// Power Button INPUT / OUTPUT
// -----------------------------------
// Controls detection of Power Button
// Controls depressing of Power Button
// -----------------------------------
//

long dealybeforePressing; // time before pressing button.

/**
 * Detects the Front Power Button Being Pressed, is it currently being pressed 
 * 
 * If the button is being depressed (by us) this function will not report it.
 */
boolean powerButtonDetection() {
  
  digitalWrite(POWER_SWITCH_PIN_POUT,HIGH);
  pinMode(POWER_SWITCH_PIN_POUT,INPUT_PULLUP);

  // read the pin as it is configured for input
  return digitalRead(POWER_SWITCH_PIN_POUT) == LOW; 
}

/**
 * Press the Power On Button, for 300ms, after waiting for a specified period of time.
 *
 * Note: While the button is being presssed, the input functions
 * will not function correctly, they will return false. This is because
 * we use the same PIN on arduino, and it can be used for both input and output.
 */
void initiatePowerButtonPress(long dealybefore) {
  
  // DEBUG
  DEBUG_PRINTF("Front Panel Power Button - Initiate");

  // set the time to wait
  dealybeforePressing = dealybefore;

  // setup a timer to press the button  
  timer.setVariableTimer(powerButtonCallback);
}

//
// PRIVATE ------------------
//

long powerButtonCallback(int state) {
  
  if (state==0) {
    
    // then configure it for output
    digitalWrite(POWER_SWITCH_PIN_POUT,HIGH);
    pinMode(POWER_SWITCH_PIN_POUT,OUTPUT);
    //powerbuttonMode = BUTTON_OUTPUT;

    return dealybeforePressing;

  } else if (state==1) {
    
    // Activate the PIN
    digitalWrite(POWER_SWITCH_PIN_POUT,LOW);

    // DEBUG
    DEBUG_PRINTF("Front Panel Power Button - Depressed");

    return ONE_HUNDRED_MILLIS;
    
  } else if (state==2) {  
    
    // DEBUG
    DEBUG_PRINTF("Front Panel Power Button - Released");

    // initialise the pin for input
    digitalWrite(POWER_SWITCH_PIN_POUT,HIGH);
    pinMode(POWER_SWITCH_PIN_POUT,INPUT_PULLUP);
    //powerbuttonMode = BUTTON_INPUT;
    
    return 0L;
  }
}

//
// ------------------
// INPUT UTILITY CODE
// ------------------
//

/**
 * the analog volatge from an analog input pin
 */
float readVoltage( int sensorPin ) {
 
  // make sure iys an input pin
  pinMode( sensorPin, INPUT);
  
  // read and discard once 
  analogRead(sensorPin);
 
  // Volatage = Reading / 1023 {magnitude of reading} * 5 {volts that the reading represents} 
  // take two readings and average magnitude = 2046 / 5 => 409.2
  return ( analogRead(sensorPin) + analogRead(sensorPin) ) / 409.2f; 
}

//
//
//

//
// =======
// OUTPUTS
// =======
//

//
// -----------------------------------------
// SCREEN (Inverter) PWM BRIGHTNESS - OUTPUT
// -----------------------------------------
//

// locally cached brightness from EEPROM
int brightness = 100; 
boolean inverterActive = true;
unsigned long millsWhenLCDActivated = millis();
int eepromBrightWriteTimer = -1;

// BRIGHTNESS ------------------

/**
 * Set the brightness of your LCD Inverter 
 * Parameter is 0 - 100 %
 * Note this % is mapped to an internal PWM Value.
 */
int setInverterBright(int bright) {
  // initalisation
  initInverterBright();
  
  // dont allow chnages to brigness if inactive
  if (!inverterActive) return getInverterBright();
  
  // set brightness, checking for out of range
  if (bright<0) brightness = 0;
  else if (bright>100) brightness = 100;
  else brightness = bright;
  
  // set the output
  setInverterPWMBrightness();

  return getInverterBright();
}
  
byte incInverterBright() {
  return setInverterBright(getInverterBright()+eepromRead(EEP_BRIGHT_INC));
}

byte decInverterBright() {
  return setInverterBright(getInverterBright()-eepromRead(EEP_BRIGHT_INC));
}

int getInverterBright() {
  return brightness;
}

// INVERTER ------------------

// writes active time to eprom
void flushLCDActiviation() {
  if ( inverterActive ) {
    addToCounter(EEP_TIM_LCD,(millis()-millsWhenLCDActivated)/1000L );
    millsWhenLCDActivated = millis();
  }
}

/**
 * This activates the Inverter, and sets the brightness based on brighness setting
 */
void activateInverter() {
  // initalisation
  initInverterBright();

  if (!inverterActive) {
    
    DEBUG_PRINTF("INVERTER Activated");
      
    inverterActive = true;
    millsWhenLCDActivated = millis();
    setInverterPWMBrightness();
  }
}

/**
 * This deactivates the Inverter
 */
void deactivateInverter() {
  // initalisation
  initInverterBright();

  if ( inverterActive ) {
    flushLCDActiviation();

    DEBUG_PRINTF("INVERTER Deactivated");

    inverterActive = false;
    setInverterPWMBrightness();
  }
}

// PRIVATE ------------------

void initInverterBright() {
  
  if (eepromBrightWriteTimer>=0) return;
  
  // read initial value of brightness
  brightness = eepromRead(EEP_BRIGHT);
  
  // Inverter LCD Display Bright PIN Setup
  digitalWrite(INVERTER_POUT,HIGH);
  pinMode(INVERTER_POUT,OUTPUT);
  setPWMPrescaler(INVERTER_POUT,1); // Sets 31.25KHz Frequency

  // setup timer to update EEPROM every 2 minutes, with current brightness setting.
  eepromBrightWriteTimer = timer.setInterval(TWO_MINUTE,writeBrightToEEPROM);

  // activeate the inverter, Set Bright
  setInverterPWMBrightness();  
}

void writeBrightToEEPROM() {
   
    // Every 2 minutes write brightness to EEPROM gives user time to finsh adjustment.
    // NOTE The Write function first does a read, and only writes if needed
    eepromWrite(EEP_BRIGHT,getInverterBright());
}

// Internally called function
byte setInverterPWMBrightness() {
  
  if (inverterActive) {
    
    byte inverterMin = eepromRead(EEP_MIN_BRIGHT);
    byte inverterMax = eepromRead(EEP_MAX_BRIGHT);
    int range = inverterMax - inverterMin;
    byte pwmValue = ( range * getInverterBright() / 100 ) + inverterMin;  
    analogWrite(INVERTER_POUT,pwmValue);
    return pwmValue;
  } else {
    analogWrite(INVERTER_POUT,0);
    return 0;
  }
}

// ----------------------

/*
 * Handles processing command from Serial API
 */
void processCommandBrightness(String subCmd, String extraCmd) {
  
  if (subCmd.equals("R")) {
    // Brightness Read
    Serial.println( getInverterBright() );
    
  } else if (subCmd.equals("W")) {
    // Brightness Write value BWnnn - nnn is the value
    setInverterBright( extraCmd.toInt() );
    
  } else if (subCmd.equals("A")) {
    // Activate Inverter
    activateInverter();
    
  } else if (subCmd.equals("D")) {
    // Deactivate Inverter
    deactivateInverter();
    
//  } else if (subCmd.equals("+")) {
//    // Deactivate Inverter
//    incInverterBright();
//    
//  } else if (subCmd.equals("-")) {
//    // Deactivate Inverter
//    decInverterBright();
    
  } else {
    Serial.println(F("Brightness Command Unknown: BR (read), BW (write), BA (activate), BD (deactivate)"));
  }
}

//
// ------------------
// Fan Output Voltage
// ------------------
//

#ifdef LEGACYBOARD

float targetVOLT = 3.3; // The output voltage we want to produce
int fanVoltageControlTimer = -1; // timer that controls fan voltages

/**
 * Set the Target Fan Voltage
 */
void setFanTargetValue(float volt) {
  
  initFanVoltControl();

  // set the target voltage  
  targetVOLT = volt;
}

//float getFanTargetVolt() {
//  return targetVOLT;
//}

// PRIVATE ------------------

void initFanVoltControl() {
  
  if (fanVoltageControlTimer>=0) return;

  digitalWrite(FAN_CONTROL_POUT,HIGH); // ensures minimum voltage
  pinMode(FAN_CONTROL_POUT,OUTPUT);   // PWM Output Pin For Fan
  setPWMPrescaler(FAN_CONTROL_POUT,1); // Sets 31.25KHz Frequency
  
    // setup a timer that runs every 50 ms - To Set Fan Speed
  fanVoltageControlTimer = timer.setInterval(FIFTY_MILLIS,readVoltSetFanPWM);
}

// Roughly is equal to 3.3v NOTE Cannot be computed
byte currentPWM = 150; 

byte getCurrentPWM() {
  return currentPWM;
}

void readVoltSetFanPWM() {
  
  // Roughly is equal to 3.3v NOTE Cannot be computed
  //byte currentPWM = 150; 

  // target PWM if fans are not active
  float currentVolt = readVoltage(FAN_OUTPUT_AIN) * 2.0;
  
  if ( targetVOLT < currentVolt ) {
    
    // Slow fans down slowely, rather than hard off.
    // increasing PWM duty, lowers the voltage
    currentPWM = currentPWM<255 ? currentPWM+1 : 255;
    
  } else if ( targetVOLT > currentVolt ) {
    
    // Slowly ramp up the fan speed, rather than hard on.
    // decreasing PWM duty, increases the voltage
    currentPWM = currentPWM>0 ? currentPWM-1 : 0;
  }
  
  // write the PWM  
  analogWrite(FAN_CONTROL_POUT,currentPWM);
}

#else 

//
// ----------------------
// FAN OUTPUT RPM CONTROL
// ------------------
// 

float targetRPM = 1000; // The output voltage we want to produce
int fanRPMControlTimer = -1; // timer that controls fan voltages

/**
 * Set the Target Fan Voltage
 */
void setFanTargetValue(float rpm) {
  
  initFanRPMControl();

  // set the target voltage  
  targetRPM = rpm;
}

// PRIVATE ------------------

void initFanRPMControl() {
  
  if (fanRPMControlTimer>=0) return;

  for ( byte i=0; i<3; i++ ) {
    
    digitalWrite(FAN_CONTROL_PWM[i],HIGH); // ensures minimum voltage
    pinMode(FAN_CONTROL_PWM[i],OUTPUT);   // PWM Output Pin For Fan
    setPWMPrescaler(FAN_CONTROL_PWM[i],256); // Sets 31.25KHz / 256 = 122Hz Frequency
  }
  
  // setup a timer that runs every 50 ms - To Set Fan Speed
  fanRPMControlTimer = timer.setInterval(500,readRPMSetFanPWM);
}

// Roughly is equal to 3.3v NOTE Cannot be computed
//byte currentPWM = 150; 

//byte getCurrentPWM() {
//  return currentPWM;
//}

void readRPMSetFanPWM() {
  
  // Roughly is equal to 3.3v NOTE Cannot be computed
  //byte currentPWM = 150; 

  // target PWM if fans are not active
  //float currentVolt = readVoltage(FAN_OUTPUT_AIN) * 2.0;
  
  //if ( targetVOLT < currentVolt ) {
    
    // Slow fans down slowely, rather than hard off.
    // increasing PWM duty, lowers the voltage
    //currentPWM = currentPWM<255 ? currentPWM+1 : 255;
    
  //} else if ( targetVOLT > currentVolt ) {
    
    // Slowly ramp up the fan speed, rather than hard on.
    // decreasing PWM duty, increases the voltage
    //currentPWM = currentPWM>0 ? currentPWM-1 : 0;
  //}
  
  // write the PWM  
  //analogWrite(FAN_CONTROL_POUT,currentPWM);
}

#endif

//
// ------------
// CHIME OUTPUT
// ------------
//

long delayBeforeChime;

void initiateTimedChime( long delayBefore ) {
  
   // DEBUG
  DEBUG_PRINTF("CHIME");
  
    // set the time to wait
  delayBeforeChime = delayBefore;

  // setup a timer to press the button  
  timer.setVariableTimer(chimeOutputCallback);
}

long chimeOutputCallback(int state) {
  
  if (state==0) {
    
    return delayBeforeChime;

  } else if (state==1) {
    
    // then configure it for output activate pin
    pinMode(CHIME_POUT,OUTPUT);
    digitalWrite(CHIME_POUT,LOW);

    // DEBUG
    DEBUG_PRINTF("Chime - Activated");

    return ONE_HUNDRED_MILLIS;
    
  } else if (state==2) {  
    
    // DEBUG
    DEBUG_PRINTF("Chime - Released");

    // initialise the pin for input
    pinMode(CHIME_POUT,INPUT);
    
    return 0L;
  }
}


// PRIVATE ------------------

//
// ------------------------------------
// G5 ATX PSU Control OUTPUT - POWER ON
// ------------------------------------
//

boolean psuIsActive = true;
unsigned long millsWhenPSUActivated = millis();

// writes active time to eprom
void flushPSUActiviation() {
  if ( psuIsActive ) {
    addToCounter(EEP_TIM_SYSTEM,(millis()-millsWhenPSUActivated)/1000L );
    millsWhenPSUActivated = millis();
  }
}

void activatePSU () {

  if ( ! psuIsActive ) {
    digitalWrite(PSU_DEACTIVATE_POUT,LOW);
    pinMode(PSU_DEACTIVATE_POUT,INPUT);
    psuIsActive = true;
    millsWhenPSUActivated = millis(); // reset counter
    
    DEBUG_PRINTF("PSU Activated");
  }
}

void deactivatePSU () {
  
  if ( psuIsActive ) {
    flushPSUActiviation(); // write active time to eeprom
    
    pinMode(PSU_DEACTIVATE_POUT,OUTPUT);
    digitalWrite(PSU_DEACTIVATE_POUT,HIGH);
    psuIsActive = false;
    
    DEBUG_PRINTF("PSU Deactivated");
  }
}

//
// ----------------------------------
// Front Panel LED Control - OUTPUT
// ----------------------------------
//

const byte LEDMODE_NORMAL = 0;
const byte LEDMODE_BREATH = 10;
const byte LEDMODE_RAMPUP = 20; 
const byte LEDMODE_RAMPDN = 30;

byte ledBrightnessMode = LEDMODE_NORMAL; // are we using any special features
byte targetLEDBright = 0; // what the brightness should be, smoothing

byte ledFlashCount = 0; // how many flashes are left
unsigned long ledFlashStartTime = 0; // is there a duration of the effect (fade)

/**
 * Turns off the front panel LED
 */
void deactivateFrontPanelLED() {  
  setFrontPanelLEDBright(ZERO);
}

void activateFrontPanelLEDBreath() {
  setFrontPanelLEDEffect(LEDMODE_BREATH,0L);
}

void activateFrontPanelLEDRampUp(unsigned long duration) {
  setFrontPanelLEDEffect(LEDMODE_RAMPUP,duration);
}

void activateFrontPanelLEDRampDn(unsigned long duration) {
  setFrontPanelLEDEffect(LEDMODE_RAMPDN,duration);
}

void activateFrontPanelFlash(byte count) {
  initLEDBrightness();
  ledFlashCount = count;
  ledFlashStartTime = millis();
}

/**
 * Sets the brightness 0 - 1 of front panel LED
 */
void setFrontPanelLEDBright(byte value) {
  initLEDBrightness();
  ledBrightnessMode = LEDMODE_NORMAL;
  targetLEDBright = value;
}

// PRIVATE ------------------ EFFECTS

unsigned long ledEffectStartTime = 0; // when did the effect start
unsigned long ledEffectDuration = 0; // is there a duration of the effect (fade)

float ledBase=0; // the minimum LED value
float ledFactor=255; // the maximum (multiplier)

void setFrontPanelLEDEffect(byte mode, unsigned long duration) {
  initLEDBrightness();
  ledBrightnessMode = mode;
  ledEffectStartTime = millis();
  ledEffectDuration = duration;
  ledBase = eepromRead(EEP_MIN_LED_PWM);
  ledFactor = eepromRead(EEP_MAX_LED_PWM) - eepromRead(EEP_MIN_LED_PWM);
}

// PRIVATE ------------------ 

void initLEDBrightness() {

  static int ledBrightnessTimer = -1; // timer number in use

  // has already been initialised
  if (ledBrightnessTimer >=0) return;
  
  // Front Panle Power LED Notification
  digitalWrite(SLEEP_LED_POUT,LOW);
  pinMode(SLEEP_LED_POUT,OUTPUT);

  // A timer is created to transition the LED brightness 
  // at a defined rate 0-255 full brightness in 100ms ie
  // from full on to full off in a 1/10 of a second, 
  // this gives a cleaner looking LED
  ledBrightnessTimer = timer.setInterval(FIVE_MILLIS,ledBrightnessCallback);
}

// Callback function actiually sets LED brightness
// this means the maximun change of 5 PWM Values per time slice 
void ledBrightnessCallback() {

  if ( ledFlashCount > 0 ) {
      
    // Just set the actual brightness no damping or adjustment of target       
    setActualLEDBright( floatToPWM( computeFlash() ) );   

  } else {
    
    unsigned long timeinEffect = millis()-ledEffectStartTime;
    
    switch (ledBrightnessMode) {
      // set new target brightness
      case LEDMODE_BREATH:
        targetLEDBright = floatToPWMWithOffset( computeBreath(timeinEffect) );
        break;
      case LEDMODE_RAMPUP:
        targetLEDBright = floatToPWMWithOffset( computeRampUp(timeinEffect) );
        break;
      case LEDMODE_RAMPDN:
        targetLEDBright = floatToPWMWithOffset( 1.0f - computeRampUp(timeinEffect));
        break;
    }
    setActualLEDBrightWithDamping(targetLEDBright);
  } 
}

byte floatToPWM( float value ) {
  float newValue = value*255;
  if ( newValue <= 0.0f ) return 0;
  if ( newValue >= 255.0f ) return 255;
  return newValue;
}

byte floatToPWMWithOffset( float value ) {
  float newValue = value*ledFactor + ledBase;
  if ( newValue <= 0.0f ) return 0;
  if ( newValue >= 255.0f ) return 255;
  return newValue;
}

// current ACTUAL PWM brightness, used for smoothing
int actualLEDBright = 0; 

void setActualLEDBrightWithDamping( int value ) {
  
  // delta of 25 every 10ms gives 100ms (1/10 second) from off to on
  int delta = value - actualLEDBright;

  // if the delta is greater than 25 
  if (delta >= 25 ) {
    delta = 25;
  } else if (delta <= -25 ) {
    delta = -25;
  }

  // set a new led brightness, with maximum delta
  setActualLEDBright( actualLEDBright + delta );
}

void setActualLEDBright( int value ) {
  // this is where we convert float to PWM
  actualLEDBright = value;
  
  // actual write of the PWM Value
  analogWrite(SLEEP_LED_POUT, actualLEDBright );
}

/**
 * ON Startup of SMC flash the LED 3 time 100 on 200 off to indicate power on
 */
float computeFlash() {
  
  // how much time has elapsed in the 6 second breath cycle. 0 - 2
  int timeInFlashCycle = millis()-ledFlashStartTime;
  
  // if in second half of cycle higher than one
  if ( timeInFlashCycle >=75 && timeInFlashCycle < 125 ) {
    
    return 1.0f;
    
  } else if (timeInFlashCycle>=200) {

    // decrease the remainng flash count
    ledFlashStartTime = ledFlashStartTime + 200;
    ledFlashCount--;

  }
  return 0.0f;
}

/**
 * Breathing LED effect brightness compute brightness
 */
float computeBreath(unsigned long timeInCycle) {
  
  // how much time has elapsed in the 5 second breath cycle.
  long timeInBreathCycle = timeInCycle % FIVE_SECOND;
  
  // when in the first 1.5 seconds
  if ( timeInBreathCycle <= 1500 ) {

    // simple cos function that ramps up    
    return computeTrigFactor( (float)timeInBreathCycle/1500.0f );
    
  } else {
    
    // convert to a decreasing value from 1 to 0, cos function raised to power 1.8
    return pow( computeTrigFactor((float)(5000-timeInBreathCycle)/3500.0f), 1.8f );
  }
}

/**
 * Effect to control LED Ramp Up or Ramp Down
 */
float computeRampUp( unsigned long timeInCycle ) {
  
  // if duration exceeded, will stop effect, and leave LED at end value
  if (timeInCycle>=ledEffectDuration) ledBrightnessMode = LEDMODE_NORMAL;
  
  // Workout the PWM value for LED Dimming
  return computeTrigFactor((float)timeInCycle/ledEffectDuration);
}

//
// =====================
// USB SERIAL CONTROLLER
// =====================
//

/**
 * PROCESS INPUT
 */
void loopSerialCommandProcessor() {

  static char serData[32]; // Buffer 
  static int count = 0;
  static boolean initialised = false;
  
  if (! initialised) {
    // fist tie into this method setup
    Serial.begin(SERIAL_BAUD_RATE);
    initialised = true;
  }

  // if serial port not ready (Leonardo only).
  if(!Serial) return; // cant do anything

  while(Serial.available() > 0) {

    // read a byte. and add to the buffer
    char inByte = Serial.read();
    
    // if th read failed
    if (inByte == -1) {
      break;
    
    // if EOL
    } else if ( inByte == CR || inByte == LF ) {
      
      // terminate the buffer
      serData[count++]=0;

      //Convert The array to a String
      String command = String(serData);
      
      if (command.length() > 0 ) {
      
        // process the command
        processCommand(command);
      }
      
      // clear the command buffer
      count=0;
    
    // protect against overrun of buffer
    } else if (count<30) {  
      serData[count++]=inByte;
    }  
  }
}

/**
 * Main function to process a command from terminal
 */
void processCommand(String cmd) {
  
  String firstCmd = cmd.substring(0,1);
  String secondCmd = cmd.substring(1,2);
  String extraCmd = cmd.substring(2);
  
  if (firstCmd.equals("B")) {
    processCommandBrightness(secondCmd,extraCmd);
  
#ifdef COMMANDABLE
  
  } else if (firstCmd.equals("C")) {
    processCommandCapacitive(secondCmd,extraCmd);

  } else if (firstCmd.equals("E")) {
    processCommandEeprom(secondCmd,extraCmd);
    
  } else if (firstCmd.equals("F")) {
    processCommandFan(secondCmd,extraCmd);

  } else if (firstCmd.equals("S")) {
    processCommandSystem(secondCmd,extraCmd);

  } else {
    Serial.println(F("Command Unknown: B (brighness), C (capacitive), E (eeprom), F (fans), S (system)"));
    
#endif    

  }
  
}

#ifdef COMMANDABLE

void processCommandSystem(String subCmd, String extraCmd) {
  
  if (subCmd.equals("S")) {
    
    flushPSUActiviation(); // Flushes PSU Time to EEPROM
    flushLCDActiviation();
    
    Serial.println();
    
    Serial.print("SMC Power Cycles ");
    Serial.println(eepromRead(EEP_CYC_SMC));

    Serial.print("Power On  Cycles ");
    Serial.println(eepromRead(EEP_CYC_POWER));

    Serial.print("Sleep     Cycles ");
    Serial.println(eepromRead(EEP_CYC_SLEEP));
    
    Serial.print(F("Powered Hours "));
    printHours(eepromRead(EEP_TIM_SYSTEM));
    Serial.println();

    Serial.print(F("Active  Hours "));
    printHours(eepromRead(EEP_TIM_SYSTEM)-eepromRead(EEP_TIM_SLEEP));
    Serial.println();
    
    Serial.print(F("LCD     Hours "));
    printHours(eepromRead(EEP_TIM_LCD));
    Serial.println();

    
  } else if (subCmd.equals("U")) {
    
    Serial.println();
    
    // System Timers in-use
    Serial.print(F("Threads used "));
    Serial.print(timer.getNumTimers());    
    Serial.println(F(" of 10"));

    // System Ram Available in bytes. Leonamrdo has 2.5Kbyes
    Serial.print(F("RAM in use "));
    Serial.print(2560L - freeRam());    
    Serial.println(F(" of 2560 bytes"));

    Serial.print(F("SMC Up Time "));
    printTime(millis()/1000L);
    Serial.println();

    Serial.print(F("CPU Up Time "));
    printTime((millis()-millsOfLastStartup)/1000L);
    Serial.println();
    
  } else {
    Serial.println(F("System Command Unknown: S (stats), U (uptime)"));
  }
}

void printTime( unsigned long secs ) {
  
    unsigned long mins = secs / 60L;
    secs = secs - (mins*60);
    unsigned long hrs = mins / 60L;
    mins = mins - (hrs*60);
    int days = hrs / 24;
    hrs = hrs - (days*24);  
    if (days>0) {
      Serial.print(days);
      Serial.print(F(" Days "));
    }
    if (hrs<10) Serial.print(ZERO_CHAR);
    Serial.print(hrs);
    Serial.print(COLON);    
    if (mins<10) Serial.print(ZERO_CHAR);
    Serial.print(mins);
    if (days==0) {
      Serial.print(COLON);    
      if (secs<10) Serial.print(ZERO_CHAR);
      Serial.print(secs);  
    }
}

void printHours( unsigned long secs ) {
  
    unsigned long hrs = secs / 3600;
    unsigned long decimal = (secs - (hrs*3600)) / 360L;
    Serial.print(hrs);
    Serial.print(".");
    Serial.print(decimal);
}

#endif

//
// =================================
// GENERAL PURPOSE UTILITY FUNCTIONS
// =================================
//

/**
 * Computes a Trig Function 
 * Input - Value between 0 and 1
 * Output - Value between 0 and 1, with cos smoothing
 */
float computeTrigFactor( float input ) {
  return cos(input * PI + PI) / 2.0f + 0.5f;
}

/**
 * Turns an input string into a float value
 */
float stringToFloat(String input) {
  char carray[input.length() + 1]; //determine size of the array
  input.toCharArray(carray, sizeof(carray)); //put readStringinto an array
  return atof(carray);
}

/** 
 * Function to return the free SRAM in bytes between heap and stack
 */
int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}



