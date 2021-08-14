// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.chrome.test.util.ViewUtils.createMotionEvent;

import android.support.test.InstrumentationRegistry;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.OverviewModeBehaviorWatcher;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.ArrayDeque;

/**
 * Test suite for ContentView focus and its interaction with Tab switcher,
 * Tab swiping, etc.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ContentViewFocusTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final int WAIT_RESPONSE_MS = 2000;

    private final ArrayDeque<Boolean> mFocusChanges = new ArrayDeque<Boolean>();

    private String mTitle;

    private float mDpToPx;

    @Before
    public void setUp() throws InterruptedException {
        mDpToPx = InstrumentationRegistry.getInstrumentation()
                          .getContext()
                          .getResources()
                          .getDisplayMetrics()
                          .density;
    }

    private void addFocusChangedListener(View view) {
        view.setOnFocusChangeListener((v, hasFocus) -> {
            synchronized (mFocusChanges) {
                mFocusChanges.add(Boolean.valueOf(hasFocus));
                mFocusChanges.notify();
            }
        });
    }

    private boolean blockForFocusChanged() throws InterruptedException  {
        long endTime = System.currentTimeMillis() + WAIT_RESPONSE_MS * 2;
        synchronized (mFocusChanges) {
            while (true) {
                if (!mFocusChanges.isEmpty()) {
                    return mFocusChanges.removeFirst();
                }
                long sleepTime = endTime - System.currentTimeMillis();
                if (sleepTime <= 0) {
                    throw new RuntimeException("Didn't get event");
                }
                mFocusChanges.wait(sleepTime);
            }
        }
    }

    private boolean haveFocusChanges() {
        synchronized (mFocusChanges) {
            return !mFocusChanges.isEmpty();
        }
    }

    /**
     * Verify ContentView loses/gains focus on swiping tab.
     *
     * @throws Exception
     * @MediumTest
     * @Feature({"TabContents"})
     * @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
     */
    @Test
    @FlakyTest(message = "http://crbug.com/172473")
    public void testHideSelectionOnPhoneTabSwiping() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        // Setup
        ChromeTabUtils.newTabsFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity(), 2);
        String url = UrlUtils.getIsolatedTestFileUrl(
                "chrome/test/data/android/content_view_focus/content_view_focus_long_text.html");
        mActivityTestRule.loadUrl(url);
        View view = mActivityTestRule.getActivity().getActivityTab().getContentView();

        // Give the content view focus
        TestTouchUtils.longClickView(InstrumentationRegistry.getInstrumentation(), view, 50, 10);
        Assert.assertTrue("ContentView is focused", view.hasFocus());

        // Start the swipe
        addFocusChangedListener(view);
        final SwipeHandler swipeHandler =
                mActivityTestRule.getActivity().getLayoutManager().getToolbarSwipeHandler();
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            swipeHandler.onSwipeStarted(ScrollDirection.RIGHT, createMotionEvent(0, 0));
            swipeHandler.onSwipeUpdated(
                    createMotionEvent(100 * mDpToPx, 0), 100 * mDpToPx, 0, 100 * mDpToPx, 0);
        });

        CriteriaHelper.pollUiThread(() -> {
            LayoutManagerImpl driver = mActivityTestRule.getActivity().getLayoutManager();
            return !driver.getActiveLayout().shouldDisplayContentOverlay();
        }, "Layout still requesting Tab Android view be attached");

        // Make sure the view loses focus. It is immediately given focus back
        // because it's the only focusable view.
        Assert.assertFalse("Content view didn't lose focus", blockForFocusChanged());

        // End the drag
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, swipeHandler::onSwipeFinished);

        CriteriaHelper.pollUiThread(() -> {
            LayoutManagerImpl driver = mActivityTestRule.getActivity().getLayoutManager();
            return driver.getActiveLayout().shouldDisplayContentOverlay();
        }, "Layout not requesting Tab Android view be attached");

        Assert.assertTrue("Content view didn't regain focus", blockForFocusChanged());
        Assert.assertFalse("Unexpected focus change", haveFocusChanges());
    }

    /**
     * Verify ContentView loses/gains focus on overview mode.
     *
     * @throws Exception
     * @Feature({"TabContents"})
     */
    @Test
    @MediumTest
    @Feature({"TabContents"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @FlakyTest(message = "http://crbug.com/967128")
    public void testHideSelectionOnPhoneTabSwitcher() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        // Setup
        OverviewModeBehaviorWatcher showWatcher = new OverviewModeBehaviorWatcher(
                mActivityTestRule.getActivity().getLayoutManager(), true, false);
        OverviewModeBehaviorWatcher hideWatcher = new OverviewModeBehaviorWatcher(
                mActivityTestRule.getActivity().getLayoutManager(), false, true);
        View currentView = mActivityTestRule.getActivity().getActivityTab().getContentView();
        addFocusChangedListener(currentView);

        // Enter the tab switcher
        View tabSwitcherButton =
                mActivityTestRule.getActivity().findViewById(R.id.tab_switcher_button);
        Assert.assertNotNull("'tab_switcher_button' view is not found.", tabSwitcherButton);
        TouchCommon.singleClickView(
                mActivityTestRule.getActivity().findViewById(R.id.tab_switcher_button));
        showWatcher.waitForBehavior();

        // Make sure the view loses focus. It is immediately given focus back
        // because it's the only focusable view.
        Assert.assertFalse("Content view didn't lose focus", blockForFocusChanged());

        // Hide the tab switcher
        tabSwitcherButton = mActivityTestRule.getActivity().findViewById(R.id.tab_switcher_button);
        Assert.assertNotNull("'tab_switcher_button' view is not found.", tabSwitcherButton);
        TouchCommon.singleClickView(
                mActivityTestRule.getActivity().findViewById(R.id.tab_switcher_button));
        hideWatcher.waitForBehavior();

        Assert.assertTrue("Content view didn't regain focus", blockForFocusChanged());
        Assert.assertFalse("Unexpected focus change", haveFocusChanges());
    }

    /**
     * Verify ContentView window focus changes propagate to contents.
     *
     * @throws Exception
     */
    @Test
    @MediumTest
    public void testPauseTriggersBlur() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        final WebContents webContents = mActivityTestRule.getWebContents();
        final CallbackHelper onTitleUpdatedHelper = new CallbackHelper();
        final WebContentsObserver observer =
                new WebContentsObserver(webContents) {
                    @Override
                    public void titleWasSet(String title) {
                        mTitle = title;
                        onTitleUpdatedHelper.notifyCalled();
                    }
                };
        int callCount = onTitleUpdatedHelper.getCallCount();
        String url = UrlUtils.getIsolatedTestFileUrl(
                "chrome/test/data/android/content_view_focus/content_view_blur_focus.html");
        mActivityTestRule.loadUrl(url);
        ViewEventSink eventSink = WebContentsUtils.getViewEventSink(webContents);
        onTitleUpdatedHelper.waitForCallback(callCount);
        Assert.assertEquals("initial", mTitle);
        callCount = onTitleUpdatedHelper.getCallCount();
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> eventSink.onPauseForTesting());
        onTitleUpdatedHelper.waitForCallback(callCount);
        Assert.assertEquals("blurred", mTitle);
        callCount = onTitleUpdatedHelper.getCallCount();
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> eventSink.onResumeForTesting());
        onTitleUpdatedHelper.waitForCallback(callCount);
        Assert.assertEquals("focused", mTitle);
        mActivityTestRule.getWebContents().removeObserver(observer);
    }
}
