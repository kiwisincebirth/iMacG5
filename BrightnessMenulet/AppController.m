#import "AppController.h"
#include <IOKit/graphics/IOGraphicsLib.h>
#include "arduino-serial-lib.h"
//#include "ORSSerialPort.h"
//#include <IOKit/IOKitLib.h>
//#include <IOKit/IOTypes.h>
#import <IOKit/hidsystem/ev_keymap.h>

@implementation AppController

- (void) configure_Port {
    
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
            //NSString *USB = [NSString stringWithFormat:@"%@",serialPort];  //p
            //[ArduinoPortTextField setStringValue:USB];
            [ArduinoPortTextField setStringValue:serialPort]; // display the USB port in the preferences window
        }
    }
    
    //serialPort = @"/dev/tty.usbmodem411";
    //serialPort = @"/dev/tty.usbmodem1421";
    //serialPort = @"/dev/tty.usbmodem14411";
    //serialPort = @"/dev/tty.usbmodem1411";
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
    [self configure_Port];
    
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
    
    //
    // Creates Thread that controls shutting off inverter
    //
    // [NSThread detachNewThreadSelector:@selector(bgThread:) toTarget:self withObject:nil];
    
    // register for SCREEN sleep notification
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self selector:@selector(receiveScreenSleepNotification:) name:NSWorkspaceScreensDidSleepNotification object:NULL];
    
    // register for SCREEN wake notification
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self selector:@selector(receiveScreenWakeNotification:) name:NSWorkspaceScreensDidWakeNotification object:NULL];

    // register for SYSTEM sleep notification
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self selector:@selector(receiveSleepNotification:) name:NSWorkspaceWillSleepNotification object:NULL];
    
    // register for SYSTEM wake notification
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self selector:@selector(receiveWakeNotification:) name:NSWorkspaceDidWakeNotification object:NULL];

    // startup get initial brightness by loading from the Arduino
	double initialBright = [self get_brightness];
    
    // make sure inverter is active
    [self activate_inverter];
    
    // set tooltips to indicate the current brightness
    [self set_tooltips:initialBright];

    // set the slider to correct position
    [(id)brightnessSlider setDoubleValue:initialBright];
    
	//Enables highlighting
	[statusItem setHighlightMode:YES];
	
    // not sure
	[(id)brightnessSlider becomeFirstResponder];
    
    // enable 2s interval timer
    [self intervalTimer];
}


- (void)receiveScreenSleepNotification:(NSNotification *)notification
{
    NSLog(@"got Screen sleep notification");

    // deactivae the inverter
    [self deactivate_inverter];
    
    // close serial port
    serialHandle = serialport_close(serialHandle);
}


- (void)receiveScreenWakeNotification:(NSNotification *)notification
{
    NSLog(@"got Screen wake notification");

    // Configure Serial Ports ***
    [self configure_Port];
    
    // activae the inverter
    [self activate_inverter];
}


- (void)receiveSleepNotification:(NSNotification *)notification
{
    NSLog(@"got System Sleep notification");
    
    // deactivate the inverter
    //[self deactivate_inverter];
}


- (void)receiveWakeNotification:(NSNotification *)notification
{
    NSLog(@"got System Wake notification");
    
    // close serial port
    //serialHandle = serialport_close(serialHandle);
    
    //sleep(1); // is important. Otherwise the serial command can not be sent to the Arduino after system wake.
    
    // activate the inverter
    //[self activate_inverter];
}


- (void) menuNeedsUpdate: (NSMenu *)menu
{
    //
    // Called before the menu is displayed, after it is clicked
    // This is used to enable hidden menu items when holding down Option key
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
	arduinoBright = [self get_brightness]; // get current brightness by loading from the Arduino
    
    [self get_rpm]; // get current RPM and temperature readings by loading from the Arduino
    
    [self set_tooltips:arduinoBright];            // set tooltips to indicate the current brightness
   
    [(id)brightnessSlider setDoubleValue:arduinoBright];      // set the slider to correct position
}


