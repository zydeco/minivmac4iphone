#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <UIKit/UISectionList.h>
#import "vMacApp.h"

#define InsertDiskViewAnimationDuration     0.3
#define InsertDiskViewFrameHidden           CGRectMake(480.0, 0.0, 240.0, 320.0)
#define InsertDiskViewFrameVisible          CGRectMake(240.0, 0.0, 240.0, 320.0)

@interface InsertDiskView : UIView {
    id <VirtualDiskDrive>   diskDrive;
    NSArray*                diskFiles;
    
    UITable*                table;
    UINavigationBar*        navBar;
}

@property (nonatomic, assign) id <VirtualDiskDrive> diskDrive;

- (void)hide;
- (void)show;
- (void)findDiskFiles;
- (void)didEjectDisk:(NSNotification *)aNotification;
- (void)didInsertDisk:(NSNotification *)aNotification;

@end