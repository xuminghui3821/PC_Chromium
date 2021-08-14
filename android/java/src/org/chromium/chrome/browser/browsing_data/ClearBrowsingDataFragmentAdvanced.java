// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import android.os.Bundle;

import androidx.preference.Preference;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;

import java.util.Arrays;
import java.util.List;

/**
 * A more advanced version of {@link ClearBrowsingDataFragment} with more dialog options and less
 * explanatory text.
 */
public class ClearBrowsingDataFragmentAdvanced extends ClearBrowsingDataFragment {
    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        super.onCreatePreferences(savedInstanceState, rootKey);
        // Remove the search history text preference if it exists, since it should only appear on
        // the basic tab of Clear Browsing Data.
        Preference searchHistoryTextPref =
                findPreference(ClearBrowsingDataFragment.PREF_SEARCH_HISTORY_TEXT);
        if (searchHistoryTextPref != null) {
            getPreferenceScreen().removePreference(searchHistoryTextPref);
        }
    }

    @Override
    protected int getClearBrowsingDataTabType() {
        return ClearBrowsingDataTab.ADVANCED;
    }

    @Override
    protected List<Integer> getDialogOptions() {
        return Arrays.asList(DialogOption.CLEAR_HISTORY, DialogOption.CLEAR_COOKIES_AND_SITE_DATA,
                DialogOption.CLEAR_CACHE, DialogOption.CLEAR_PASSWORDS,
                DialogOption.CLEAR_FORM_DATA, DialogOption.CLEAR_SITE_SETTINGS);
    }

    @Override
    protected void onClearBrowsingData() {
        super.onClearBrowsingData();
        RecordHistogram.recordEnumeratedHistogram("History.ClearBrowsingData.UserDeletedFromTab",
                ClearBrowsingDataTab.ADVANCED, ClearBrowsingDataTab.NUM_TYPES);
        RecordUserAction.record("ClearBrowsingData_AdvancedTab");
    }
}
