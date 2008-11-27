#import "SettingsView.h"
#import "vMacApp.h"
#import <UIKit/UIPreferencesControlTableCell.h>
#import <UIKit/UISliderControl.h>
#import <UIKit/UISwitchControl.h>

extern NSString *kUIButtonBarButtonAction;
extern NSString *kUIButtonBarButtonInfo;
extern NSString *kUIButtonBarButtonInfoOffset;
extern NSString *kUIButtonBarButtonSelectedInfo;
extern NSString *kUIButtonBarButtonStyle;
extern NSString *kUIButtonBarButtonTag;
extern NSString *kUIButtonBarButtonTarget;
extern NSString *kUIButtonBarButtonTitle;
extern NSString *kUIButtonBarButtonTitleVerticalHeight;
extern NSString *kUIButtonBarButtonTitleWidth;
extern NSString *kUIButtonBarButtonType;

@implementation SettingsView

- (id)initWithFrame:(CGRect)frame {
    if (self = [super initWithFrame:frame]) {
        layouts = [[[vMacApp sharedInstance] availableKeyboardLayouts] retain];
        layoutIDs = [[[layouts allKeys] sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)] retain];
        defaults = [NSUserDefaults standardUserDefaults];
        
        // create nav bar
        navBar = [[UINavigationBar alloc] initWithFrame: CGRectMake(0.0, 0.0, frame.size.width, 48.0)];
        [navBar setDelegate:self];
        UINavigationItem *navItem = [[UINavigationItem alloc] initWithTitle:@"Settings"];
        [navBar pushNavigationItem: navItem];
        [navBar showButtonsWithLeftTitle: @"Close" rightTitle: nil  leftBack: YES];
        [self addSubview: navBar];
        [navItem autorelease];
        
        // create table
        CGRect tableRect = CGRectMake(0.0, 48.0, frame.size.width, frame.size.height-92.0);
        table = [[UIPreferencesTable alloc] initWithFrame: tableRect];
        [table setDelegate: self];
        [table setDataSource: self];
        [self addSubview: table];
        
        // create toolbar
        CGRect toolbarRect = CGRectMake(0.0, 320.0-44.0, frame.size.width, 44.0);
        NSArray *buttonBarButtons = [NSArray arrayWithObjects:
            [NSDictionary dictionaryWithObjectsAndKeys:
                self,                                   kUIButtonBarButtonTarget,
                @"buttonBarItemTapped:",                kUIButtonBarButtonAction,
                [NSNumber numberWithUnsignedInt:1],     kUIButtonBarButtonTag,
                [NSNumber numberWithUnsignedInt:0],     kUIButtonBarButtonStyle,
                [NSNumber numberWithUnsignedInt:2],     kUIButtonBarButtonType,
                [UIImage imageNamed:@"PSInterrupt.png"],kUIButtonBarButtonInfo,
                nil],
            [NSDictionary dictionaryWithObjectsAndKeys:
                self,                                   kUIButtonBarButtonTarget,
                @"buttonBarItemTapped:",                kUIButtonBarButtonAction,
                [NSNumber numberWithUnsignedInt:2],     kUIButtonBarButtonTag,
                [NSNumber numberWithUnsignedInt:0],     kUIButtonBarButtonStyle,
                [NSNumber numberWithUnsignedInt:2],     kUIButtonBarButtonType,
                [UIImage imageNamed:@"PSReset.png"],    kUIButtonBarButtonInfo,
                nil],
            nil];
        toolbar = [[UIToolbar alloc] initInView:self withFrame:toolbarRect withItemList:buttonBarButtons];
        [toolbar setBarStyle: 2];
        int buttonTags[] = {1, 2}; 
        [toolbar registerButtonGroup: 0 withButtons: buttonTags withCount: 2];
        [toolbar showButtonGroup: 0 withDuration: 0.0];
    }
    return self;
}

- (void)dealloc {
    [navBar release];
    [layouts release];
    [layoutIDs release];
    [super dealloc];
}

- (void)hide {
    [UIView beginAnimations:nil context:nil];
    [UIView setAnimationDuration:SettingsViewAnimationDuration];
    self.frame = SettingsViewFrameHidden;
    [UIView endAnimations];
}

- (void)show {
    [UIView beginAnimations:nil context:nil];
    [UIView setAnimationDuration:SettingsViewAnimationDuration];
    self.frame = SettingsViewFrameVisible;
    [UIView endAnimations];
    [table reloadData];
}

- (void)notifyPrefsUpdate {
    NSNotificationCenter * nc = [NSNotificationCenter defaultCenter];
    [nc postNotificationName:@"preferencesUpdated" object:self];
}

#if 0
#pragma mark -
#pragma mark Navigation Bar & Toolbar
#endif

- (void)navigationBar:(UINavigationBar *)navbar buttonClicked:(int)button {
    // only one button exists
    [self hide];
}

