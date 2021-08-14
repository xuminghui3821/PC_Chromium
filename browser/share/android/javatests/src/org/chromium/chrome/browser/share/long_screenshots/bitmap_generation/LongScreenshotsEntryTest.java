// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots.bitmap_generation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.LongScreenshotsEntry.EntryStatus;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.paintpreview.player.CompositorStatus;

/** Tests for the LongScreenshotsEntry. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures(ChromeFeatureList.CHROME_SHARE_LONG_SCREENSHOT)
public class LongScreenshotsEntryTest {
    @Mock
    private Context mContext;

    @Mock
    private Tab mTab;

    @Mock
    private LongScreenshotsCompositor mCompositor;

    @Mock
    private LongScreenshotsTabService mTabService;

    @Mock
    private ScreenshotBoundsManager mBoundsManager;

    private Bitmap mTestBitmap = Bitmap.createBitmap(512, 1024, Bitmap.Config.ARGB_8888);

    class TestEntryListener implements LongScreenshotsEntry.EntryListener {
        @EntryStatus
        int mReturnedStatus;

        @Override
        public void onResult(@EntryStatus int status) {
            mReturnedStatus = status;
        }

        public @EntryStatus int getReturnedStatus() {
            return mReturnedStatus;
        }
    }

    class TestBitmapGenerator extends BitmapGenerator {
        private boolean mThrowErrorOnComposite;

        public TestBitmapGenerator(Rect rect) {
            super(mTab, mBoundsManager, null);
            setTabServiceAndCompositorForTest(mTabService, mCompositor);
        }

        void throwErrorOnComposite() {
            mThrowErrorOnComposite = true;
        }

        public void setCompositorStatus(@CompositorStatus int status) {
            mGeneratorCallBack.onCompositorResult(status);
        }

        public void setCaptureStatus(@Status int status) {
            mGeneratorCallBack.onCaptureResult(status);
        }

        @Override
        public int compositeBitmap(
                Rect rect, Runnable errorCallback, Callback<Bitmap> onBitmapGenerated) {
            if (mThrowErrorOnComposite) {
                errorCallback.run();
                return -1;
            }
            onBitmapGenerated.onResult(mTestBitmap);
            return 1;
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        when(mBoundsManager.getCaptureBounds()).thenReturn(new Rect(0, 100, 0, 1000));
        when(mBoundsManager.calculateBoundsRelativeToCapture(any(Rect.class)))
                .thenReturn(new Rect(0, 100, 0, 500));
    }

    @Test
    public void testSuccessfulEntry() {
        TestBitmapGenerator testGenerator = new TestBitmapGenerator(new Rect(0, 0, 200, 1000));

        LongScreenshotsEntry entry = new LongScreenshotsEntry(
                testGenerator, new Rect(0, 1000, 0, 2000), new Callback<Integer>() {
                    @Override
                    public void onResult(Integer result) {
                        assertEquals((int) result, 2097152);
                    }
                });
        TestEntryListener entryListener = new TestEntryListener();
        entry.setListener(entryListener);
        entry.generateBitmap();

        assertEquals(mTestBitmap, entry.getBitmap());
        assertEquals(EntryStatus.BITMAP_GENERATED, entryListener.getReturnedStatus());
    }

    @Test
    public void testBitmapGenerationError() {
        TestBitmapGenerator testGenerator = new TestBitmapGenerator(new Rect(0, 0, 200, 1000));
        testGenerator.throwErrorOnComposite();

        LongScreenshotsEntry entry = new LongScreenshotsEntry(
                testGenerator, new Rect(0, 1000, 0, 2000), new Callback<Integer>() {
                    @Override
                    public void onResult(Integer result) {
                        fail("MemoryUsage should not be called");
                    }
                });
        TestEntryListener entryListener = new TestEntryListener();
        entry.setListener(entryListener);
        entry.generateBitmap();

        assertNull(entry.getBitmap());
        assertEquals(EntryStatus.GENERATION_ERROR, entryListener.getReturnedStatus());
    }
}
