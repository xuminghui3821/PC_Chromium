// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;
import android.net.Uri;
import android.support.test.InstrumentationRegistry;
import android.util.Base64;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory.CustomTabNavigationDelegate;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.InterceptNavigationDelegateTabHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResultType;
import org.chromium.components.external_intents.InterceptNavigationDelegateImpl;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.concurrent.atomic.AtomicReference;

/**
 * Tests for external navigation handling of Custom Tabs generated by Chrome.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class CustomTabFromChromeExternalNavigationTest {
    @Rule
    public CustomTabActivityTestRule mActivityRule = new CustomTabActivityTestRule();

    @Rule
    public ChromeTabbedActivityTestRule mChromeActivityTestRule =
            new ChromeTabbedActivityTestRule();

    public EmbeddedTestServerRule mServerRule = new EmbeddedTestServerRule();

    private Intent getCustomTabFromChromeIntent(final String url, final boolean markFromChrome) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            intent = LaunchIntentDispatcher.createCustomTabActivityIntent(
                    InstrumentationRegistry.getTargetContext(), intent);
            if (markFromChrome) {
                // Explicitly not marking this as a trusted intent.  If you add the trusted bit,
                // then it prohibits launching external apps.  We want to allow external apps
                // to be launched, so we are intentionally not adding that for now.  Ideally,
                // being opened by Chrome would only be allowed with the corresponding trusted
                // flag, but that requires additional refactoring in our external navigation
                // handling.
                intent.putExtra(CustomTabIntentDataProvider.EXTRA_IS_OPENED_BY_CHROME, true);
            }
            return intent;
        });
    }

    private void startCustomTabFromChrome(String url) {
        Intent intent = getCustomTabFromChromeIntent(url, true);
        mActivityRule.startCustomTabActivityWithIntent(intent);
    }

    @Test
    @Feature("CustomTabFromChrome")
    @MediumTest
    public void testUsingStandardExternalNavigationHandler() {
        startCustomTabFromChrome("about:blank");

        Tab tab = mActivityRule.getActivity().getActivityTab();
        TabDelegateFactory delegateFactory = TabTestUtils.getDelegateFactory(tab);
        Assert.assertTrue(delegateFactory instanceof CustomTabDelegateFactory);
        CustomTabDelegateFactory customTabDelegateFactory =
                ((CustomTabDelegateFactory) delegateFactory);
        Assert.assertFalse(customTabDelegateFactory.getExternalNavigationDelegate()
                                   instanceof CustomTabNavigationDelegate);
    }

    @Test
    @Feature("CustomTabFromChrome")
    @LargeTest
    public void testIntentWithRedirectToApp() {
        final String redirectUrl = "https://maps.google.com/maps?q=1600+amphitheatre+parkway";
        final String initialUrl =
                mServerRule.getServer().getURL("/chrome/test/data/android/redirect/js_redirect.html"
                        + "?replace_text="
                        + Base64.encodeToString(
                                ApiCompatibilityUtils.getBytesUtf8("PARAM_URL"), Base64.URL_SAFE)
                        + ":"
                        + Base64.encodeToString(
                                ApiCompatibilityUtils.getBytesUtf8(redirectUrl), Base64.URL_SAFE));

        mActivityRule.launchActivity(getCustomTabFromChromeIntent(initialUrl, true));
        mActivityRule.waitForActivityNativeInitializationComplete();

        final AtomicReference<InterceptNavigationDelegateImpl> navigationDelegate =
                new AtomicReference<>();

        CriteriaHelper.pollUiThread(() -> {
            return mActivityRule.getActivity().getActivityTab() != null;
        }, "Tab never initialized.");

        CriteriaHelper.pollUiThread(() -> {
            Tab tab = mActivityRule.getActivity().getActivityTab();
            InterceptNavigationDelegateImpl delegate =
                    InterceptNavigationDelegateTabHelper.get(tab);
            if (delegate == null) return false;
            navigationDelegate.set(delegate);
            return true;
        }, "Navigation delegate never initialized.");

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    navigationDelegate.get().getLastOverrideUrlLoadingResultTypeForTests(),
                    Matchers.is(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT));
        });

        CriteriaHelper.pollUiThread(() -> {
            int state = ApplicationStatus.getStateForActivity(mActivityRule.getActivity());
            return state == ActivityState.STOPPED || state == ActivityState.DESTROYED;
        }, "Activity never stopped or destroyed.");
    }
}
