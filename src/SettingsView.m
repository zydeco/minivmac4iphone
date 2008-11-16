#import "SettingsView.h"
#import "vMacApp.h"

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
        CGRect tableRect = CGRectMake(0.0, 48.0, frame.size.width, frame.size.height-48.0);
        table = [[UIPreferencesTable alloc] initWithFrame: tableRect];
        [table setDelegate: self];
        [table setDataSource: self];
        [self addSubview: table];
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
#pragma mark Navigation Bar Delegate
#endif

- (void)navigationBar:(UINavigationBar *)navbar buttonClicked:(int)button {
    // only one button exists
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
        return [layouts count];
}

- (UIPreferencesTableCell*)preferencesTable:(UIPreferencesTable*)aTable cellForGroup:(int)group {
    UIPreferencesTableCell * cell = [[UIPreferencesTableCell alloc] init];
    
    if (group == settingsGroupKeyboard)
        [cell setTitle:@"Keyboard Layout"];
    
    return [cell autorelease];
}

- (UIPreferencesTableCell*)preferencesTable:(UIPreferencesTable*)aTable cellForRow:(int)row inGroup:(int)group {
    UIPreferencesTableCell * cell;
    
    if (group == settingsGroupKeyboard) {
        cell = [[UIPreferencesTableCell alloc] init];
        [cell setTitle:[layouts objectForKey:[layoutIDs objectAtIndex:row]]];
        [cell setChecked:[[layoutIDs objectAtIndex:row] isEqualToString:[defaults objectForKey:@"KeyboardLayout"]]];
    }
    
    return [cell autorelease];
}

- (float)preferencesTable:(UIPreferencesTable*)aTable heightForRow:(int)row inGroup:(int)group withProposedHeight:(float)proposed {
    return 48.0;
}

- (void)tableRowSelected:(NSNotification*)notification {
    int group, row;
    [table getGroup:&group row:&row forTableRow:[table selectedRow]];
    
    if (group == settingsGroupKeyboard) {
        [defaults setObject:[layoutIDs objectAtIndex:row] forKey:@"KeyboardLayout"];
        [defaults synchronize];
        [self notifyPrefsUpdate];
        [table reloadData];
    }
}

@end
