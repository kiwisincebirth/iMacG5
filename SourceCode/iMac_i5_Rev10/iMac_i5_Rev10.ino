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
#define LEGACYBOARD // supports the legacy fan control LM317 voltage, proto board  
#define LEGACY-RPM // Legacy Controlled by RPM Inputs, not LM317 Voltage feedback
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

//
// ---------------------
// CONSTANTS
// ---------------------
//

// time before transition is effective (input debounce)
#define POWER_STATE_TRANSITION_TIME 50

// the amount of time to wait after acivation of Main Power before pressing NUC switch
#define NUC_POWER_SWITCH_WAIT_TIME 1000

// The amount of time the NUC Power switch is depressed
#define NUC_POWER_SWITCH_ACTIVATION_TIME 100

// The amount of time the Chime switch is depressed
#define CHIME_ACTIVATION_TIME 100

// Amount of Time the Front panel LED takes to Fade Up
#define LED_FADE_UP_TIME_WARM_BOOT 5000
#define LED_FADE_UP_TIME_COLD_BOOT 10000

// when starting up, if transition takes longer then move to shutdown
#define STARTUP_TIMEOUT_BEFORE_SHUTDOWN 10000

// Cycle time for Sleep LED Breathing
#define LED_BREATH_CYCLE_TIME 5000

// time after shutdown the ATX PSU is turned off.
#define ATX_PSU_SHUTDOWN_TIMOUT 30000

// time interval that Temperatures are sampled, and fan targets are set.
#define TEMPERATURE_SAMPLE_PERIOD 30000

// time intervale to attempt write of Brightness to EEPROM, burnout protection
#define EEPROM_BRIGHTNESS_PERIOD 120000

// amount of times the sensor is read
#define CAPACITIVE_SENSOR_READ_PERIOD 300

// time between capacitive recalibration - 10 minutes
#define CAPACITIVE_RECALIBRATE_PERIOD 600000

// Some important constants
//#define SERIAL_BAUD_RATE 9600
#define SERIAL_BAUD_RATE 115200

// ---------------------

// Some ASCII Constancts
#define CR 13
#define LF 10
#define TAB '\t'
#define STOP ','
#define COMMA ','
#define COLON ':'
#define EQUALS '='
#define ZERO '0'

//
// ================
// REVISION HISTORY
// ================
// 1.0 - 26/07/2014 - Initial Stable Feature complete Release
// 1.1 - 31/01/2015 - Added support for RPM Fan Input, future feature
// 1.1 - 31/01/2015 - Simplified EEPROM Code, Long support Only.
// 1.1 - 31/01/2015 - Added addition ifdefs to reduce size of bnary.
// 1.2 - 15/02/2015 - Started to add support for RPM Fan Control
// 1.3 - 19/04/2015 - Added Support for Mac Testers Slider App, Temps, 115200, OK
//
// --------------
// Future Futures
// --------------
// Better Capacitance Calibration Routines, that ask user to touch sensor, then measure
// If brightness change by Capacatance report report back to the app, so slider can moved
// System State command SS - FSM State, Power, Inverter, etc
//

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
#define POWER_SWITCH_PIN_POUT 3 
 
// Main PSU Control to de-active ATX PSU
#define PSU_DEACTIVATE_POUT 5

// Power Notification
#define SLEEP_LED_POUT 6

// Pwer State Sense Pins
#define NUC_POWER_LED_VCC_PIN 14
#define NUC_POWER_LED_GND_PIN 15

// ---------------------- FAN VOLTAGE INPUT

// Fan Voltage Input Analog Detection
#define FAN_OUTPUT_AIN A2

// NUMBER OF FAN NPUTS
#define FAN_COUNT 2

// The Pins For Fan Input
const byte FAN_RPM_PIN[] = { 0, 1 }; 

// Interrupts that fans are attached to.
#define FAN1_INTERRUPT 2 
#define FAN2_INTERRUPT 3 