- (void)menuWillOpen:(NSMenu *)menu {
    
    //double arduinoBright = [self get_brightness];
    //NSLog(@"Menu Will Open Brightness %f",arduinoBright);
    
}


- (void)menuDidClose:(NSMenu *)menu {
    
    //
    // called after menu closes, thus we close any serial port connection
    //
    
    // close serial port
    // serialHandle = serialport_close(serialHandle);
    
    NSLog(@"Menu did close");
    
    arduinoBright = [self get_brightness];
    
    [self get_rpm]; // get current RPM and temperature readings by loading from the Arduino
    }


//called on slider change
- (IBAction)sliderChanged:(id)sender{
	
    //
    // when the slider position changes
    //
    
    // get value
	int brightness = [sender doubleValue];
    
    // set Arduino Brightness and tooltips with current brightness
    [self set_brightness:brightness];
    [self set_tooltips:brightness];
}


- (double) get_brightness {
	
    //
    // Get Brightness from the Arduino
    //
    
    // make sure serial port open
    if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);

    // The command we send to the Serial port "F" Fan "R" Read
    char buf[] = {"BR\n"};
    serialport_write(serialHandle, buf);

    // read result from serial port, until receive \n
    serialport_read_until(serialHandle, buf, '\n', 32, 1000);
    
    // convert the value in the buffer to 7bit ASCII
    NSString *val = [NSString stringWithCString:buf encoding:NSASCIIStringEncoding];
    
    // return the ASCII value as double
    return [val doubleValue];
    }


- (void) get_rpm {
    
    //
    // Get fan RPM and temperature sensor readings from the Arduino
    //
    
    // make sure serial port open
    if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);
    
    // The command we send to the Serial port "F" Fan "R" Read
    char buf[] = {"FR\n"};
    serialport_write(serialHandle, buf);
   
    // read result from serial port until receive ; or \n, then convert the value in the buffer to 7bit ASCII
    serialport_read_until(serialHandle, buf, ';', 32, 1000);
    NSString *val1 = [NSString stringWithCString:buf encoding:NSASCIIStringEncoding];
    
    serialport_read_until(serialHandle, buf, ';', 32, 1000);
    NSString *val2 = [NSString stringWithCString:buf encoding:NSASCIIStringEncoding];
    
    serialport_read_until(serialHandle, buf, ';', 32, 1000);
    NSString *val3 = [NSString stringWithCString:buf encoding:NSASCIIStringEncoding];
    
    serialport_read_until(serialHandle, buf, ';', 32, 1000);
    NSString *val4 = [NSString stringWithCString:buf encoding:NSASCIIStringEncoding];
    
    serialport_read_until(serialHandle, buf, ';', 32, 1000);
    NSString *val5 = [NSString stringWithCString:buf encoding:NSASCIIStringEncoding];
    
    serialport_read_until(serialHandle, buf, '\n', 32, 1000);
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


- (void) set_tooltips:(double) new_brightness {

    //
    // set tool tip with brightness
    //
    
    [statusItem setToolTip:[NSString stringWithFormat:@"Brightness %1.0f%%",new_brightness]];
}


- (void) set_brightness:(double) new_brightness {

    //
    // set the brightness into the Arduino by sendig serial command (in accordance with slider position)
    //
    
    // make sure serial port open
    if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);

    // init a buffer with command "B" Brightness "W" Write, followed by brightness number and the \n
    char buf[8];
    sprintf(buf, "BW%1.0f\n", new_brightness);
    
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
        char buf[] = {"B+\n"};
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
        char buf[] = {"B-\n"};
        serialport_write(serialHandle, buf);
        
        NSLog( @"Brightness Decreased");
    }
}


