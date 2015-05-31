#import <Cocoa/Cocoa.h>
#include "arduino-serial-lib.h"

@interface AppController : NSApplication {
    
    // Declare global variables
      
    double arduinoBright;
    
    int fan1Rpm;
    int fan2Rpm;
    int fan3Rpm;
    float sens0Temp;
    float sens1Temp;
    float sens2Temp;
    
    NSString *terminalCommand;
    
    // Serial Port
    NSString *serialPort;
    int serialBaud;
    int serialHandle;
    
    // Readings Window
    IBOutlet NSWindow *ReadingsWindow;
    
    // Preferences Window
    IBOutlet NSWindow *PreferencesWindow;
    
	// Our outlets which allow us to access the interface
	IBOutlet NSMenu *statusMenu;
	
	// Menu Status Item
	NSStatusItem *statusItem;
    
    // Images
	NSImage *statusImage;
	NSImage *statusHighlightImage;
    
    // Slider
	IBOutlet NSSlider *brightnessSlider;
    
    // Readings Text Fields
    IBOutlet NSTextField *rpm1TextField;
    IBOutlet NSTextField *rpm2TextField;
    IBOutlet NSTextField *rpm3TextField;
    
    IBOutlet NSTextField *sens0TempTextField;
    IBOutlet NSTextField *sens1TempTextField;
    IBOutlet NSTextField *sens2TempTextField;
    
    //Preferences Text Fields
    IBOutlet NSTextField *ArduinoPortTextField;
    
    IBOutlet NSTextField *terminalTextField;
    IBOutlet NSTextView *terminalAnswerTextView;
   }

// called when the menu is clicked on
- (void)menuNeedsUpdate:(NSMenu *)menu;

// called when the slider position is changed
- (IBAction)sliderChanged:(id)sender;

// called when the Readings menu is clicked on
- (IBAction)readingsClicked:(id)sender;

// called when the Preferences menu is clicked on
- (IBAction)prefsClicked:(id)sender;

// called when the Exit menu is clicked on
- (IBAction)exitClicked:(id)sender;

// called when the Port Scan Button is clicked on
- (IBAction)portScanClicked:(id)sender;

// called when the Uptime Button is clicked on
- (IBAction)systemUptimeClicked:(id)sender;

// called when the Temperature Button is clicked on
- (IBAction)sysytemTemperatureClicked:(id)sender;

// called when the Statistics Button is clicked on
- (IBAction)systemStatisticsClicked:(id)sender;

// called when the Chime Test Button is clicked on
- (IBAction)chimeTestClicked:(id)sender;

// called when the Door Test Button is clicked on
- (IBAction)doorTestClicked:(id)sender;

// called when the LED Test Button is clicked on
- (IBAction)ledTestClicked:(id)sender;

// called when enter is pressed within the terminal commands text field
- (IBAction)TerminalTextFieldEnter:(NSTextField *)sender;

@end