// --------------------- FAN CONTROL

#ifdef LEGACY-RPM

// Time between controlling the Fans
#define FAN_CONTROL_PERIOD 5000

#else

// Time between controlling the Fans
#define FAN_CONTROL_PERIOD 100

#endif

// Number of Output Fan Control PWM's
#define FAN_CONTROL_COUNT 1

// PWM Fan Speed Voltage OUTPUT
const byte FAN_CONTROL_PWM[] = { 9 } ;

// Prescale Values for controlling PWM
const int FAN_CONTROL_PWM_PRESCALE[] = {1};

#ifdef LEGACY-RPM

// Values in RPM
#define OFF_FAN_VALUE 100
#define MIN_FAN_VALUE 1200
#define MAX_FAN_VALUE 5000

#else

// Values in Millivolts
#define OFF_FAN_VALUE 100
#define MIN_FAN_VALUE 330
#define MAX_FAN_VALUE 1200

#endif

// -------------------- TEMPS 

// Digital Pin For Temps
#define TEMP_PIN 16

// -------------------- INVERTER 

// Inverter Brigtness PWM Out
#define INVERTER_PWM 10 

// --------------------- CAP TOUCH

// Up and down capacitive touch broghtness pins
#define TOUCH_COMMON_PIN_CAP 2
#define TOUCH_DOWN_PIN_CAP 7
#define TOUCH_UP_PIN_CAP 8

// -------------------- CHIME

// Chime Output --->>>  HIGH <<<---  Level OUTPUT
#define CHIME_POUT 1

#else

//
//
// MODERN PINS
//
//

// -------------- POWER DETECTION CONTROL

// Power Switch Control Pins
#define POWER_SWITCH_PIN_POUT A1
 
// Main PSU Control to de-active ATX PSU
#define PSU_DEACTIVATE_POUT 4

// Power Notification
#define SLEEP_LED_POUT 3

// Pwer State Sense Pins
#define NUC_POWER_LED_VCC_PIN 14
#define NUC_POWER_LED_GND_PIN 15

// ---------------------- FAN SPEED PINS

// NUMBER OF FAN NPUTS
#define FAN_COUNT 3

// The Pins For Fan Input
const byte FAN_RPM_PIN[] = { 1, 2, 7 }; 

// Interrupts that fans are attached to.
#define FAN1_INTERRUPT 3 
#define FAN2_INTERRUPT 1 
#define FAN3_INTERRUPT 4 

// --------------------- FAN CONTROL

// Time between controlling the Fans
#define FAN_CONTROL_PERIOD 5000

// Number of Output Fan Control PWM's
#define FAN_CONTROL_COUNT 3

// Fan Control Outputs
const byte FAN_CONTROL_PWM[] = { 9, 6, 10 } ;

// Prescale Values for controlling PWM
const int FAN_CONTROL_PWM_PRESCALE[] = { 256, 256, 256 };

// Values in RPM
#define OFF_FAN_VALUE 0
#define MIN_FAN_VALUE 1200
#define MAX_FAN_VALUE 4000

// -------------------- TEMPS 

// Digital Pin For Temps
#define TEMP_PIN A2

// -------------------- INVERTER 

// Inverter Brigtness PWM Out
#define INVERTER_PWM 5 

// --------------------- CAP TOUCH

// Up and down capacitive touch broghtness pins
#define TOUCH_COMMON_PIN_CAP A0
#define TOUCH_DOWN_PIN_CAP 16
#define TOUCH_UP_PIN_CAP 8

// -------------------- CHIME

// Chime Output LOW Level OUTPUT
#define CHIME_POUT A3

#endif

//
// ================================================
// EEPROM Locations that configuratyion data is stored
// ==================================================
//

// Location the Current Version ID is stored
#define EEP_VERSION 0

