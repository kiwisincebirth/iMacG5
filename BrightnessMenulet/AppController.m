#import "AppController.h"
#include <IOKit/graphics/IOGraphicsLib.h>
#include "arduino-serial-lib.h"
//#include "ORSSerialPort.h"
//#include <IOKit/IOKitLib.h>
//#include <IOKit/IOTypes.h>
#import <IOKit/hidsystem/ev_keymap.h>

@implementation AppController

- (void) configurePort {
    
    //
    // configure Serial Ports
    //
    
    serialHandle = -1;
    serialBaud = 115200;
    //serialPort = @"/dev/tty.usbmodem";
    
    
    // ITERATE
    
    NSString* file;
    NSString* path = @"/dev";
    NSDirectoryEnumerator* enumerator = [[NSFileManager defaultManager] enumeratorAtPath:path];
    while (file = [enumerator nextObject])
    {
        if ([file rangeOfString:@"tty.usbmodem"].location != NSNotFound) {
            serialPort = [@"/dev/" stringByAppendingString:file];
            NSLog(@"FOUND Serial Port %@",serialPort);
            [ArduinoPortTextField setStringValue:serialPort]; // display the USB port in the preferences window
        }
    }
    //serialPort = @"/dev/tty.usbmodem1d151"; //G4 Cube
    //serialPort = @"/dev/tty.usbmodem14331"; //HemiMac
}


- (void) awakeFromNib{
	
    //
    // Called when the Menu Starts Up
    // Responsible for initialisation
    //
    
    NSLog(@"Hello from Brightness");
    
    // Configure Serial Ports
    [self configurePort];
    
	//Create the NSStatusBar and set its length
	statusItem = [[[NSStatusBar systemStatusBar] statusItemWithLength:NSSquareStatusItemLength] retain];

    //Used to detect where our files are
	NSBundle *bundle = [NSBundle mainBundle];
	
	//Allocates and loads the images into the application which will be used for our NSStatusItem
	statusImage = [[NSImage alloc] initWithContentsOfFile:[bundle pathForResource:@"icon" ofType:@"png"]];
	statusHighlightImage = [[NSImage alloc] initWithContentsOfFile:[bundle pathForResource:@"icon-alt" ofType:@"png"]];
	
    // 10.10 or higher, so setTemplate: is safe
    [statusImage setTemplate:YES];
    
	//Sets the images in our NSStatusItem
	[statusItem setImage:statusImage];
	[statusItem setAlternateImage:statusHighlightImage];
	
	//Tells the NSStatusItem what menu to load
	[statusItem setMenu:statusMenu];
    
    // need a delegate for the menu
    [statusMenu setDelegate:(id)self];
    
    // register for SCREEN sleep notification
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self selector:@selector(receiveScreenSleepNotification:) name:NSWorkspaceScreensDidSleepNotification object:NULL];
    
    // register for SCREEN wake notification
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self selector:@selector(receiveScreenWakeNotification:) name:NSWorkspaceScreensDidWakeNotification object:NULL];
/*
    // register for SYSTEM sleep notification
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self selector:@selector(receiveSleepNotification:) name:NSWorkspaceWillSleepNotification object:NULL];
    
    // register for SYSTEM wake notification
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self selector:@selector(receiveWakeNotification:) name:NSWorkspaceDidWakeNotification object:NULL];
*/
    // make sure inverter is active
    [self activateInverter];
    
    // delay 0.3s (time for sensing the serial data)
    NSDate *future = [NSDate dateWithTimeIntervalSinceNow: 0.3 ];
    [NSThread sleepUntilDate:future];
    
    // startup get initial brightness by loading from the Arduino
	arduinoBright = [self getBrightness];
    
    // set tooltips to indicate the current brightness
    [self setTooltips:arduinoBright];

    // set the slider to correct position
    [(id)brightnessSlider setDoubleValue:arduinoBright];
    
	//Enables highlighting
	[statusItem setHighlightMode:YES];
	
    // not sure
	//[(id)brightnessSlider becomeFirstResponder];
    
    // enable 2s interval timer
    [self intervalTimer];
}


