#import <GraphicsServices/GraphicsServices.h>
#import "DiskListTable.h"

@implementation DiskListTable

- (BOOL)canHandleSwipes {
    _swipeToDeleteRow = YES;
    return YES;
}

- (int)swipe:(int)type withEvent:(GSEventRef)event {
    CGPoint point = [self convertPoint:GSEventGetLocationInWindow(event).origin fromView:nil];
    int row = [self rowAtPoint:point];
    
    // hide remove button, or it won't show the >=2nd time
    [[self visibleCellForRow:row column:0]
       _showDeleteOrInsertion: NO
       withDisclosure: NO
       animated: YES
       isDelete: YES
       andRemoveConfirmation: YES
    ];
    
    // show remove button if it's removable
    if ([self canDeleteRow:row]) {
        [[self visibleCellForRow:row column:0]
           _showDeleteOrInsertion: YES
           withDisclosure: NO
           animated: YES
           isDelete: YES
           andRemoveConfirmation: YES
        ];
    }
    return [super swipe:type withEvent:event];
}

@end