#define EEP_BRIGHT 1 // brghtness Setting 0 - 100 %
#define EEP_CYC_SMC 2 // SMC Power / Rest Cycles (StartUp)
#define EEP_CYC_POWER 3 // CPU Power Cycles (Power Up)
#define EEP_CYC_SLEEP 4 // CPU Sleep Cycles (Wake)
#define EEP_TIM_SYSTEM 5 // System Uptime
#define EEP_TIM_LCD 6 //LCD Power On Time
#define EEP_TIM_SLEEP 7 //LCD Power On Time

#define EEP_RESERVED3 8 
#define EEP_RESERVED4 9 
#define EEP_RESERVED5 10 
#define EEP_RESERVED6 11 
#define EEP_RESERVED7 12 
#define EEP_RESERVED8 13 
#define EEP_RESERVED9 14 
#define EEP_RESERVEDA 15 

#define EEP_MIN_BRIGHT 16 // Minimum PWM for Inverter - When Brihhtness is 0%
#define EEP_MAX_BRIGHT 17 // Maximum PWM for Inverter - When Brightness is 100%
#define EEP_BRIGHT_INC 18 // Number of percent brightness Increased or decreased
#define EEP_MIN_TEMP 19 // Fan Control to Map Temp to Voltage
#define EEP_MAX_TEMP 20 // Fan Control to Map Temp to Voltage
#define EEP_MIN_VOLT_RPM 21 // Fan Control to Map Temp to Voltage / RPM
#define EEP_MAX_VOLT_RPM 22 // Fan Control to Map Temp to Voltage / RPM
#define EEP_DN_CAP_THR 23 // Capacatance Treshold for Down Button
#define EEP_UP_CAP_THR 24 // Capacatance Treshold for Up Button
#define EEP_CAP_SAMPLE 25 // Number of Samples for Capacatance
#define EEP_DEPRECATED1 26 // The Old Model ID of the project this board implements
#define EEP_MIN_LED_PWM 27 // Minimum PWM Value for Front Panel LED Effects
#define EEP_MAX_LED_PWM 28 // Maximum PWM Value for Front Panel LED Effects
#define EEP_INVERTER_STARTUP_DELAY 29 // milliseconds before starting inverter 
#define EEP_CAP_CONTROL 30 // is the capacatace controller enabled 
#define EEP_FAN_CONTROL 31 // is the fan temp controller enabled  
#define EEP_INVERTER_WARM_DELAY 32 // milliseconds before starting inverter 

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
const char PROGMEM string_05[] PROGMEM = "MinTemp-.01c"; 
const char PROGMEM string_06[] PROGMEM = "MaxTemp-.01c";
#ifdef LEGACYBOARD
  #ifdef LEGACY-RPM
const char PROGMEM string_07[] PROGMEM = "MinRPM    "; // redefined to be RPM Range for the Fans
const char PROGMEM string_08[] PROGMEM = "MaxRPM    ";
  #else
const char PROGMEM string_07[] PROGMEM = "MinVolt-.01v";
const char PROGMEM string_08[] PROGMEM = "MaxVolt-.01v";
  #endif