- (void)receiveScreenSleepNotification:(NSNotification *)notification
{
    NSLog(@"Got Screen Sleep Notification");

    // deactivate the inverter
    [self deactivateInverter];
    
    // close readings window (important, because the serial port will be closed after 1s)
    [ReadingsWindow close];
    
    // delay 1s (time for sending the serial data)
    NSDate *future = [NSDate dateWithTimeIntervalSinceNow: 1.0 ];
    [NSThread sleepUntilDate:future];
    
    // close serial port
    NSLog(@"Arduino serial port closed");
    serialHandle = serialport_close(serialHandle);
}


- (void)receiveScreenWakeNotification:(NSNotification *)notification
{
    NSLog(@"Got Screen Wake Notification");
    
    // delay 2s (for USB initialization)
    NSDate *future = [NSDate dateWithTimeIntervalSinceNow: 2.0 ];
    [NSThread sleepUntilDate:future];
    
    // then activate the inverter
    [self activateInverter];
}

/*
- (void)receiveSleepNotification:(NSNotification *)notification
{
    NSLog(@"got system sleep notification - no action");
}


- (void)receiveWakeNotification:(NSNotification *)notification
{
    NSLog(@"got system wake notification - no action");
}
*/

- (void) menuNeedsUpdate: (NSMenu *)menu
{
    //
    // Called before the menu is displayed, after it is clicked
    // This is used to enable hidden menu items when holding down Option (Alt) Key
    //
    
    // loads keyboard flags
    NSUInteger flags = ([NSEvent modifierFlags] & NSDeviceIndependentModifierFlagsMask);
    
    // We negate the value below because, if flags == NSAlternateKeyMask is TRUE, that
    // means the user has the Option key held, and wants to see the secret menu, so we
    // need shoudHideSecretMenu to be FALSE, so we just negate the value.
    BOOL shoudHideSecretMenu = !(flags == NSAlternateKeyMask);
    
    NSLog(@"menuNeedsUpdate shoudHideSecretMenu = %d", shoudHideSecretMenu);
    
    // get the menu items
    NSMenuItem *seperatorMenuItem = [statusMenu itemAtIndex:1];
    NSMenuItem *readingsMenuItem = [statusMenu itemAtIndex:2];
    NSMenuItem *prefsMenuItem = [statusMenu itemAtIndex:3];
    NSMenuItem *quitMenuItem = [statusMenu itemAtIndex:4];
    
    // hide the menu items
    [seperatorMenuItem setHidden:shoudHideSecretMenu];
    [readingsMenuItem setHidden:shoudHideSecretMenu];
    [prefsMenuItem setHidden:shoudHideSecretMenu];
    [quitMenuItem setHidden:shoudHideSecretMenu];
    
    // set slider & tooltips to indicate the current brightness
    // (important, if the brightness was adjusted with the touch sensors)
	arduinoBright = [self getBrightness]; // get current brightness by loading from the Arduino
    
    [self setTooltips:arduinoBright];            // set tooltips to indicate the current brightness
   
    [(id)brightnessSlider setDoubleValue:arduinoBright];      // set the slider to correct position
}


- (void)menuDidClose:(NSMenu *)menu {
    
    //
    // called after menu closes
    //
    
    NSLog(@"Menu did close - No Action");
    
    //arduinoBright = [self getBrightness];
    
    //[self getRpm]; // get current RPM and temperature readings by loading from the Arduino
    }


//called on slider change
- (IBAction)sliderChanged:(id)sender{
	
    //
    // when the slider position changes
    //
    
    // get value
	int brightness = [sender doubleValue];
    
    // set Arduino Brightness and tooltips with current brightness
    [self setBrightness:brightness];
    [self setTooltips:brightness];
}


