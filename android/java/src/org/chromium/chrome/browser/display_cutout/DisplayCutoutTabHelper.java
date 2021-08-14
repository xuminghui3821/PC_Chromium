// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.display_cutout;

import android.app.Activity;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.components.browser_ui.display_cutout.DisplayCutoutController;
import org.chromium.components.browser_ui.widget.InsetObserverView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;

/**
 * Wraps a {@link DisplayCutoutController} for a Chrome {@link Tab}.
 *
 * This will only be created once the tab sets a non-default viewport fit.
 */
public class DisplayCutoutTabHelper implements UserData {
    private static final Class<DisplayCutoutTabHelper> USER_DATA_KEY = DisplayCutoutTabHelper.class;

    /** The tab that this object belongs to. */
    private Tab mTab;

    @VisibleForTesting
    DisplayCutoutController mCutoutController;

    /** Listens to various Tab events. */
    private final TabObserver mTabObserver = new EmptyTabObserver() {
        @Override
        public void onShown(Tab tab, @TabSelectionType int type) {
            assert tab == mTab;

            // Force a layout update if we are now being shown.
            mCutoutController.maybeUpdateLayout();
        }

        @Override
        public void onInteractabilityChanged(Tab tab, boolean interactable) {
            // Force a layout update if the tab is now in the foreground.
            mCutoutController.maybeUpdateLayout();
        }

        @Override
        public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
            assert tab == mTab;

            mCutoutController.onActivityAttachmentChanged(window);
        }
    };

    public static DisplayCutoutTabHelper from(Tab tab) {
        UserDataHost host = tab.getUserDataHost();
        DisplayCutoutTabHelper tabHelper = host.getUserData(USER_DATA_KEY);
        return tabHelper == null ? host.setUserData(USER_DATA_KEY, new DisplayCutoutTabHelper(tab))
                                 : tabHelper;
    }

    @VisibleForTesting
    static class ChromeDisplayCutoutDelegate implements DisplayCutoutController.Delegate {
        private Tab mTab;

        ChromeDisplayCutoutDelegate(Tab tab) {
            mTab = tab;
        }

        @Override
        public Activity getAttachedActivity() {
            return mTab.getWindowAndroid().getActivity().get();
        }
        @Override
        public WebContents getWebContents() {
            return mTab.getWebContents();
        }
        @Override
        public InsetObserverView getInsetObserverView() {
            Activity activity = getAttachedActivity();
            return activity == null ? null : ((ChromeActivity) activity).getInsetObserverView();
        }
        @Override
        public boolean isInteractable() {
            return mTab.isUserInteractable();
        }
    }

    /**
     * Constructs a new DisplayCutoutTabHelper for a specific tab.
     * @param tab The tab that this object belongs to.
     */
    @VisibleForTesting
    DisplayCutoutTabHelper(Tab tab) {
        mTab = tab;
        tab.addObserver(mTabObserver);
        mCutoutController = new DisplayCutoutController(new ChromeDisplayCutoutDelegate(mTab));
    }

    /**
     * Set the viewport fit value for the tab.
     * @param value The new viewport fit value.
     */
    public void setViewportFit(@WebContentsObserver.ViewportFitType int value) {
        mCutoutController.setViewportFit(value);
    }

    @Override
    public void destroy() {
        mTab.removeObserver(mTabObserver);
        mCutoutController.destroy();
    }

    @VisibleForTesting
    static void initForTesting(Tab tab, DisplayCutoutController controller) {
        DisplayCutoutTabHelper tabHelper = new DisplayCutoutTabHelper(tab);
        tabHelper.mCutoutController = controller;
        tab.getUserDataHost().setUserData(USER_DATA_KEY, tabHelper);
    }
}