#else
const char PROGMEM string_07[] PROGMEM = "MinRPM    "; // redefined to be RPM Range for the Fans
const char PROGMEM string_08[] PROGMEM = "MaxRPM    ";
#endif
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
    eepromWrite( EEP_CYC_SMC, 0);   // 
    eepromWrite( EEP_CYC_POWER, 0); // 
    eepromWrite( EEP_CYC_SLEEP, 0); // 
    eepromWrite( EEP_TIM_SYSTEM, 0); // 
    eepromWrite( EEP_TIM_LCD, 0); // 
    eepromWrite( EEP_TIM_SLEEP, 0); // 
    eepromWrite( EEP_RESERVED3, 0); // 
    eepromWrite( EEP_RESERVED4, 0); // 
    eepromWrite( EEP_RESERVED5, 0); // 
    eepromWrite( EEP_RESERVED6, 0); // 
    eepromWrite( EEP_RESERVED7, 0); // 
    eepromWrite( EEP_RESERVED8, 0); // 
    eepromWrite( EEP_RESERVED9, 0); // 
    eepromWrite( EEP_RESERVEDA, 0); // 
    eepromWrite( EEP_MIN_BRIGHT, 60 ); // PWM
    eepromWrite( EEP_MAX_BRIGHT, 255 ); // PWM
    eepromWrite( EEP_BRIGHT_INC, 5 ); // Brightness Increme
    eepromWrite( EEP_MIN_TEMP, 0 ); // 100ths of a dreee e.g. 2000 = 20.0 degrees
    eepromWrite( EEP_MAX_TEMP, 2000 ); // 100ths of a dreee
    eepromWrite( EEP_MIN_VOLT_RPM, 250 ); // 100ths of a volt e.g. 330 = 3.3V
    eepromWrite( EEP_MAX_VOLT_RPM, 550 ); // 100ths of a volt
    eepromWrite( EEP_DN_CAP_THR, 150 ); // Down Threashhold
    eepromWrite( EEP_UP_CAP_THR, 150 ); // Up Threashold
    eepromWrite( EEP_CAP_SAMPLE, 200 ); // Samples
    eepromWrite( EEP_DEPRECATED1, 0 ); // Was Old Default Model ID
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
    initiateTimedChime(100);
    
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
  activateFrontPanelLEDRampDn(ATX_PSU_SHUTDOWN_TIMOUT);
  
  // and turn off LCD, and FANS
  deactivateInverter();
  deactivateTempFanControl();
}

//
// POWER DOWN STATE
// After Nuc Turned off but Before main power turned off
//

void updatePowerDownMode() {
  
  if ( fsm.timeInCurrentState() >= ATX_PSU_SHUTDOWN_TIMOUT) {
    
    // DEBUG
    DEBUG_PRINTF("Powered Down for 30 Seconds -> Inactive");
  
    // if 30 seconds have been reached Transition to Inactive Mode
    fsm.transitionTo(STATE_INACTIVE);    
    
  } else if (powerS0()) {
    
    // WARM BOOT
    // the user probably turned machine back on, so normal PWR UP.
    fsm.transitionTo(STATE_ACTIVE);

    // With a quick LED Ramp up.
    activateFrontPanelLEDRampUp( LED_FADE_UP_TIME_WARM_BOOT );
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
  initiatePowerButtonPress( NUC_POWER_SWITCH_WAIT_TIME );

  // Ramp the LED slowely up over ten seconds
  activateFrontPanelLEDRampUp( LED_FADE_UP_TIME_COLD_BOOT );

  // Initialise the Mini FSM for powerup (see below).
  powerupeventsequence = 1;
}

void updatePowerUpMode() {
  
  // if 10 seconds have been reached 
  if ( fsm.timeInCurrentState() >= STARTUP_TIMEOUT_BEFORE_SHUTDOWN ) {
    
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
    timer.setInterval( CAPACITIVE_RECALIBRATE_PERIOD, calibrateSensorReading );

    // Timer, which triggers the Touch Sensor Control every 300ms, for brightness
    capacitiveTimer = timer.setInterval( CAPACITIVE_SENSOR_READ_PERIOD ,touchControl);
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
    setFanContolTarget( MIN_FAN_VALUE );
    
    return;
  }
  
  if ( tempFanControlTimer >= 0 ) { 
    
    // make sure the Timer Thread is active
    timer.enable(tempFanControlTimer);
    
  } else {
   
    // setup a timer - To Read Temperature
    tempFanControlTimer = timer.setInterval(TEMPERATURE_SAMPLE_PERIOD,readTempSetTarget);
  }
  
  // set initial voltage
  readTempSetTarget();
}

/**
 * Deactive the controller, and turn the fans off
 */
void deactivateTempFanControl() {

  // disable the timer if it exists
  if (tempFanControlTimer >=0 ) timer.disable(tempFanControlTimer);  

  // and shut the fans down.
  setFanContolTarget( OFF_FAN_VALUE ); // sets target volt to be low SHUTS FANS OFF
}

// PRIVATE ------------------

boolean debugFans = false;

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

  } else if (subCmd.equals("R")) {
    // Fan Read (Slider Applet reads from the Arduino)
    Serial.print(getFanRPM(0));
    Serial.print(";");
    Serial.print(getFanRPM(1));
    Serial.print(";");
    if (FAN_COUNT>2) Serial.print(getFanRPM(2)); else Serial.print(0);
    Serial.print(";");
    Serial.print(getTemp1());
    Serial.print(";");
    Serial.print(getTemp2());
    Serial.print(";");
    Serial.println(getTemp3());

  } else if (subCmd.equals("M")) {
    deactivateTempFanControl();
    Serial.print(F("Fans Set To Max"));
    setFanContolTarget( MAX_FAN_VALUE );

  } else if (subCmd.equals("L")) {
    // "LOG" - Starts Logging
    debugFans = true;
    
  } else if (subCmd.equals("O")) {
    // "OFF" - Stops Logging
    debugFans = false;

  } else {
    Serial.println(F("Fan Command Unknown: FM (max), FA (activate), FR (read), FD (deactivate), FL (log), FO (log off)"));
  }  
}

