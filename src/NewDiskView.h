#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <UIKit/UISliderControl.h>

extern float NewDiskViewAnimationDuration;
extern CGRect NewDiskViewFrameHidden;
extern CGRect NewDiskViewFrameVisible;

@interface NewDiskView : UIView
{
    UINavigationBar*        navBar;
    UITextLabel*            labels[2];
    UITextLabel*            sizeLabel;
    UITextField*            nameField;
    UISliderControl*        sizeSlider;
    UIImageView*            iconView;
}

- (void)hide;
- (void)show;
- (void)sizeSliderChanged:(UISliderControl*)slider;
- (int)selectedImageSize;
@end

