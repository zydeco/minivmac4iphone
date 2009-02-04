#import "InsertDiskView.h"
#import <UIKit/UISimpleTableCell.h>
#import <Foundation/NSTask.h>
#import "ExtendedAttributes.h"

@implementation InsertDiskView
@synthesize diskDrive;

- (id)initWithFrame:(CGRect)rect {
    if ((self = [super initWithFrame: rect]) != nil) {
        diskFiles = [[NSArray array] retain];
        
        // create table
        CGRect tableRect = CGRectMake(0.0, 48.0, rect.size.width, rect.size.height-48.0);
        table = [[DiskListTable alloc] initWithFrame: tableRect];
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
        [navBar showButtonsWithLeftTitle:([vMacApp sharedInstance].canCreateDiskImages?NSLocalizedString(@"NewDiskImage",nil):nil) rightTitle: NSLocalizedString(@"Cancel", nil) leftBack: NO];
        [self addSubview: navBar];
        [navItem autorelease];
        
        // notification
        NSNotificationCenter * nc = [NSNotificationCenter defaultCenter];
        [nc addObserver:self selector:@selector(didInsertDisk:) name:@"diskInserted" object:nil];
        [nc addObserver:self selector:@selector(didEjectDisk:) name:@"diskEjected" object:nil];
        [nc addObserver:table selector:@selector(reloadData) name:@"diskIconUpdate" object:nil];
    }
    return self;
}

- (void)dealloc {
    NSNotificationCenter * nc = [NSNotificationCenter defaultCenter];
    [nc removeObserver:self];
    [nc removeObserver:table];
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
    [NSObject cancelPreviousPerformRequestsWithTarget:[vMacApp sharedInstance] selector:@selector(createDiskIcons:) object:nil];
}

- (void)show {
    [table selectRow:-1 byExtendingSelection:NO];
    [UIView beginAnimations:nil context:nil];
    [UIView setAnimationDuration:InsertDiskViewAnimationDuration];
    self.frame = InsertDiskViewFrameVisible;
    [UIView endAnimations];
    [self findDiskFiles];
    [table reloadData];
    [[vMacApp sharedInstance] performSelector:@selector(createDiskIcons:) withObject:nil afterDelay:2.0];
}

- (void)didEjectDisk:(NSNotification *)aNotification {
    [table reloadData];
}

- (void)didInsertDisk:(NSNotification *)aNotification {
    [table reloadData];
}

- (void)findDiskFiles {
    [diskFiles release];
    diskFiles = [[[vMacApp sharedInstance] availableDiskImages] retain];
}

#if 0
#pragma mark -
#pragma mark Disk Icons
#endif

- (UIImage*)iconForDiskImageAtPath:(NSString *)path {
    NSFileManager* fm = [NSFileManager defaultManager];
    UIImage* iconImage = nil;
    
    // get icon from xattr
    iconImage = [UIImage imageWithData:[fm extendedAttribute:@"net.namedfork.DiskImageIcon" atPath:path traverseLink:YES error:NULL]];
    
    // get icon from file
    if (iconImage == nil) {
        NSString *iconPath = [[path stringByDeletingPathExtension] stringByAppendingPathExtension:@"png"];
        if ([fm fileExistsAtPath:iconPath])
            iconImage = [UIImage imageAtPath:iconPath];
    }
    
    // set default icon
    if (iconImage == nil) {
        NSDictionary* fileAttrs = [fm attributesOfItemAtPath:path error:NULL];
        NSNumber* fileSize = [fileAttrs valueForKey:NSFileSize];
        if ([fileSize longLongValue] < 1440*1024+100)
            iconImage = [UIImage imageNamed:@"DiskListFloppy.png"];
        else 
            iconImage = [UIImage imageNamed:@"DiskListHD.png"];
    }
    
    return iconImage;
}

#if 0
#pragma mark -
#pragma mark Navigation Bar Delegate
#endif

- (void)navigationBar:(UINavigationBar *)navbar buttonClicked:(int)button {
    if (button == 1) {
        // new disk image
    } else if (button == 0) {
        // close
        [self hide];
    }
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
    NSFileManager *fm = [NSFileManager defaultManager];
    
    // get path
    NSString* diskPath = [diskFiles objectAtIndex:row];
    
    // set icon
    [cell setIcon:[self iconForDiskImageAtPath:diskPath]];
    
    // set title
    NSString *diskTitle = [[diskPath lastPathComponent] stringByDeletingPathExtension];
    [cell setTitle:diskTitle];
    
    // enable?
    if ([diskDrive diskIsInserted:[diskFiles objectAtIndex:row]])
        [cell setEnabled:NO];
    
    return [cell autorelease];
}

- (BOOL)table:(UITable *)aTable canDeleteRow:(int)row {
    NSString * diskPath = [diskFiles objectAtIndex:row];
    return [[NSFileManager defaultManager] isDeletableFileAtPath:diskPath];
}

- (void)table:(UITable *)aTable deleteRow:(int)row {
    NSFileManager * fm = [NSFileManager defaultManager];
    // delete file
    NSString * diskPath = [diskFiles objectAtIndex:row];
    if ([fm removeItemAtPath:diskPath error:NULL]) {
        // delete icon file
        NSString * iconPath = [[diskPath stringByDeletingPathExtension] stringByAppendingPathExtension:@"png"];
        [fm removeItemAtPath:iconPath error:NULL];
    }
    
    [self findDiskFiles];
}

- (void)tableRowSelected:(NSNotification*)notification {
    @try {
        [diskDrive insertDisk:[diskFiles objectAtIndex:[table selectedRow]]];
        [self hide];
    }
    @catch (NSException* e) {}
}

@end