- (double) getBrightness {
	
    //
    // Get Brightness from the Arduino
    //
    
    // make sure serial port open
    if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);

    // The command we send to the Serial port "B" Brightness "R" Read
    char buf[32] = {"BR\n"};
    serialport_write(serialHandle, buf);

    // read result from serial port, until receive \n
    serialport_read_until(serialHandle, buf, '\n', 30, 1000);
    
    // convert the value in the buffer to 7bit ASCII
    NSString *val = [NSString stringWithCString:buf encoding:NSASCIIStringEncoding];
    
    // return the ASCII value as double
    return [val doubleValue];
    }


- (void) getRpm {
    
    //
    // Get fan RPM and temperature sensor readings from the Arduino
    //
    
    // make sure serial port open
    if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);
    
    // The command we send to the Serial port "F" Fan "R" Read
    char buf[32] = {"FR\n"};
    serialport_write(serialHandle, buf);
   
    // read result from serial port until receive ; or \n, then convert the value in the buffer to 7bit ASCII
    serialport_read_until(serialHandle, buf, ';', 30, 100);
    NSString *val1 = [NSString stringWithCString:buf encoding:NSASCIIStringEncoding];
    
    serialport_read_until(serialHandle, buf, ';', 30, 100);
    NSString *val2 = [NSString stringWithCString:buf encoding:NSASCIIStringEncoding];
    
    serialport_read_until(serialHandle, buf, ';', 30, 100);
    NSString *val3 = [NSString stringWithCString:buf encoding:NSASCIIStringEncoding];
    
    serialport_read_until(serialHandle, buf, ';', 30, 100);
    NSString *val4 = [NSString stringWithCString:buf encoding:NSASCIIStringEncoding];
    
    serialport_read_until(serialHandle, buf, ';', 30, 100);
    NSString *val5 = [NSString stringWithCString:buf encoding:NSASCIIStringEncoding];
    
    serialport_read_until(serialHandle, buf, '\n', 30, 100);
    NSString *val6 = [NSString stringWithCString:buf encoding:NSASCIIStringEncoding];
    
    fan1Rpm = [val1 doubleValue];
    fan2Rpm = [val2 doubleValue];
    fan3Rpm = [val3 doubleValue];
    sens0Temp = [val4 floatValue];
    sens1Temp = [val5 floatValue];
    sens2Temp = [val6 floatValue];
    
    [rpm1TextField setIntValue:fan1Rpm];    // set the TextFields in the "Readings" window to indicate the current readings
    [rpm2TextField setIntValue:fan2Rpm];
    [rpm3TextField setIntValue:fan3Rpm];
    [sens0TempTextField setFloatValue:sens0Temp];
    [sens1TempTextField setFloatValue:sens1Temp];
    [sens2TempTextField setFloatValue:sens2Temp];

    NSLog(@"Brightness = %f", arduinoBright);
    NSLog(@"Fan1 RPM = %i", fan1Rpm);
    NSLog(@"Fan2 RPM = %i", fan2Rpm);
    NSLog(@"Fan3 RPM = %i", fan3Rpm);
    NSLog(@"Temperature 0 [°C] = %f", sens0Temp);
    NSLog(@"Temperature 1 [°C] = %f", sens1Temp);
    NSLog(@"Temperature 2 [°C] = %f", sens2Temp);
}


- (void) setTooltips:(double) newBrightness {

    //
    // set tool tip with brightness
    //
    
    [statusItem setToolTip:[NSString stringWithFormat:@"Brightness %1.0f%%",newBrightness]];
}


- (void) setBrightness:(double) newBrightness {

    //
    // set the brightness into the Arduino by sendig serial command (in accordance with slider position)
    //
    
    // make sure serial port open
    if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);

    // init a buffer with command "B" Brightness "W" Write, followed by brightness number and the \n
    char buf[32];
    sprintf(buf, "BW%1.0f\n", newBrightness);
    
    // send the command via serial port
    serialport_write(serialHandle, buf);
}


