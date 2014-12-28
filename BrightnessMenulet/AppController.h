#import <Cocoa/Cocoa.h>
#include "arduino-serial-lib.h"
//#include "ORSSerialPort.h"

//@interface AppController : NSResponder {
@interface AppController : NSApplication {
    
    // Serial Port
    int serialHandle;
    NSString *serialPort;
    int serialBaud;
    
	/* Our outlets which allow us to access the interface */
	IBOutlet NSMenu *statusMenu;
	
	/* The other stuff :P */
	NSStatusItem *statusItem;
    
    /* Images */
	NSImage *statusImage;
	NSImage *statusHighlightImage;
    
    /* Slider */
	IBOutlet NSSlider *mySlider;
}

/* called when the menu is clicked on */
- (void)menuNeedsUpdate:(NSMenu *)menu;

/* Our IBAction which will call the method when our slder is changed */
- (IBAction)sliderChanged:(id)sender;

//- (IBAction)prefs:(id)sender;
- (IBAction)prefs:(id)sender;

/* Our IBAction which will call the metdod on exit */
- (IBAction)exit:(id)sender;

- (void) brightnessDown;
- (void) brightnessUp;

// media key event method handler
// - (void)sendEvent:(NSEvent *)event;
// - (void)mediaKeyEvent:(int)key state:(BOOL)state;

//void MyDisplayReconfigurationCallBack (
//                                       CGDirectDisplayID display,
//                                       CGDisplayChangeSummaryFlags flags,
//                                       void *userInfo
//                                       );

@end