#endif

void readTempSetTarget() {
 
  // read the current temperature
  float tempAboveAmbient = getTempDiff();
    
  // Computes the Factor 0 - 1 (with trig rounding) that the temp is on our MIN MAX Scale
  float tempFactor = computeTempFactor(tempAboveAmbient);
    
  float minValue = (float)eepromRead(EEP_MIN_VOLT_RPM);
  float maxValue = (float)eepromRead(EEP_MAX_VOLT_RPM);

  // compute the Target Voltage based on the temperature 
  setFanContolTarget ( tempFactor * (maxValue - minValue) + minValue );
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

//
// ----------
// RPM INPUTS
// ----------
//

volatile long fanRotationCount[FAN_COUNT];
unsigned long fanTimeStart[FAN_COUNT];

void interruptFan1() {
  fanRotationCount[0]++;
}

void interruptFan2() {
  fanRotationCount[1]++;
}

void interruptFan3() {
  fanRotationCount[2]++;
}

int getFanRPM( byte fan ) {
  
  static boolean started = false;
  if (!started) {
    started = true;
    
    for ( byte fan=0; fan<FAN_COUNT; fan++ ) {
      // Start Up the RPM by setting PIN 
      pinMode(FAN_RPM_PIN[fan],INPUT_PULLUP);    
    }
    
    //and attaching interrupt
#ifdef FAN1_INTERRUPT
    attachInterrupt(FAN1_INTERRUPT,interruptFan1,FALLING);
#endif
#ifdef FAN2_INTERRUPT
    attachInterrupt(FAN2_INTERRUPT,interruptFan2,FALLING);
#endif
#ifdef FAN3_INTERRUPT
    attachInterrupt(FAN3_INTERRUPT,interruptFan3,FALLING);
#endif
  }

  // pulses counted; /2 pulses per rotation; *60000 milliseconds per minute; /millis elapsed
  double ret = fanRotationCount[fan] /2L *60000L / ( millis() - fanTimeStart[fan] );
  fanRotationCount[fan] = 0;
  fanTimeStart[fan] = millis();
  return ret;
}

//
// ----------
// TEMP INPUT
// ----------
//

#ifdef TEMPERATURE

//float temp_1 = DEVICE_DISCONNECTED_C;
float temp_1 = 20;
float temp_2 = 30;

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
  float readtemp0 = sensors.getTempCByIndex(0); 
  float readtemp1 = sensors.getTempCByIndex(1); 
  
  //if (temp0==DEVICE_DISCONNECTED_C || temp1==DEVICE_DISCONNECTED_C) return -1;
  
  if (readtemp0 > readtemp1) {
    temp_1 = readtemp1;
    temp_2 = readtemp0;
  } else {
    temp_1 = readtemp0;
    temp_2 = readtemp1;
  }
  
  return temp_2 - temp_1;
}

