#import "InsertDiskView.h"
#import <UIKit/UISimpleTableCell.h>

@implementation InsertDiskView
@synthesize diskDrive;

- (id)initWithFrame:(CGRect)rect {
    if ((self = [super initWithFrame: rect]) != nil) {
        diskFiles = [[NSArray array] retain];
        
        // create table
        CGRect tableRect = CGRectMake(0.0, 48.0, rect.size.width, rect.size.height-48.0);
        table = [[UITable alloc] initWithFrame: tableRect];
        UITableColumn* col = [[UITableColumn alloc] initWithTitle:@"Title" identifier:@"title" width:tableRect.size.width];
        [table addTableColumn: col];
        [table setSeparatorStyle: 1];
        [table setRowHeight: 48.0];
        [table setDelegate: self];
        [table setDataSource: self];
        [self addSubview: table];
        
        // create nav bar
        navBar = [[UINavigationBar alloc] initWithFrame: CGRectMake(0.0, 0.0, rect.size.width, 48.0)];
        [navBar setDelegate:self];
        UINavigationItem *navItem = [[UINavigationItem alloc] initWithTitle:NSLocalizedString(@"InsertDisk", nil)];
        [navBar pushNavigationItem: navItem];
        [navBar showButtonsWithLeftTitle:nil rightTitle: NSLocalizedString(@"Cancel", nil) leftBack: NO];
        [self addSubview: navBar];
        [navItem autorelease];
        
        // notification
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(didInsertDisk:) name:@"diskInserted" object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(didEjectDisk:) name:@"diskEjected" object:nil];
    }
    return self;
}

- (void)dealloc {
    [table release];
    [navBar release];
    [diskFiles release];
    [super dealloc];
}

- (void)hide {
    [UIView beginAnimations:nil context:nil];
    [UIView setAnimationDuration:InsertDiskViewAnimationDuration];
    self.frame = InsertDiskViewFrameHidden;
    [UIView endAnimations];
}

- (void)show {
    [table selectRow:-1 byExtendingSelection:NO];
    [UIView beginAnimations:nil context:nil];
    [UIView setAnimationDuration:InsertDiskViewAnimationDuration];
    self.frame = InsertDiskViewFrameVisible;
    [UIView endAnimations];
    [self findDiskFiles];
    [table reloadData];
}

- (void)didEjectDisk:(NSNotification *)aNotification {
    [table reloadData];
}

- (void)didInsertDisk:(NSNotification *)aNotification {
    [table reloadData];
}

- (void)findDiskFiles {
    NSFileManager* fm = [NSFileManager defaultManager];
    NSArray* sources = [[vMacApp sharedInstance] searchPaths];
    NSArray* extensions = [NSArray arrayWithObjects: @"dsk", @"img", @"DSK", @"IMG", nil];
    
    NSMutableArray* myDiskFiles = [NSMutableArray arrayWithCapacity:10];
    for(NSString *srcDir in sources) {
        NSArray *dirFiles = [[fm contentsOfDirectoryAtPath:srcDir error:NULL] pathsMatchingExtensions:extensions];
        for(NSString *filename in dirFiles)
            [myDiskFiles addObject:[srcDir stringByAppendingPathComponent: filename]];
    }
    
    [diskFiles release];
    diskFiles = [[NSArray arrayWithArray:myDiskFiles] retain];
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

- (int)numberOfRowsInTable: (UITable *)table {
    return [diskFiles count];
}

- (UITableCell *)table:(UITable *)table cellForRow:(int)row column:(UITableColumn *)col {
    UISimpleTableCell *cell = [[UISimpleTableCell alloc] init];
    
    // get path
    NSString* diskPath = [diskFiles objectAtIndex:row];
    
    // get size for icon
    NSDictionary* fileAttrs = [[NSFileManager defaultManager] attributesOfItemAtPath:diskPath error:NULL];
    NSNumber* fileSize = [fileAttrs valueForKey:NSFileSize];
    if ([fileSize longLongValue] < 1440*1024+100)
        [cell setIcon:[UIImage imageNamed:@"DiskListFloppy.png"]];
    else [cell setIcon:[UIImage imageNamed:@"DiskListHD.png"]];
    
    // set title
    NSString *diskTitle = [[diskPath lastPathComponent] stringByDeletingPathExtension];
    [cell setTitle:diskTitle];
    
    // enable?
    if ([diskDrive diskIsInserted:[diskFiles objectAtIndex:row]])
        [cell setEnabled:NO];
    
    return [cell autorelease];
}

- (void)tableRowSelected:(NSNotification*)notification {
    @try {
        [diskDrive insertDisk:[diskFiles objectAtIndex:[table selectedRow]]];
        [self hide];
    }
    @catch (NSException* e) {}
}

@end