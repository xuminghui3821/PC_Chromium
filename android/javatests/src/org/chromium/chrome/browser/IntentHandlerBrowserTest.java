// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Intent;
import android.speech.RecognizerResultsIntent;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeBrowserTestRule;

import java.util.ArrayList;
import java.util.Collections;

/**
 * Tests for IntentHandler that require Browser initialization.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class IntentHandlerBrowserTest {
    @ClassRule
    public static final ChromeBrowserTestRule sRule = new ChromeBrowserTestRule();

    private static final String VOICE_SEARCH_QUERY = "VOICE_QUERY";
    private static final String VOICE_SEARCH_QUERY_URL = "http://www.google.com/?q=VOICE_QUERY";

    private static final String VOICE_URL_QUERY = "www.google.com";
    private static final String VOICE_URL_QUERY_URL = "INVALID_URLZ";

    @Test
    @MediumTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testGetQueryFromVoiceSearchResultIntent_validVoiceQuery() {
        Intent intent = new Intent(RecognizerResultsIntent.ACTION_VOICE_SEARCH_RESULTS);
        intent.putStringArrayListExtra(RecognizerResultsIntent.EXTRA_VOICE_SEARCH_RESULT_STRINGS,
                new ArrayList<>(Collections.singletonList(VOICE_SEARCH_QUERY)));
        intent.putStringArrayListExtra(RecognizerResultsIntent.EXTRA_VOICE_SEARCH_RESULT_URLS,
                new ArrayList<>(Collections.singletonList(VOICE_SEARCH_QUERY_URL)));
        String query = IntentHandler.getUrlFromVoiceSearchResult(intent);
        Assert.assertEquals(VOICE_SEARCH_QUERY_URL, query);
    }

    @Test
    @MediumTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testGetQueryFromVoiceSearchResultIntent_validUrlQuery() {
        Intent intent = new Intent(RecognizerResultsIntent.ACTION_VOICE_SEARCH_RESULTS);
        intent.putStringArrayListExtra(RecognizerResultsIntent.EXTRA_VOICE_SEARCH_RESULT_STRINGS,
                new ArrayList<>(Collections.singletonList(VOICE_URL_QUERY)));
        intent.putStringArrayListExtra(RecognizerResultsIntent.EXTRA_VOICE_SEARCH_RESULT_URLS,
                new ArrayList<>(Collections.singletonList(VOICE_URL_QUERY_URL)));
        String query = IntentHandler.getUrlFromVoiceSearchResult(intent);
        Assert.assertTrue(String.format("Expected qualified URL: %s, to start "
                                          + "with http://www.google.com",
                                  query),
                query.indexOf("http://www.google.com") == 0);
    }
}
