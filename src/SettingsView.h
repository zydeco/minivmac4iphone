#import <UIKit/UIKit.h>
#import <UIKit/UIPreferencesTable.h>
#import <UIKit/UIPreferencesTableCell.h>

#define SettingsViewAnimationDuration     0.3
#define SettingsViewFrameHidden           CGRectMake(-240.0, 0.0, 240.0, 320.0)
#define SettingsViewFrameVisible          CGRectMake(0.0, 0.0, 240.0, 320.0)

typedef enum {
    settingsGroupKeyboard,
    settingsGroupSound,
    settingsGroupVersion,
    
    settingsGroupCount
} SettingsTableGroup;

@interface SettingsView : UIView {
    UINavigationBar *       navBar;
    UIPreferencesTable *    table;
    UIToolbar *             toolbar;
    NSUserDefaults *        defaults;
    
    NSDictionary *          layouts;
    NSArray *               layoutIDs;
}

- (void)hide;
- (void)show;
- (void)notifyPrefsUpdate;
@end