/**
 * Get the Ambiant Temp which is based on secondary temp sensor
 * and is levellled ober time, so no big spikes.
 */
float getTemp1() {
  return temp_1;
}

float getTemp2() {
  return temp_2;
}

float getTemp3() {
  return 0;
}

#else

float getTempDiff() { return 10; }
float getTemp1()    { return 20; }
float getTemp2()    { return 30; }
float getTemp3()    { return  0; }

#endif

//
// -----------------------------
// MOTHERBOARD CPU State - INPUT 
// -----------------------------
//

// Power State Constants.
#define POWER_STATE_S0 0
#define POWER_STATE_S3 3
#define POWER_STATE_S5 5

/**
 * Fully Active Mode - LED ON
 */
boolean powerS0() {
  return (powerState()==POWER_STATE_S0); 
}

/**
 * Sleep Mode - LED IS SECOND COLOUR
 */
boolean powerS3() {
  return (powerState()==POWER_STATE_S3); 
}

/**
 * Sleep Mode - LED IS OFF
 */
boolean powerS5() {
  return (powerState()==POWER_STATE_S5); 
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
  if ( lastObservered != reportedState && firstObservervedWhen-millis() > POWER_STATE_TRANSITION_TIME ) {
    
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

    return NUC_POWER_SWITCH_ACTIVATION_TIME;
    
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
  digitalWrite(INVERTER_PWM,HIGH);
  pinMode(INVERTER_PWM,OUTPUT);
  setPWMPrescaler(INVERTER_PWM,1); // Sets 31.25KHz Frequency

  // setup timer to update EEPROM every 2 minutes, with current brightness setting.
  eepromBrightWriteTimer = timer.setInterval(EEPROM_BRIGHTNESS_PERIOD,writeBrightToEEPROM);

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
    analogWrite(INVERTER_PWM,pwmValue);
    return pwmValue;
  } else {
    analogWrite(INVERTER_PWM,0);
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
    Serial.println("OK");
    
  } else if (subCmd.equals("D")) {
    // Deactivate Inverter
    deactivateInverter();
    Serial.println("OK");
    
  } else {
    Serial.println(F("Brightness Command Unknown: BR (read), BW (write), BA (activate), BD (deactivate)"));
  }
}

//
// --------------------------
// Fan Output Control
// --------------------------
//

// The output Fan RPM (or mVolts for LM317) we desire
int targetFanValue[] = { MIN_FAN_VALUE, MIN_FAN_VALUE, MIN_FAN_VALUE }; 

// Set the Target Fan RPM (mVolt LM317) for a ALL Fans
// This controls all Fans, setting them to the same value
void setFanContolTarget(int value) {
  for ( int fan=0; fan<FAN_COUNT; fan++ ) {
    setFanContolTarget( fan, value );
  }
}

// Set the Target Fan RPM ( cVolt LM317) for a specified Fan
// this can be used for independant Fan Control.
// Legacy Mode only support a singe (0) target
void setFanContolTarget(byte fan,  int value) {
  
  // set the target voltage  
  targetFanValue[fan] = value;
  
  // init the controller
  initFanController();
}

// TARGET RPM (cVolt LM317)
int getFanControlTarget(byte fan) {
  return targetFanValue[fan];
}

int fanControlTimer = -1; // timer that controls fan voltages

