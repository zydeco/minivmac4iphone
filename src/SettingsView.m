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
        UINavigationItem *navItem = [[UINavigationItem alloc] initWithTitle:NSLocalizedString(@"Settings", nil)];
        [navBar pushNavigationItem: navItem];
        [navBar showButtonsWithLeftTitle: NSLocalizedString(@"Close", nil) rightTitle: nil  leftBack: YES];
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
    switch(group) {
    case settingsGroupKeyboard:
        return [layouts count] + 1;
    case settingsGroupMouse:
        return 1;
    case settingsGroupSound:
        return 3;
    case settingsGroupVersion:
        return 1;
    }
}

- (UIPreferencesTableCell*)preferencesTable:(UIPreferencesTable*)aTable cellForGroup:(int)group {
    UIPreferencesTableCell * cell = [[[UIPreferencesTableCell alloc] init] autorelease];
    
    switch(group) {
        case settingsGroupKeyboard:
            [cell setTitle:NSLocalizedString(@"SettingsKeyboard", nil)];
            break;
        case settingsGroupMouse:
            [cell setTitle:NSLocalizedString(@"SettingsMouse", nil)];
            break;
        case settingsGroupSound:
            [cell setTitle:NSLocalizedString(@"SettingsSound", nil)];
            break;
        default:
            cell = nil;
    }
    return cell;
}

- (UIPreferencesTableCell*)preferencesTable:(UIPreferencesTable*)aTable cellForRow:(int)row inGroup:(int)group {
    UISwitchControl * swc;
    UISliderControl * slc;
    id cell;
    
    if (group == settingsGroupKeyboard) {
        switch (row - [layoutIDs count]) {
        case 0:
            // keyboard alpha
            cell = [[UIPreferencesControlTableCell alloc] init];
            [cell setTitle:NSLocalizedString(@"SettingsKeyboardOpacity", nil)];
            [cell setShowSelection:NO];
            slc = [[UISliderControl alloc] initWithFrame: CGRectMake(96.0f, 4.0f, 130.0f, 40.0f)];
            [slc addTarget:self action:@selector(keyboardAlphaChanged:) forEvents:4096];
            [slc setAllowsTickMarkValuesOnly:NO];
            [slc setMinValue:0.2];
            [slc setMaxValue:1.0];
            [slc setValue: [defaults floatForKey:@"KeyboardAlpha"]];
            [slc setShowValue:NO];
            [slc setContinuous:YES];
            [cell setControl:[slc autorelease]];
            break;
        default:
            // keyboard layout
            cell = [[UIPreferencesTableCell alloc] init];
            [cell setTitle:[layouts objectForKey:[layoutIDs objectAtIndex:row]]];
            [cell setChecked:[[layoutIDs objectAtIndex:row] isEqualToString:[defaults objectForKey:@"KeyboardLayout"]]];
        }
    } else if (group == settingsGroupMouse) {
        cell = [[UIPreferencesControlTableCell alloc] init];
        [cell setTitle:NSLocalizedString(@"SettingsMouseTrackpadMode", nil)];
        [cell setShowSelection: NO];
        swc = [[UISwitchControl alloc] init];
        [swc addTarget:self action:@selector(trackpadModeChanged:) forEvents:4096];
        [swc setOrigin:CGPointMake(127, 10)];
        [swc setValue: [defaults boolForKey:@"TrackpadMode"]?1.0f:0.0f];
        [cell setControl:[swc autorelease]];
    } else if (group == settingsGroupSound) {
        if (row == 0) {
            // mac sound
            cell = [[UIPreferencesControlTableCell alloc] init];
            [cell setTitle:NSLocalizedString(@"SettingsSoundEnable", nil)];
            [cell setShowSelection: NO];
            swc = [[UISwitchControl alloc] init];
            [swc addTarget:self action:@selector(soundEnabledChanged:) forEvents:4096];
            [swc setOrigin:CGPointMake(127, 10)];
            [swc setValue: [defaults boolForKey:@"SoundEnabled"]?1.0f:0.0f];
            [cell setControl:[swc autorelease]];
        } else if (row == 1) {
            // disk eject
            cell = [[UIPreferencesControlTableCell alloc] init];
            [cell setTitle:NSLocalizedString(@"SettingsSoundDiskEject", nil)];
            [cell setShowSelection: NO];
            swc = [[UISwitchControl alloc] init];
            [swc addTarget:self action:@selector(soundDiskEjectChanged:) forEvents:4096];
            [swc setOrigin:CGPointMake(127, 10)];
            [swc setValue: [defaults boolForKey:@"DiskEjectSound"]?1.0f:0.0f];
            [cell setControl:[swc autorelease]];
        } else if (row == 2) {
            // key sound
            cell = [[UIPreferencesControlTableCell alloc] init];
            [cell setTitle:NSLocalizedString(@"SettingsKeyboardSound", nil)];
            [cell setShowSelection: NO];
            swc = [[UISwitchControl alloc] init];
            [swc addTarget:self action:@selector(keyboardSoundChanged:) forEvents:4096];
            [swc setOrigin:CGPointMake(127, 10)];
            [swc setValue: [defaults boolForKey:@"KeyboardSound"]?1.0f:0.0f];
            [cell setControl:[swc autorelease]];
        }
        
    } else if (group == settingsGroupVersion) {
        cell = [[UIPreferencesTableCell alloc] init];
        NSString* bundleVersion = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleVersion"];
        NSString* bundleLongName = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleLongName"];
        [cell setTitle:[NSString stringWithFormat:@"%@ %@\n©2008 namedfork.net", bundleLongName, bundleVersion]];
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

- (void)soundDiskEjectChanged:(UIPreferencesControlTableCell*)cell {
    UISwitchControl* control = [cell control];
    [defaults setBool:([control value] == 1.0) forKey:@"DiskEjectSound"];
    [defaults synchronize];
    [self notifyPrefsUpdate];
}

- (void)trackpadModeChanged:(UIPreferencesControlTableCell*)cell {
    UISwitchControl* control = [cell control];
    [defaults setBool:([control value] == 1.0) forKey:@"TrackpadMode"];
    [defaults synchronize];
    [self notifyPrefsUpdate];
}

- (void)keyboardSoundChanged:(UIPreferencesControlTableCell*)cell {
    UISwitchControl* control = [cell control];
    [defaults setBool:([control value] == 1.0) forKey:@"KeyboardSound"];
    [defaults synchronize];
    [self notifyPrefsUpdate];
}

@end
