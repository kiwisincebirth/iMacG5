#import "AppController.h"
#include <IOKit/graphics/IOGraphicsLib.h>
#include "arduino-serial-lib.h"
//#include "ORSSerialPort.h"
//#include <IOKit/IOKitLib.h>
//#include <IOKit/IOTypes.h>
#import <IOKit/hidsystem/ev_keymap.h>

@implementation AppController

//
//Key Events
//

/*
- (void)sendEvent:(NSEvent *)event
{
    // Catch media key events
    if ([event type] == NSSystemDefined && [event subtype] == 8)
    {
        int keyCode = (([event data1] & 0xFFFF0000) >> 16);
        int keyFlags = ([event data1] & 0x0000FFFF);
        int keyState = (((keyFlags & 0xFF00) >> 8)) == 0xA;
        
        // Process the media key event and return
        [self mediaKeyEvent:keyCode state:keyState];
        
        return;
    }
    
    // Continue on to super
    [super sendEvent:event];
}

- (void)mediaKeyEvent:(int)key state:(BOOL)state
{
    NSLog(@"Media Key Event %i, state=%i",key,state);
    
 	switch( key )
	{
		case NX_KEYTYPE_PLAY:
			if( state == NO ) {
                NSLog(@"Play");
            }
            break;
            
        case NX_KEYTYPE_BRIGHTNESS_UP:
            if( state == 0 ) {
                [self brightnessUp];
                NSLog(@"Brightness Up");
            }
            break;
            
        case NX_KEYTYPE_BRIGHTNESS_DOWN:
            if( state == 0 ) {
                [self brightnessDown];
                NSLog(@"Brightness Down");
            }
            break;
	}
}
*/

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
    
    // set tooltips to ndicate the current brightness
    [self set_tooltips:initialBright];

    // set the slider to correct position
    [(id)mySlider setDoubleValue:initialBright];
    
	//Enables highlighting
	[statusItem setHighlightMode:YES];
	
    // not sure
	[(id)mySlider becomeFirstResponder];
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

/*
const float INTERACTIVE = 1.0;
const float BACKGROUND = 2.0;

- (void)bgThread:(NSConnection *)connection
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
	float wait_amount = BACKGROUND;
    Boolean currentSleepState = false;

    //
    // A Thread that runs in background and activates/deactivates inverter.
    //
    
	while (true) {

		// every 1.0 or 2.0s  seconds (depends on currentSleepState) poll for display sleep state
		NSDate *future = [NSDate dateWithTimeIntervalSinceNow: wait_amount ];
		[NSThread sleepUntilDate:future];
        wait_amount = currentSleepState? BACKGROUND:INTERACTIVE;
		      
        //Boolean newSleepState = (CGDisplayIsAsleep(CGMainDisplayID()) != 0);
        Boolean newSleepState = !(CGDisplayIsActive(CGMainDisplayID()));
        
        if ( newSleepState != currentSleepState ) {

            // only change the slider if brightness has changed
            //NSLog(@"Thread CGMainDisplayID %d",CGMainDisplayID() );
            //NSLog(@"Thread CGDisplayIsAsleep %d",CGDisplayIsAsleep(CGMainDisplayID) );

            // make sure serial port open
            if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);
            
            if (newSleepState == true) {
                // deactivae the inverter
                [self deactivate_inverter];
                NSLog(@"Display sleep %d",CGMainDisplayID() );
                currentSleepState = true;
            } else {
                // deactivae the inverter
                [self activate_inverter];
                NSLog(@"Display wake %d",CGMainDisplayID() );
                currentSleepState = false;
            }
        }
	}
	[pool release];
}
*/

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
    
    // get the three menu items
    NSMenuItem *seperatorMenuItem = [statusMenu itemAtIndex:1];
    NSMenuItem *prefsMenuItem = [statusMenu itemAtIndex:2];
    NSMenuItem *quitMenuItem = [statusMenu itemAtIndex:3];
    
    // enable the three menu items
    [seperatorMenuItem setHidden:shoudHideSecretMenu];
    [prefsMenuItem setHidden:shoudHideSecretMenu];
    [quitMenuItem setHidden:shoudHideSecretMenu];
    
    // set slider & tooltips to indicate the current brightness
    // (important, if the brightness was adjusted with the touch sensors)
	double arduinoBright = [self get_brightness]; // get current brightness by loading from the Arduino

    NSLog(@"menuNeedsUpdate brightness = %f", arduinoBright);

    [self set_tooltips:arduinoBright];            // set tooltips to indicate the current brightness
    [(id)mySlider setDoubleValue:arduinoBright];      // set the slider to correct position
}

