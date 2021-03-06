// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots.bitmap_generation;

import android.content.Context;
import android.graphics.Rect;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.LongScreenshotsEntry.EntryStatus;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.paintpreview.player.CompositorStatus;

import java.util.ArrayList;
import java.util.List;

/**
 * Entry manager responsible for managing all the of the {@LongScreenshotEntry}. This should be used
 * to generate and retrieve the needed bitmaps. The first bitmap can be generated by calling
 * {@link generateInitialEntry}.
 */
public class EntryManager {
    private static final int KB_IN_BYTES = 1024;
    // List of all entries in correspondence of the webpage.
    private List<LongScreenshotsEntry> mEntries;
    // List of entries that are queued to generate the bitmap. Entries should only be queued
    // while the capture is in progress.
    private List<LongScreenshotsEntry> mQueuedEntries;
    private BitmapGenerator mGenerator;
    private @EntryStatus int mGeneratorStatus;
    private ScreenshotBoundsManager mBoundsManager;
    private int mMemoryUsedInKb;
    private int mMaxMemoryUsageInKb;

    /**
     * @param context An instance of current Android {@link Context}.
     * @param tab Tab to generate the bitmap for.
     */
    public EntryManager(Context context, Tab tab) {
        mEntries = new ArrayList<LongScreenshotsEntry>();
        mQueuedEntries = new ArrayList<LongScreenshotsEntry>();
        mBoundsManager = new ScreenshotBoundsManager(context, tab);

        mGenerator = new BitmapGenerator(tab, mBoundsManager, createBitmapGeneratorCallback());
        mGenerator.captureTab();
        updateGeneratorStatus(EntryStatus.CAPTURE_IN_PROGRESS);
        // TODO(cb/1153969): Make this a finch param instead.
        mMaxMemoryUsageInKb = 16 * 1024;
    }

    /**
     * Generates the first bitmap of the page that is the height of the phone display. Callers of
     * this function should add a listener to the returned entry to get that status of the
     * generation and retrieve the bitmap.
     */
    public LongScreenshotsEntry generateInitialEntry() {
        LongScreenshotsEntry entry = new LongScreenshotsEntry(
                mGenerator, mBoundsManager.getInitialEntryBounds(), this::updateMemoryUsage);
        processEntry(entry, false);
        // Pre-compute these entries so that they are ready to go when the user starts scrolling.
        getPreviousEntry(entry.getId());
        getNextEntry(entry.getId());
        return entry;
    }

    /**
     * Creates the entry to generate the bitmap before the one passed in.
     *
     * @param relativeToId Id to base the new entry off of.
     * @return The new entry that generates the bitmap.
     */
    public LongScreenshotsEntry getPreviousEntry(int relativeToId) {
        int found = -1;
        for (int i = 0; i < mEntries.size(); i++) {
            if (mEntries.get(i).getId() == relativeToId) {
                found = i;
            }
        }

        if (found == -1) {
            return null;
        }

        if (found > 0) {
            return mEntries.get(found - 1);
        }

        // Before generating a new bitmap, make sure too much memory has not already been used.
        if (mMemoryUsedInKb >= mMaxMemoryUsageInKb) {
            return LongScreenshotsEntry.createEntryWithStatus(EntryStatus.INSUFFICIENT_MEMORY);
        }

        Rect bounds = mBoundsManager.calculateClipBoundsAbove(mEntries.get(0).getId());
        if (bounds == null) {
            return LongScreenshotsEntry.createEntryWithStatus(EntryStatus.BOUNDS_ABOVE_CAPTURE);
        }

        // found = 0
        LongScreenshotsEntry newEntry =
                new LongScreenshotsEntry(mGenerator, bounds, this::updateMemoryUsage);
        processEntry(newEntry, true);
        return newEntry;
    }

