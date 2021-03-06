// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.previews;

import org.chromium.base.metrics.RecordHistogram;

/**
 * Central place to record UMA for Previews in Android.
 */
public final class PreviewsUma {
    // Don't allow this class to be instantiated.
    private PreviewsUma() {}

    // The base histogram name. The current previews type will be added as a suffix.
    private static final String BASE_HISTOGRAM_NAME = "Previews.OmniboxAction.%s";

    // This must remain in sync with PreviewsUserOmniboxAction in
    // //tools/metrics/histograms/enums.xml.

    // User opted out of the preview.
    // OBSOLETE: private static final int ACTION_OPT_OUT = 0;
    // User opened the page info dialog.
    private static final int ACTION_PAGE_INFO_OPENED = 1;
    // The Lite Page badge was displayed at commit.
    // OBSOLETE: private static final int ACTION_LITE_PAGE_AT_COMMIT = 2;
    // OBSOLETE:  private static final int ACTION_LITE_PAGE_AT_FINISH = 3;
    private static final int ACTION_INDEX_BOUNDARY = 4;

    /**
     * Records the given action on the BASE_HISTOGRAM with the committed previews type suffix.
     * @param webContents the active WebContents
     * @param action the action to record
     */
    private static final void recordHistogram(final String previewType, final int action) {
        assert action >= 0 && action < ACTION_INDEX_BOUNDARY;
        if (previewType == null || previewType.length() == 0) return;

        final String histogram = String.format(BASE_HISTOGRAM_NAME, previewType);
        RecordHistogram.recordEnumeratedHistogram(histogram, action, ACTION_INDEX_BOUNDARY);
    }

    /**
     * Records that the user opened the page info dialog, and https image compression message was
     * shown in the page info.
     */
    public static void recordHttpsImageCompressionPageInfoOpened() {
        recordHistogram("HttpsImageCompression", ACTION_PAGE_INFO_OPENED);
    }
}