- (void) brightnessUp {
	
    //
    // Increase Brightness on Arduino
    //
    
    // make sure serial port open
    if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);{
        
        // The command we send to the Serial port "B" Brihgtness "+" Up
        char buf[32] = {"B+\n"};
        serialport_write(serialHandle, buf);
        
        NSLog( @"Brightness Increased");
    }
}


- (void) brightnessDown {
	
    //
    // Decrease Brightness on Arduino
    //
    
    // make sure serial port open
    if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);{
        
        // The command we send to the Serial port "B" Brightness "-" Down
        char buf[32] = {"B-\n"};
        serialport_write(serialHandle, buf);
        
        NSLog( @"Brightness Decreased");
    }
}


- (void) activateInverter {
	
    //
    // Activate Inverter on Arduino
    //
    
    Boolean confirmed = false;
    int loop = 0;
    
    while(! confirmed) {
        // make sure serial port open
        if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);
    
        // The command we send to the Serial port "B" Brightness "A" Activate
        char buf[32] = {"BA\n"};
        serialport_write(serialHandle, buf);
        loop = loop + 1;
        
        // read result from serial port, until receive \n
        serialport_read_until(serialHandle, buf, '\n', 4, 500);
        
        // Then Check for a Message "OK"
        if (buf[0]=='O' && buf[1]=='K') {
            // IF received OK, then don't resend
            confirmed = true;
        }
        // Timeout after 10 loops!
        if (loop >= 10) {
            NSLog( @"Serial Communication Timeout (BA)");
            confirmed = true;
        }
    }
    NSLog( @"Inverter Activated");
}


- (void) deactivateInverter {
    
    //
    // De-Activate Inverter on Arduino
    //
    
    Boolean confirmed = false;
    int loop = 0;

    
    while(! confirmed) {
        // make sure serial port open
        if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);
        
        // The command we send the the Serial port "B" Brightness "D" Deactivate
        char buf[32] = {"BD\n"};
        serialport_write(serialHandle, buf);
        loop = loop + 1;
    
        // read result from serial port, until receive \n
        serialport_read_until(serialHandle, buf, '\n', 4, 500);
        
        // Then Check for a Message "OK"
        if (buf[0]=='O' && buf[1]=='K') {
            // IF received OK, then don't resend
            confirmed = true;
        }
        // Timeout after 10 loops!
        if (loop >= 10) {
            NSLog( @"Serial Communication Timeout (BD)");
            confirmed = true;
        }
    }
    NSLog( @"Inverter De-Activated");
}


- (void) receiveTerminalAnswer {
    
    //
    // Receive the terminal answer from the Arduino
    //
    
    // read result from serial port, until receive \ or timeout
    char bufr[1024];
    serialport_read_until(serialHandle, bufr, '\f', 1024, 100);
    NSString *val1 = [NSString stringWithCString:bufr encoding:NSASCIIStringEncoding];
    [terminalAnswerTextView setString:val1];  //display the answer from the Arduino
    NSLog(@"Terminal Message received: %s",bufr);
}

- (void) intervalTimer {
    //
    // 2s Interval Timer
    //
    if ([ReadingsWindow isVisible]) {
        [self getRpm]; // get current RPM and temperature readings by loading from the Arduino
        //NSLog( @"Readings refreshed");
    }

    // loop this function every 2s in order to refresh the readings!
    dispatch_async(dispatch_get_main_queue(), ^{
        [NSTimer scheduledTimerWithTimeInterval:2.0f target:self selector:@selector(intervalTimer) userInfo:nil repeats:NO];
    });
}


- (IBAction)portScanClicked:(id)sender {
    
    //
    // Port Scan Button
    //
    
    [self configurePort];
}

- (IBAction)systemUptimeClicked:(id)sender {
    
    //
    // Uptime Button
    //
    
    // make sure serial port open
    if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);{
        
        // The command we send to the Serial port "M" Melody "P" Play
        char buf[32] = {"SU\n"};
        serialport_write(serialHandle, buf);    }
    NSLog( @"System Uptime");
    
    // receive answer from the Arduino
    [self receiveTerminalAnswer];

}