    /**
     * Creates the entry to generate the bitmap after the one passed in.
     *
     * @param relativeToId Id to base the new entry off of.
     * @return The new entry that generates the bitmap.
     */
    public LongScreenshotsEntry getNextEntry(int relativeToId) {
        int found = -1;
        for (int i = 0; i < mEntries.size(); i++) {
            if (mEntries.get(i).getId() == relativeToId) {
                found = i;
            }
        }

        if (found == -1) {
            return null;
        }

        if (found < mEntries.size() - 1) {
            return mEntries.get(found + 1);
        }

        // Before generating a new bitmap, make sure too much memory has not already been used.
        if (mMemoryUsedInKb >= mMaxMemoryUsageInKb) {
            return LongScreenshotsEntry.createEntryWithStatus(EntryStatus.INSUFFICIENT_MEMORY);
        }

        // found = last entry in the arraylist
        int newStartY = mEntries.get(mEntries.size() - 1).getEndYAxis() + 1;

        Rect bounds = mBoundsManager.calculateClipBoundsBelow(newStartY);
        if (bounds == null) {
            return LongScreenshotsEntry.createEntryWithStatus(EntryStatus.BOUNDS_BELOW_CAPTURE);
        }

        LongScreenshotsEntry newEntry =
                new LongScreenshotsEntry(mGenerator, bounds, this::updateMemoryUsage);
        processEntry(newEntry, false);
        return newEntry;
    }

    private void processEntry(LongScreenshotsEntry entry, boolean addToBeginningOfList) {
        if (mGeneratorStatus == EntryStatus.CAPTURE_COMPLETE) {
            entry.generateBitmap();
        } else if (mGeneratorStatus == EntryStatus.CAPTURE_IN_PROGRESS) {
            mQueuedEntries.add(entry);
        } else {
            entry.updateStatus(mGeneratorStatus);
        }

        // Add to the list of all entries
        if (addToBeginningOfList) {
            mEntries.add(0, entry);
        } else {
            mEntries.add(entry);
        }
    }

    /**
     * Updates based on the generator status. If the capture is complete, generates the bitmap for
     * all the queued entries.
     *
     * @param status New status from the generator.
     */
    private void updateGeneratorStatus(@EntryStatus int status) {
        mGeneratorStatus = status;
        if (status == EntryStatus.CAPTURE_COMPLETE) {
            for (LongScreenshotsEntry entry : mQueuedEntries) {
                entry.generateBitmap();
            }
            mQueuedEntries.clear();
        } else {
            for (LongScreenshotsEntry entry : mQueuedEntries) {
                entry.updateStatus(status);
            }
        }
    }

    private void updateMemoryUsage(int bytedUsed) {
        mMemoryUsedInKb += (bytedUsed / KB_IN_BYTES);
    }

    /**
     * Creates the default BitmapGenerator to be used to retrieve the state of the generation. This
     * is the default implementation and should only be overridden for tests.
     */
    @VisibleForTesting
    public BitmapGenerator.GeneratorCallBack createBitmapGeneratorCallback() {
        return new BitmapGenerator.GeneratorCallBack() {
            @Override
            public void onCompositorResult(@CompositorStatus int status) {
                // TODO(tgupta): Add metrics logging here.
                if (status == CompositorStatus.STOPPED_DUE_TO_MEMORY_PRESSURE
                        || status == CompositorStatus.SKIPPED_DUE_TO_MEMORY_PRESSURE) {
                    updateGeneratorStatus(EntryStatus.INSUFFICIENT_MEMORY);
                } else if (status == CompositorStatus.OK) {
                    updateGeneratorStatus(EntryStatus.CAPTURE_COMPLETE);
                } else {
                    updateGeneratorStatus(EntryStatus.GENERATION_ERROR);
                }
            }

            @Override
            public void onCaptureResult(@Status int status) {
                // TODO(tgupta): Add metrics logging here.
                if (status == Status.LOW_MEMORY_DETECTED) {
                    updateGeneratorStatus(EntryStatus.INSUFFICIENT_MEMORY);
                } else if (status != Status.OK) {
                    updateGeneratorStatus(EntryStatus.GENERATION_ERROR);
                }
            }
        };
    }
}
