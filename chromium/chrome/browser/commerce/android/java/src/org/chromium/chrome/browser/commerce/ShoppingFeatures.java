// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;

//Vivaldi
import org.chromium.build.BuildConfig;
import org.chromium.components.commerce.core.ShoppingService;

/** Self-documenting feature class for shopping. */
public class ShoppingFeatures {
    private static Boolean sShoppingListEligibleForTestsing;

    /** Wrapper function for ShoppingService.isShoppingListEligibile(). */
    public static boolean isShoppingListEligible() {
        if (BuildConfig.IS_VIVALDI) return false;
        if (sShoppingListEligibleForTestsing != null) return sShoppingListEligibleForTestsing;
        if (!ProfileManager.isInitialized()) return false;

        Profile profile = Profile.getLastUsedRegularProfile();
        if (profile == null) return false;
        ShoppingService service = ShoppingServiceFactory.getForProfile(profile);
        if (service == null) return false;
        return service.isShoppingListEligible();
    }

    public static void setShoppingListEligibleForTesting(Boolean eligible) {
        sShoppingListEligibleForTestsing = eligible;
        ResettersForTesting.register(() -> sShoppingListEligibleForTestsing = null);
    }
}