- (void)menuWillOpen:(NSMenu *)menu {
    
    //double arduinoBright = [self get_brightness];
    //NSLog(@"Menu Will Open Brightness %f",arduinoBright);
    //NSLog(@"menu will open");
    
}

- (void)menuDidClose:(NSMenu *)menu {
    
    //
    // called after menu closes, thus we close any serial port connection
    //
    
    // close serial port
    // serialHandle = serialport_close(serialHandle);
    
    double arduinoBright = [self get_brightness];
    NSLog(@"Menu Did Close Brightness %f",arduinoBright);
}

//called on slider change
- (IBAction)sliderChanged:(id)sender{
	
    //
    // when the slider position chnages
    //
    
    // get value
	double value =  [sender doubleValue];
    
    // set Arduino Brightness and tooltips with current brightness
    [self set_brightness:value];
    [self set_tooltips:value];
}


- (void) configure_Port {
    
    //
    // configure Serial Ports
    //
    
    serialHandle = -1;
    serialBaud = 9600;
    serialPort = @"/dev/tty.usbmodem";
    
    // ITERATE
    
    NSString* file;
    NSString* path = @"/dev";
    NSDirectoryEnumerator* enumerator = [[NSFileManager defaultManager] enumeratorAtPath:path];
    while (file = [enumerator nextObject])
    {
        if ([file rangeOfString:@"tty.usbmodem"].location != NSNotFound) {
            serialPort = [@"/dev/" stringByAppendingString:file];
            NSLog(@"FOUND Serial Port %@",serialPort);
        }
    }
    
    //serialPort = @"/dev/tty.usbmodem411";
    //serialPort = @"/dev/tty.usbmodem1421";
    //serialPort = @"/dev/tty.usbmodem14411";
    //serialPort = @"/dev/tty.usbmodem1411";
    //serialPort = @"/dev/tty.usbmodem1d151"; //G4 Cube
    //serialPort = @"/dev/tty.usbmodem14331"; //HemiMac
}

// almost completely from:
- (double) get_brightness {
	
    //
    // Get Brightness from the Arduino
    //
    
    // make sure serial port open
    if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);

    // The command we send the the Serial port "B" Brihgtness "R" Read
    char buf[32] = {"BR\n"};
    serialport_write(serialHandle, buf);

    // read result from serial port, until receive \r
    serialport_read_until(serialHandle, buf, '\n', 32, 1000);
    
    // convert the value in the buffer to double and return
    NSString *val = [NSString stringWithCString:buf encoding:NSASCIIStringEncoding];
    
    return [val doubleValue];
}

- (void) set_tooltips:(double) new_brightness {

    //
    // set tool tip with brigetness
    //
    
    [statusItem setToolTip:[NSString stringWithFormat:@"Brightness %1.0f%%",new_brightness]];
}

- (void) set_brightness:(double) new_brightness {

    //
    // set the brightnes into the Arduino by sendig serial command
    //
    
    // make sure serial port open
    if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);

    // init a buffer with command "B" Brightness "W" Write, followed by brightness number the \n
    char buf[32];
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
        
        // The command we send the the Serial port "B" Brihgtness "u" Up
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
        
        // The command we send the the Serial port "B" Brihgtness "u" Up
        char buf[32] = {"B-\n"};
        serialport_write(serialHandle, buf);
        
        NSLog( @"Brightness Decreased");
    }
}

- (void) activate_inverter {
	
    //
    // Activate Inverter on Arduino
    //
    
    // make sure serial port open
    if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);
    
    // The command we send the the Serial port "B" Brihgtness "R" Read
    char buf[32] = {"BA\n"};
    serialport_write(serialHandle, buf);
    
    NSLog( @"Inverter Activated");
}

- (void) deactivate_inverter {
	
    //
    // De-Activate Inverter on Arduino
    //
    
    // make sure serial port open
    if (serialHandle<=0) serialHandle= serialport_init([serialPort UTF8String], serialBaud);
    
    // The command we send the the Serial port "B" Brihgtness "R" Read
    char buf[32] = {"BD\n"};
    serialport_write(serialHandle, buf);
    
    NSLog( @"Inverter De-Activated");
}

/* Our IBAction which will call the metdod on Prefs */
- (IBAction)prefs:(id)sender {
    
    //
    // Called from the CMD P Menu Item "Preferences"
    //

    NSLog(@"Prefs from Brightness");

    // TODO Write code to dispaly a preferences window with other functons    
}

- (IBAction)exit:(id)sender{
    
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