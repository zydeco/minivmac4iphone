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
        table = [[UITableView alloc] initWithFrame:tableRect style:0];
        [table setDelegate: self];
        [table setDataSource: self];
        [self addSubview: table];
        
        // create nav bar
        navBar = [[UINavigationBar alloc] initWithFrame: CGRectMake(0.0, 0.0, rect.size.width, 48.0)];
        UINavigationItem *navItem = [[UINavigationItem alloc] initWithTitle:NSLocalizedString(@"InsertDisk", nil)];
        UIBarButtonItem *button;
        if ([vMacApp sharedInstance].canCreateDiskImages) {
            button = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:4 target:self action:@selector(newDiskImage)]; // XXX:UIBarButtonSystemItemAdd
            [navItem setLeftBarButtonItem:button animated:NO];
            [button release];
        }
        
        // cancel button
        button = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:14 target:self action:@selector(hide)]; // XXX: UIBarButtonSystemItemStop
        [navItem setRightBarButtonItem:button animated:NO];
        [button release];
        [navBar pushNavigationItem:navItem animated:NO];
        [navItem release];
        [self addSubview: navBar];
        
        // notification
        NSNotificationCenter * nc = [NSNotificationCenter defaultCenter];
        [nc addObserver:self selector:@selector(didInsertDisk:) name:@"diskInserted" object:nil];
        [nc addObserver:self selector:@selector(didEjectDisk:) name:@"diskEjected" object:nil];
        [nc addObserver:self selector:@selector(didCreateDisk:) name:@"diskCreated" object:nil];
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
    [newDisk release];
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
    [table selectRowAtIndexPath:[NSIndexPath indexPathForRow:-1 inSection:0] animated:NO scrollPosition:1]; // XXX: UITableViewScrollPositionTop
    [UIView beginAnimations:nil context:nil];
    [UIView setAnimationDuration:InsertDiskViewAnimationDuration];
    self.frame = InsertDiskViewFrameVisible;
    [UIView endAnimations];
    [self findDiskFiles];
    [table reloadData];
    [[vMacApp sharedInstance] performSelector:@selector(createDiskIcons:) withObject:nil afterDelay:5.0];
}

- (void)didCreateDisk:(NSNotification *)aNotification {
    BOOL success = [[aNotification object] boolValue];
    if (success) {
        [self findDiskFiles];
        [table reloadData];
        [newDisk hide];
    }
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

- (void)newDiskImage {
    if (newDisk == nil) {
        newDisk = [[NewDiskView alloc] initWithFrame:NewDiskViewFrameHidden];
        [self.superview addSubview:newDisk];
    }
    [newDisk show];
    [self.superview bringSubviewToFront:newDisk];
}

#if 0
#pragma mark -
#pragma mark Table Data Source & Delegate
#endif

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    return [diskFiles count];
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    NSString *cellIdentifier = @"diskCell";
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:cellIdentifier];
    NSFileManager *fm = [NSFileManager defaultManager];
    
    if (cell == nil) {
        cell = [[UITableViewCell alloc] initWithFrame:CGRectZero reuseIdentifier:cellIdentifier];
        [cell autorelease];
    }
    
    // get path
    NSString* diskPath = [diskFiles objectAtIndex:indexPath.row];
    
    // set icon
    cell.image = [self iconForDiskImageAtPath:diskPath];
    
    // set title
    cell.text = [[diskPath lastPathComponent] stringByDeletingPathExtension];
    
    // can't cells be disabled?
    if ([diskDrive diskIsInserted:[diskFiles objectAtIndex:indexPath.row]])
        cell.textColor = [UIColor grayColor];
    else
        cell.textColor = [UIColor blackColor];
    
    return cell;
}

- (UITableViewCellEditingStyle)tableView:(UITableView *)tableView editingStyleForRowAtIndexPath:(NSIndexPath *)indexPath {
    NSString * diskPath = [diskFiles objectAtIndex:indexPath.row];
    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"CanDeleteDiskImages"] == NO) return UITableViewCellEditingStyleNone;
    if ([diskDrive diskIsInserted:diskPath]) return UITableViewCellEditingStyleNone;
    if ([[NSFileManager defaultManager] isDeletableFileAtPath:diskPath]) return UITableViewCellEditingStyleDelete;
    return UITableViewCellEditingStyleNone;
}

- (void)tableView:(UITableView *)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath *)indexPath {
    if (editingStyle == UITableViewCellEditingStyleDelete) {
        NSFileManager * fm = [NSFileManager defaultManager];
        // delete file
        NSString * diskPath = [diskFiles objectAtIndex:indexPath.row];
        if ([fm removeItemAtPath:diskPath error:NULL]) {
            [self findDiskFiles];
            [table deleteRowsAtIndexPaths:[NSArray arrayWithObject:indexPath] withRowAnimation:UITableViewRowAnimationFade];
        }
    }
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
    @try {
        id diskFile = [diskFiles objectAtIndex:indexPath.row];
        if ([diskDrive diskIsInserted:diskFile]) return;
        [diskDrive insertDisk:diskFile];
        [self hide];
    }
    @catch (NSException* e) {}
}

- (NSIndexPath *)tableView:(UITableView *)tableView willSelectRowAtIndexPath:(NSIndexPath *)indexPath {
    @try {
        id diskFile = [diskFiles objectAtIndex:indexPath.row];
        if ([diskDrive diskIsInserted:diskFile]) return nil;
        return indexPath;
    } @catch (NSException* e) {}
}
@end