- (void) activate_inverter {
	
    //
    // Activate Inverter on Arduino
    //
    
    Boolean confirmed = false;
    int loop = 0;
    
    while(! confirmed) {
        // make sure serial port open
        if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);
    
        // The command we send to the Serial port "B" Brightness "A" Activate
        char buf[] = {"BA\n"};
        serialport_write(serialHandle, buf);
        loop = loop + 1;
        
        // read result from serial port, until receive \r
        serialport_read_until(serialHandle, buf, '\n', 4, 500);
        
        // Then Check for a Message "OK"
        if (buf[0]=='O' && buf[1]=='K') {
            // IF received OK, then don't resend
            confirmed = true;
        }
        // Timeout after 10 loops!
        if (loop >= 10) {
            NSLog( @"Serial Communication Timeout!");
            confirmed = true;
        }
    }
 
    
    NSLog( @"Inverter Activated");
}


- (void) deactivate_inverter {
    
    //
    // De-Activate Inverter on Arduino
    //
    
    Boolean confirmed = false;
    int loop = 0;

    
    while(! confirmed) {
        // make sure serial port open
        if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);
        
        // The command we send the the Serial port "B" Brightness "D" Deactivate
        char buf[] = {"BD\n"};
        serialport_write(serialHandle, buf);
        loop = loop + 1;
    
        // read result from serial port, until receive \r
        serialport_read_until(serialHandle, buf, '\n', 4, 500);
        
        // Then Check for a Message "OK"
        if (buf[0]=='O' && buf[1]=='K') {
            // IF received OK, then don't resend
            confirmed = true;
        }
        // Timeout after 10 loops!
        if (loop >= 10) {
            NSLog( @"Serial Communication Timeout!");
            confirmed = true;
        }
    }

    NSLog( @"Inverter De-Activated");
    
}

- (void) intervalTimer {
    //
    // 2s Interval Timer
    //
    if ([ReadingsWindow isVisible]) {
        [self get_rpm]; // get current RPM and temperature readings by loading from the Arduino
        NSLog( @"Readings refreshed");
    }

    // loop this function every 2s in order to refresh the readings!
    dispatch_async(dispatch_get_main_queue(), ^{
        [NSTimer scheduledTimerWithTimeInterval:2.0f target:self selector:@selector(intervalTimer) userInfo:nil repeats:NO];
    });
}


- (IBAction)chimeTestClicked:(id)sender {
    
    //
    // Test Chime
    //
    
    // make sure serial port open
    if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);{
        
        // The command we send to the Serial port "M" Melody "P" Play
        char buf[] = {"MP\n"};
        serialport_write(serialHandle, buf);    }
    NSLog( @"Chime Test");
}

- (IBAction)doorTestClicked:(id)sender {
    
    //
    // Test Door
    //
    
    // make sure serial port open
    if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);{
        
        // The command we send to the Serial port "D" door "S" switch
        char buf[] = {"DS\n"};
        serialport_write(serialHandle, buf);    }
    NSLog( @"Door Test");
}

- (IBAction)TerminalTextFieldEnter:(NSTextField *)sender {
    
    //
    // Terminal
    //
    
    // make sure serial port open
    if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);{
       
        NSString *serialCommand = [terminalTextField stringValue];//read the text field
        const char *buffer = [serialCommand UTF8String];
        char buf[128];
        sprintf(buf, "%s\n", buffer);           //format the command
        serialport_write(serialHandle, buf);    //send the command
        NSLog(@"Terminal Message sent: %s",buf);
    
    
        // read result from serial port, until receive \n or timeout
        serialport_read_until(serialHandle, buf, '\n', 128, 500);
        NSString *val1 = [NSString stringWithCString:buf encoding:NSASCIIStringEncoding];
        [terminalAnswerTextField setStringValue:val1];  //display the answer from the Arduino
        NSLog(@"Terminal Message received: %s",buf);
    }
}



- (IBAction)readingsClicked:(id)sender {
    
    //
    // Called from the CMD R Menu Item "Readings"
    //
    
    NSLog(@"Readings from Brightness");
    [NSApp activateIgnoringOtherApps:YES];
    [ReadingsWindow makeKeyAndOrderFront:sender];
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