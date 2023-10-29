// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_TOOLBAR_VIVALDI_STICKY_TOOLBAR_VIEW_H_
#define IOS_UI_TOOLBAR_VIVALDI_STICKY_TOOLBAR_VIEW_H_

#import <UIKit/UIKit.h>

// A view to show the location label and image in fullscreen mode.
@interface VivaldiStickyToolbarView : UIView

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// SETTERS
- (void)setText:(NSString*)text
       clipTail:(BOOL)clipTail;
- (void)setLocationImage:(UIImage*)locationImage;
- (void)setTintColor:(BOOL)isPrivateMode;
- (void)setSecurityLevelAccessibilityString:(NSString*)string;

@end

#endif  // IOS_UI_TOOLBAR_VIVALDI_STICKY_TOOLBAR_VIEW_H_