- (IBAction)sysytemTemperatureClicked:(id)sender {
    
    //
    // Temperature Button
    //
    
    // make sure serial port open
    if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);{
        
        // The command we send to the Serial port "M" Melody "P" Play
        char buf[32] = {"ST\n"};
        serialport_write(serialHandle, buf);    }
    NSLog( @"System Temperature");
    
    // receive answer from the Arduino
    [self receiveTerminalAnswer];
}


- (IBAction)systemStatisticsClicked:(id)sender {
    
    //
    // Statistics Button
    //
    
    // make sure serial port open
    if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);{
        
        // The command we send to the Serial port "M" Melody "P" Play
        char buf[32] = {"SS\n"};
        serialport_write(serialHandle, buf);    }
    NSLog( @"System Statistics");
    
    // receive answer from the Arduino
    [self receiveTerminalAnswer];
}


- (IBAction)chimeTestClicked:(id)sender {
    
    //
    // Test Chime
    //
    
    // make sure serial port open
    if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);{
        
        // The command we send to the Serial port "M" Melody "P" Play
        char buf[32] = {"MP\n"};
        serialport_write(serialHandle, buf);    }
    NSLog( @"Chime Test");
    
    // receive answer from the Arduino
    [self receiveTerminalAnswer];
}


- (IBAction)doorTestClicked:(id)sender {
    
    //
    // Test Door
    //
    
    // make sure serial port open
    if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);{
        
        // The command we send to the Serial port "D" door "S" switch
        char buf[32] = {"DS\n"};
        serialport_write(serialHandle, buf);    }
    NSLog( @"Door Test");
    
    // receive answer from the Arduino
    [self receiveTerminalAnswer];
}

- (IBAction)ledTestClicked:(id)sender {
    
    //
    // Test LED
    //
    
    // make sure serial port open
    if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);{
        
        // The command we send to the Serial port "D" door "S" switch
        char buf[32] = {"LT\n"};
        serialport_write(serialHandle, buf);    }
    NSLog( @"LED Test");
    
    // receive answer from the Arduino
    [self receiveTerminalAnswer];
}


- (IBAction)TerminalTextFieldEnter:(NSTextField *)sender {
    
    //
    // Terminal Command
    //
    
    // make sure serial port open
    if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);{
       
        // send the command
        NSString *serialCommand = [terminalTextField stringValue];//read the text field
        const char *buffer = [serialCommand UTF8String];
        char buf[32];
        sprintf(buf, "%s\n", buffer);           //format the command
        serialport_write(serialHandle, buf);    //send the command
        NSLog(@"Terminal Message sent: %s",buf);
        
        // receive answer from the Arduino
        [self receiveTerminalAnswer];
    }
}


- (IBAction)readingsClicked:(id)sender {
    
    //
    // Called from the CMD R Menu Item "Readings"
    //
    
    NSLog(@"Readings from Brightness");
    [NSApp activateIgnoringOtherApps:YES];
    [ReadingsWindow makeKeyAndOrderFront:sender];
    
    [self getRpm];          // get current RPM and temperature readings by loading from the Arduino
}


- (IBAction)prefsClicked:(id)sender {
    
    //
    // Called from the CMD P Menu Item "Preferences"
    //
    
    NSLog(@"Preferences from Brightness");
    [NSApp activateIgnoringOtherApps:YES];
    [PreferencesWindow makeKeyAndOrderFront:sender];
}


- (IBAction)exitClicked:(id)sender{
    
    //
    // Called from the CMD Q Menu Item "Quit"
    //
    
	NSLog(@"Goodbye from Brightness");
    
    // close serial port
    serialHandle = serialport_close(serialHandle);

	// Quit the application
    exit(1);
}


- (void) dealloc {
	//Releases the 2 images we loaded into memory
	[statusImage release];
	[statusHighlightImage release];
	[super dealloc];
}

@end