void initFanController() {
  
  if (fanControlTimer>=0) return;

  // set up the three PWM outputs
  for ( byte fan=0; fan<FAN_CONTROL_COUNT; fan++ ) {
    
    digitalWrite(FAN_CONTROL_PWM[fan],HIGH); // ensures minimum voltage
    pinMode(FAN_CONTROL_PWM[fan],OUTPUT);   // PWM Output Pin For Fan
    setPWMPrescaler(FAN_CONTROL_PWM[fan],FAN_CONTROL_PWM_PRESCALE[fan]); // Sets 31.25KHz / 256 = 122Hz Frequency
  }
  
  // setup a timer that runs every 50 ms - To Set Fan Speed
  fanControlTimer = timer.setInterval(FAN_CONTROL_PERIOD,readInputSetFanPWM);
}

// Just Default Mid Range PWM Values
byte currentPWM[] = { 30, 30, 30 }; 

byte getCurrentPWM(byte fan) {
  return currentPWM[fan];
}

#ifdef LEGACYBOARD

//
// --------------------------
// Fan Output Voltage CONTROL
// --------------------------
//

// PRIVATE ------------------

// TODO The following is Legacy Code (LM317) eventually delete

void readInputSetFanPWM() {
  
#ifdef LEGACY-RPM

  // Average to the Two Fans RPMS Speeds
  int currentFanValue = ( getFanRPM(0) + getFanRPM(1) ) / 2;
  
#else

  // target centi Voltage produced by the LM317 
  // *2 Voltage Divider *100 CentiVolt
  int currentFanValue = 200.0f * readVoltage(FAN_OUTPUT_AIN);
  
#endif
  
  //byte factor = 1;
  byte factor = compareCurrentAndTarget(currentFanValue,targetFanValue[0]);
  
  if ( currentFanValue > targetFanValue[0] ) {
    
    // Slow fans down slowely, rather than hard off.
    // increasing PWM duty, lowers the voltage
    currentPWM[0] = currentPWM[0]<=(60-factor) ? currentPWM[0]+factor : 60;
    
  } else if ( currentFanValue < targetFanValue[0] ) {
    
    // Slowly ramp up the fan speed, rather than hard on.
    // decreasing PWM duty, increases the voltage
    currentPWM[0] = currentPWM[0]>=factor ? currentPWM[0]-factor : 0;
  }

  if ( debugFans ) {  
    //Serial.print(getTemp1());
    //Serial.print(F("\t"));
    //Serial.print(getTemp2());
    //Serial.print(F("\t"));
    Serial.print(factor);
    Serial.print(F("\t"));
    Serial.print(targetFanValue[0]);
    Serial.print(F("\t"));
    Serial.print(currentFanValue);
    Serial.print(F("\t"));
    Serial.print(currentPWM[0]); // Current PWM
    Serial.println(); 
  }
  
  // write the PWM  
  analogWrite(FAN_CONTROL_PWM[0],currentPWM[0]);
}

#else 

//
// ----------------------
// FAN OUTPUT RPM CONTROL
// ----------------------
// 

// PRIVATE ------------------

// called by a timer every second
void readInputSetFanPWM() {
  
  for ( byte fan=0; fan<FAN_COUNT; fan++ ) {

    // target RPM if fans are not active
    int currentFanValue = getFanRPM(fan);
  
    // what is a easonable amount to change PWM by
    byte factor = compareCurrentAndTarget(currentFanValue,targetFanValue[fan]);
  
    // todo consider damping the Fan RPM Input by shortening the sample time of the fan
    // relative to the last PWM adjustment, i.e. adjust every 2 seconds, but sample only last 1 second FAN RPM
    // the point being that the fan takes time to adjust.
    
    // todo need to put a scope on this and measure
    // VOLTAGE profile of the fan
    // PWM signal coming off the Arduino.
    // RPM of the Fan, pullup resistor, or could get arduino tom monitor it.
    // Voltage of the control Signal.
    
    // todo sketch to control the fans as per this sketch with VIN (POT) to control RPM
    // todo with full monitoring.
    // Event just monitor RPM, and see what varianc we get from one second to the next. 
  
    if ( currentFanValue < targetFanValue[fan] ) {
    
      // Slow fans down slowely, rather than hard off.
      // increasing PWM duty, lowers the voltage
      currentPWM[fan] = currentPWM[fan]<(255-factor) ? currentPWM[fan]+factor : 255;
    
    } else if ( currentFanValue > targetFanValue[fan] ) {
    
      // Slowly ramp up the fan speed, rather than hard on.
      // decreasing PWM duty, increases the voltage
      currentPWM[fan] = currentPWM[fan]>factor ? currentPWM[fan]-factor : 0;
    }
  
    // write the PWM  
    analogWrite(FAN_CONTROL_PWM[fan],currentPWM[fan]);
  }
}

