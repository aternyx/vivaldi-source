// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/sync/vivaldi_sync_settings_view_controller.h"

#import "base/apple/foundation_util.h"
#import "components/sync/base/user_selectable_type.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/ui/settings/utils/settings_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/settings/sync/cells/vivaldi_table_view_sync_status_item.h"
#import "ios/ui/settings/sync/vivaldi_sync_settings_constants.h"
#import "ios/ui/table_view/cells/vivaldi_table_view_segmented_control_item.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation VivaldiSyncSettingsViewController

- (instancetype)initWithStyle:(UITableViewStyle)style {
  if ((self = [super initWithStyle:style])) {

  }
  return self;
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.delegate vivaldiSyncSettingsViewControllerWasRemoved:self];
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadModel];
  self.title = l10n_util::GetNSString(IDS_PREFS_VIVALDI_SYNC);
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];

  [self.modelDelegate vivaldiSyncSettingsViewControllerLoadModel:self];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
      willDisplayCell:(UITableViewCell*)cell
    forRowAtIndexPath:(NSIndexPath*)indexPath {
  if ([cell isKindOfClass:[VivaldiTableViewSyncStatusCell class]]) {
    VivaldiTableViewSyncStatusCell* editCell =
        base::apple::ObjCCastStrict<VivaldiTableViewSyncStatusCell>(cell);
    [editCell updateLabelCornerRadius];
  }
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);

  switch (itemType) {
    case ItemTypeHeaderItem:
    case ItemTypeSyncUserInfo:
    case ItemTypeSyncStatus:
    case ItemTypeSyncAllInfoTextbox: {
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
    }
    case ItemTypeSyncWhatSegmentedControl: {
      VivaldiTableViewSegmentedControlCell* segmentedControlCell =
          base::apple::ObjCCastStrict<VivaldiTableViewSegmentedControlCell>(cell);
      [segmentedControlCell.segmentedControl addTarget:self
                    action:@selector(syncAllOptionChanged:)
          forControlEvents:UIControlEventValueChanged];
      // This is the top cell and we want to hide the separator line
      segmentedControlCell.separatorInset =
          UIEdgeInsetsMake(0,0,0,tableView.frame.size.width);
      break;
    }
    case ItemTypeSyncBookmarksSwitch: {
      TableViewSwitchCell* switchCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                              action:@selector(bookmarksSwitchToggled:)
                    forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeSyncSettingsSwitch: {
      TableViewSwitchCell* switchCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                              action:@selector(settingsSwitchToggled:)
                    forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeSyncPasswordsSwitch: {
      TableViewSwitchCell* switchCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                              action:@selector(passwordsSwitchToggled:)
                    forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeSyncAutofillSwitch: {
      TableViewSwitchCell* switchCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                              action:@selector(autofillSwitchToggled:)
                    forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeSyncTabsSwitch: {
      TableViewSwitchCell* switchCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                              action:@selector(tabsSwitchToggled:)
                    forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeSyncHistorySwitch: {
      TableViewSwitchCell* switchCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                              action:@selector(historySwitchToggled:)
                    forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeSyncReadingListSwitch: {
      TableViewSwitchCell* switchCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                              action:@selector(readingListSwitchToggled:)
                    forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeSyncNotesSwitch: {
      TableViewSwitchCell* switchCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                              action:@selector(notesSwitchToggled:)
                    forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeStartSyncingButton: {
      TableViewTextButtonCell* tableViewTextButtonCell =
          base::apple::ObjCCastStrict<TableViewTextButtonCell>(cell);
      [self clearExistingTarget:tableViewTextButtonCell.button];
      [tableViewTextButtonCell.button
                 addTarget:self
                    action:@selector(startSyncingAllButtonPressed:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    case ItemTypeEncryptionPasswordButton: {
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      TableViewDetailTextCell* tableViewDetailTextCell =
          base::apple::ObjCCastStrict<TableViewDetailTextCell>(cell);
      UITapGestureRecognizer* tap = [[UITapGestureRecognizer alloc]
          initWithTarget:self
          action:@selector(encryptionInfoButtonPressed)];
      [tableViewDetailTextCell.accessoryView addGestureRecognizer:tap];
      [tableViewDetailTextCell.accessoryView setUserInteractionEnabled:YES];
      tableViewDetailTextCell.accessoryView.tintColor =
          [UIColor colorNamed:kBlueColor];
      break;
    }
    case ItemTypeBackupRecoveryKeyButton: {
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      TableViewDetailTextCell* tableViewDetailTextCell =
          base::apple::ObjCCastStrict<TableViewDetailTextCell>(cell);
      UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
      label.text = l10n_util::GetNSString(IDS_VIVALDI_SAVE);
      label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
      label.numberOfLines = 1;
      [label sizeToFit];
      label.textColor = [UIColor colorNamed:kBlueColor];
      tableViewDetailTextCell.accessoryView = label;
      tableViewDetailTextCell.accessoryView.tintColor =
          [UIColor colorNamed:kBlueColor];
      UITapGestureRecognizer* tap = [[UITapGestureRecognizer alloc]
          initWithTarget:self
          action:@selector(backupEncryptionKeyButtonPressed)];
      [tableViewDetailTextCell.accessoryView addGestureRecognizer:tap];
      [tableViewDetailTextCell.accessoryView setUserInteractionEnabled:YES];
      break;
    }
    case ItemTypeLogOutButton: {
      TableViewTextButtonCell* tableViewTextButtonCell =
          base::apple::ObjCCastStrict<TableViewTextButtonCell>(cell);
      [self clearExistingTarget:tableViewTextButtonCell.button];
      [tableViewTextButtonCell.button
                 addTarget:self
                    action:@selector(logOutButtonPressed:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    case ItemTypeDeleteDataButton: {
      TableViewTextButtonCell* tableViewTextButtonCell =
          base::apple::ObjCCastStrict<TableViewTextButtonCell>(cell);
      [self clearExistingTarget:tableViewTextButtonCell.button];
      [tableViewTextButtonCell.button
                 addTarget:self
                    action:@selector(deleteDataButtonPressed:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
  }

  return cell;
}

#pragma mark - VivaldiSyncSettingsConsumer

- (void)reloadItem:(TableViewItem*)item {
  if (!self.tableViewModel) {
    // No need to reload since the model has not been loaded yet.
    return;
  }
  NSIndexPath* indexPath = [self.tableViewModel indexPathForItem:item];
  [self.tableView reloadRowsAtIndexPaths:@[ indexPath ]
                        withRowAnimation:UITableViewRowAnimationNone];
}

- (void)reloadSection:(NSInteger)sectionIdentifier {
  if (!self.tableViewModel) {
    // No need to reload since the model has not been loaded yet.
    return;
  }
  if (![self.tableViewModel
          hasSectionForSectionIdentifier:sectionIdentifier]) {
    // No need to reload if the section is not there
    return;
  }
  NSUInteger index = [self.tableViewModel
      sectionForSectionIdentifier:sectionIdentifier];
  [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:index]
                withRowAnimation:UITableViewRowAnimationNone];
}

#pragma mark - Private Methods

- (void)backupEncryptionKeyButtonPressed {
  NSString* filePath = [self.serviceDelegate createTempBackupEncryptionKeyFile];

  // Get the URL of your text file
  NSURL* fileURL = [NSURL fileURLWithPath:filePath];

  // Create an array containing the items to share
  NSArray* itemsToShare = @[fileURL];

  UIActivityViewController* activityVC = [[UIActivityViewController alloc]
      initWithActivityItems:itemsToShare applicationActivities:nil];

  NSArray* excludedActivityTypes = @[
    UIActivityTypeAddToReadingList,
    UIActivityTypeCopyToPasteboard, UIActivityTypeOpenInIBooks,
    UIActivityTypePostToFacebook, UIActivityTypePostToFlickr,
    UIActivityTypePostToTencentWeibo, UIActivityTypePostToTwitter,
    UIActivityTypePostToVimeo, UIActivityTypePostToWeibo, UIActivityTypePrint
  ];
  activityVC.excludedActivityTypes = excludedActivityTypes;

  __weak __typeof(self) weakSelf = self;
  activityVC.completionWithItemsHandler =
      ^(NSString *activityType, BOOL completed,
        NSArray *returnedItems, NSError *activityError) {
    [weakSelf.serviceDelegate removeTempBackupEncryptionKeyFile:filePath];
  };

  [self presentViewController:activityVC animated:YES completion:nil];
}

- (void)encryptionInfoButtonPressed {
  BlockToOpenURL(self, self.applicationCommandsHandler)(GURL(
      l10n_util::GetStringUTF8(IDS_VIVALDI_IOS_ENCRYPTION_INFO_URL)));
}

- (void)logOutButtonPressed:(UIButton*)sender {
  [self.serviceDelegate logOutButtonPressed];
}

- (void)deleteDataButtonPressed:(UIButton*)sender {
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:l10n_util::GetNSString(
          IDS_VIVALDI_SYNC_CONFIRM_CLEAR_SERVER_DATA_TITLE)
        message:l10n_util::GetNSString(
          IDS_VIVALDI_SYNC_CONFIRM_CLEAR_SERVER_DATA_MESSAGE)
        preferredStyle:UIAlertControllerStyleAlert];
  __weak __typeof(self) weakSelf = self;

  UIAlertAction* deleteAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_VIVALDI_SYNC_CLEAR_SERVER_DATA)
                style:UIAlertActionStyleDestructive
              handler:^(UIAlertAction* action) {
                [weakSelf.serviceDelegate clearSyncDataWithNoWarning];
              }];
  [alertController addAction:deleteAction];

  UIAlertAction* cancelAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(
          IDS_VIVALDI_SYNC_CANCEL_CLEAR_SERVER_DATA_MESSAGE)
                style:UIAlertActionStyleCancel
              handler:^(UIAlertAction* action){
              }];
  [alertController addAction:cancelAction];
  [self presentViewController:alertController animated:YES completion:nil];
}

- (void)startSyncingAllButtonPressed:(UIButton*)sender {
  [self.serviceDelegate startSyncingAllButtonPressed];
}

- (void)syncAllOptionChanged:(UISegmentedControl*)sender {
  bool syncAllData = sender.selectedSegmentIndex == SyncAll ? YES : NO;
  [self.serviceDelegate syncAllOptionChanged:syncAllData];
}

- (void)bookmarksSwitchToggled:(UISwitch*)sender {
  [self.serviceDelegate updateChosenTypes:
      syncer::UserSelectableType::kBookmarks isOn:sender.isOn];
}

- (void)settingsSwitchToggled:(UISwitch*)sender {
  [self.serviceDelegate updateChosenTypes:
      syncer::UserSelectableType::kPreferences isOn:sender.isOn];
}

- (void)passwordsSwitchToggled:(UISwitch*)sender {
  [self.serviceDelegate updateChosenTypes:
      syncer::UserSelectableType::kPasswords isOn:sender.isOn];
}

- (void)autofillSwitchToggled:(UISwitch*)sender {
  [self.serviceDelegate updateChosenTypes:
      syncer::UserSelectableType::kAutofill isOn:sender.isOn];
}

- (void)tabsSwitchToggled:(UISwitch*)sender {
  [self.serviceDelegate updateChosenTypes:
      syncer::UserSelectableType::kTabs isOn:sender.isOn];
}

- (void)historySwitchToggled:(UISwitch*)sender {
  [self.serviceDelegate updateChosenTypes:
      syncer::UserSelectableType::kHistory isOn:sender.isOn];
}

- (void)readingListSwitchToggled:(UISwitch*)sender {
  [self.serviceDelegate updateChosenTypes:
      syncer::UserSelectableType::kReadingList isOn:sender.isOn];
}

- (void)notesSwitchToggled:(UISwitch*)sender {
  [self.serviceDelegate updateChosenTypes:
      syncer::UserSelectableType::kNotes isOn:sender.isOn];
}

#pragma mark - PRIVATE

- (void)clearExistingTarget:(UIButton*)button {
  [button removeTarget:nil
                action:nil
      forControlEvents:UIControlEventAllEvents];
}

@end
