// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// A view that displays a list of actions in the overflow menu.
@available(iOS 15, *)
struct OverflowMenuActionList: View {
  /// The list of action groups for this view.
  var actionGroups: [OverflowMenuActionGroup]

  /// The metrics handler to alert when the user takes metrics actions.
  weak var metricsHandler: PopupMenuMetricsHandler?

  /// The namespace for the animation of this view appearing or disappearing.
  let namespace: Namespace.ID

  var body: some View {
    List {
      ForEach(actionGroups.filter({ !$0.actions.isEmpty })) { actionGroup in
        OverflowMenuActionSection(
          actionGroup: actionGroup, metricsHandler: metricsHandler)
      }
    }
    .matchedGeometryEffect(id: MenuCustomizationAnimationID.actions, in: namespace)
    .simultaneousGesture(
      DragGesture().onChanged({ _ in
        metricsHandler?.popupMenuScrolledVertically()
      })
    )
    .accessibilityIdentifier(kPopupMenuToolsMenuActionListId)
    .overflowMenuListStyle()
  }
}

/// Calls `onScroll` when the user performs a drag gesture over the content of the list.
struct ListScrollDetectionModifier: ViewModifier {
  let onScroll: () -> Void
  func body(content: Content) -> some View {
    content
      // For some reason, without this, user interaction is not forwarded to the list.
      .onTapGesture(perform: {})
      // Long press gestures are dismissed and `onPressingChanged` called with
      // `pressing` equal to `false` when the user performs a drag gesture
      // over the content, hence why this works. `DragGesture` cannot be used
      // here, even with a `simultaneousGesture` modifier, because it prevents
      // swipe-to-delete from correctly triggering within the list.
      .onLongPressGesture(
        perform: {},
        onPressingChanged: { pressing in
          if !pressing {
            onScroll()
          }
        })
  }
}
