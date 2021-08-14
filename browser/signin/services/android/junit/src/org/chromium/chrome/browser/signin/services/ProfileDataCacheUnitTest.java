// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.identitymanager.AccountInfoService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.IdentityManagerJni;

/**
 * Unit tests for {@link ProfileDataCache}
 */
@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({ChromeFeatureList.DEPRECATE_MENAGERIE_API})
public class ProfileDataCacheUnitTest {
    private static final long NATIVE_IDENTITY_MANAGER = 10001L;
    private static final String ACCOUNT_EMAIL = "test@gmail.com";

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final JniMocker mocker = new JniMocker();

    @Rule
    public final Features.JUnitProcessor processor = new Features.JUnitProcessor();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Mock
    private IdentityManager.Natives mIdentityManagerNativeMock;

    @Mock
    private ProfileDataCache.Observer mObserverMock;

    private final IdentityManager mIdentityManager =
            IdentityManager.create(NATIVE_IDENTITY_MANAGER, null /* OAuth2TokenService */);

    private ProfileDataCache mProfileDataCache;

    @Before
    public void setUp() {
        mocker.mock(IdentityManagerJni.TEST_HOOKS, mIdentityManagerNativeMock);
        AccountInfoService.init(mIdentityManager);
        mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                RuntimeEnvironment.application.getApplicationContext());
    }

    @After
    public void tearDown() {
        AccountInfoService.resetForTests();
    }

    @Test
    public void accountInfoIsUpdatedWithOnlyFullName() {
        final String fullName = "full name1";
        final AccountInfo accountInfo = new AccountInfo(new CoreAccountId("gaia-id-test"),
                ACCOUNT_EMAIL, "gaia-id-test", fullName, null, null);
        mProfileDataCache.addObserver(mObserverMock);
        Assert.assertNull(mProfileDataCache.getProfileDataOrDefault(ACCOUNT_EMAIL).getFullName());

        mIdentityManager.onExtendedAccountInfoUpdated(accountInfo);

        Assert.assertEquals(
                fullName, mProfileDataCache.getProfileDataOrDefault(ACCOUNT_EMAIL).getFullName());
    }

    @Test
    public void accountInfoIsUpdatedWithOnlyGivenName() {
        final String givenName = "given name1";
        final AccountInfo accountInfo = new AccountInfo(new CoreAccountId("gaia-id-test"),
                ACCOUNT_EMAIL, "gaia-id-test", null, givenName, null);
        mProfileDataCache.addObserver(mObserverMock);
        Assert.assertNull(mProfileDataCache.getProfileDataOrDefault(ACCOUNT_EMAIL).getGivenName());

        mIdentityManager.onExtendedAccountInfoUpdated(accountInfo);

        Assert.assertEquals(
                givenName, mProfileDataCache.getProfileDataOrDefault(ACCOUNT_EMAIL).getGivenName());
    }
}