#endif

// <5% - then 0
// <10% - then just inc / dec by 1
// <20% - 3
// <50% - 5
// >50% - 10
byte compareCurrentAndTarget(int current, int target) {
  
  int diff = current-target;
  if (diff < 0) diff = diff * -1;
  byte percent = 100.0f* (float)diff / (float)target;
  
  if (percent<5) {
    return 0;
  } else if (percent <20) {
    return 1;
  } else if (percent <30) {
    return 2;
  } else if (percent <40) {
    return 3;
  } else if (percent <50) {
    return 4;
  }
  return 5;
}

//
// ------------
// CHIME OUTPUT
// ------------
//

unsigned int delayBeforeChime;

void initiateTimedChime( unsigned int delayBefore ) {
  
   // DEBUG
  DEBUG_PRINTF("CHIME");
  
    // set the time to wait
  delayBeforeChime = delayBefore + 5; // NOTE delay cannot be 0, as this breaks

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

    return CHIME_ACTIVATION_TIME;
    
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

#define LEDMODE_NORMAL 0
#define LEDMODE_BREATH 10
#define LEDMODE_RAMPUP 20 
#define LEDMODE_RAMPDN 30

byte ledBrightnessMode = LEDMODE_NORMAL; // are we using any special features
byte targetLEDBright = 0; // what the brightness should be, smoothing

byte ledFlashCount = 0; // how many flashes are left
unsigned long ledFlashStartTime = 0; // is there a duration of the effect (fade)

/**
 * Turns off the front panel LED
 */
void deactivateFrontPanelLED() {  
  setFrontPanelLEDBright(0);
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

//period that the LED Timer Runs
#define LED_TIMER_PERIOD 5

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
  ledBrightnessTimer = timer.setInterval(LED_TIMER_PERIOD,ledBrightnessCallback);
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
  long timeInBreathCycle = timeInCycle % LED_BREATH_CYCLE_TIME;
  
  static float breathInTime = LED_BREATH_CYCLE_TIME * 3 / 10;
  static float breathOutTime = LED_BREATH_CYCLE_TIME * 7 / 10;
  
  // when in the first 1.5 seconds
  if ( timeInBreathCycle <= breathInTime ) {

    // simple cos function that ramps up    
    return computeTrigFactor( (float)timeInBreathCycle/breathInTime );
    
  } else {
    
    // convert to a decreasing value from 1 to 0, cos function raised to power 1.8
    return pow( computeTrigFactor((float)(LED_BREATH_CYCLE_TIME-timeInBreathCycle)/breathOutTime), 1.8f );
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

  if (cmd.equals("MP")) {
    initiateTimedChime(0); // Play Chime
    
  } else if (firstCmd.equals("B")) {
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
    if (hrs<10) Serial.print(ZERO);
    Serial.print(hrs);
    Serial.print(COLON);    
    if (mins<10) Serial.print(ZERO);
    Serial.print(mins);
    if (days==0) {
      Serial.print(COLON);    
      if (secs<10) Serial.print(ZERO);
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