- (void)buttonBarItemTapped:(id)sender {
    switch([sender tag]) {
        case 1: // Interrupt
            WantMacInterrupt = YES;
            break;
        case 2: // Reset
            WantMacReset = YES;
            break;
    }
    [self hide];
}

#if 0
#pragma mark -
#pragma mark Table Delegate
#endif

- (int)numberOfGroupsInPreferencesTable:(UIPreferencesTable*)aTable {
    return settingsGroupCount;
}

- (int)preferencesTable:(UIPreferencesTable*)aTable numberOfRowsInGroup:(int)group {
    if (group == settingsGroupKeyboard)
        return [layouts count] + 1;
    else if (group == settingsGroupSound)
        return 1;
    else if (group == settingsGroupVersion)
        return 1;
}

- (UIPreferencesTableCell*)preferencesTable:(UIPreferencesTable*)aTable cellForGroup:(int)group {
    UIPreferencesTableCell * cell = [[[UIPreferencesTableCell alloc] init] autorelease];
    
    if (group == settingsGroupKeyboard)
        [cell setTitle:@"Keyboard"];
    else if (group == settingsGroupSound)
        [cell setTitle:@"Sound"];
    else
        cell = nil;
    
    return cell;
}

- (UIPreferencesTableCell*)preferencesTable:(UIPreferencesTable*)aTable cellForRow:(int)row inGroup:(int)group {
    id cell;
    
    if (group == settingsGroupKeyboard) {
        if (row < [layoutIDs count]) {
            // keyboard layout
            cell = [[UIPreferencesTableCell alloc] init];
            [cell setTitle:[layouts objectForKey:[layoutIDs objectAtIndex:row]]];
            [cell setChecked:[[layoutIDs objectAtIndex:row] isEqualToString:[defaults objectForKey:@"KeyboardLayout"]]];
        } else {
            // keyboard alpha
            cell = [[UIPreferencesControlTableCell alloc] init];
            [cell setTitle:@"Opacity"];
            [cell setShowSelection:NO];
            UISliderControl * sc = [[UISliderControl alloc] initWithFrame: CGRectMake(86.0f, 4.0f, 140.0f, 40.0f)];
            [sc addTarget:self action:@selector(keyboardAlphaChanged:) forEvents:4096];
            [sc setAllowsTickMarkValuesOnly:NO];
            [sc setMinValue:0.2];
            [sc setMaxValue:1.0];
            [sc setValue: [defaults floatForKey:@"KeyboardAlpha"]];
            [sc setShowValue:YES];
            [sc setContinuous:YES];
            [cell setControl:[sc autorelease]];
        }
    } else if (group == settingsGroupSound) {
        cell = [[UIPreferencesControlTableCell alloc] init];
        [cell setTitle:@"Enable"];
        [cell setShowSelection: NO];
        UISwitchControl * sc = [[UISwitchControl alloc] init];
        [sc addTarget:self action:@selector(soundEnabledChanged:) forEvents:4096];
        [sc setOrigin:CGPointMake(127, 10)];
        [sc setValue: [defaults boolForKey:@"SoundEnabled"]?1.0f:0.0f];
        [cell setControl:[sc autorelease]];
    } else if (group == settingsGroupVersion) {
        cell = [[UIPreferencesTableCell alloc] init];
        NSString* bundleVersion = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleVersion"];
        [cell setTitle:[NSString stringWithFormat:@"Mini vMac for iPhone %@\n©2008 namedfork.net", bundleVersion]];
    }
    
    return [cell autorelease];
}

- (float)preferencesTable:(UIPreferencesTable*)aTable heightForRow:(int)row inGroup:(int)group withProposedHeight:(float)proposed {
    return 48.0;
}

- (BOOL)preferencesTable:(UIPreferencesTable*)aTable isLabelGroup:(int)group {
    switch(group) {
        case settingsGroupVersion: return YES;
        default: return NO;
    }
}

- (BOOL)table:(UIPreferencesTable*)aTable canSelectRow:(int)row {
    if ([aTable groupForTableRow:row] == settingsGroupVersion) return NO;
    return YES;
}

- (void)tableRowSelected:(NSNotification*)notification {
    int group, row;
    [table getGroup:&group row:&row forTableRow:[table selectedRow]];
    
    if (group == settingsGroupKeyboard && (row < [layoutIDs count])) {
        [defaults setObject:[layoutIDs objectAtIndex:row] forKey:@"KeyboardLayout"];
        [defaults synchronize];
        [self notifyPrefsUpdate];
        [table reloadData];
    }
}

- (void)keyboardAlphaChanged:(UISliderControl*)slider {
    [defaults setFloat:[slider value] forKey:@"KeyboardAlpha"];
    [defaults synchronize];
    [self notifyPrefsUpdate];
}

- (void)soundEnabledChanged:(UIPreferencesControlTableCell*)cell {
    UISwitchControl* control = [cell control];
    [defaults setBool:([control value] == 1.0) forKey:@"SoundEnabled"];
    [defaults synchronize];
    [self notifyPrefsUpdate];
}
@end
