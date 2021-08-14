// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.areAnimatorsEnabled;

import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Build;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.FrameLayout;

import androidx.core.content.ContextCompat;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.DummyUiActivityTestCase;
import org.chromium.ui.widget.ChromeImageView;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for {@link TabGridPanelViewBinder}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TabGridPanelViewBinderTest extends DummyUiActivityTestCase {
    private static final int CONTENT_TOP_MARGIN = 56;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mMCP;
    private TabGroupUiToolbarView mToolbarView;
    private RecyclerView mContentView;
    private TabGridDialogView mTabGridDialogView;
    private ChromeImageView mRightButton;
    private ChromeImageView mLeftButton;
    private EditText mTitleTextView;
    private View mMainContent;
    private ScrimCoordinator mScrimCoordinator;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            FrameLayout parentView = new FrameLayout(getActivity());
            getActivity().setContentView(parentView);
            mContentView =
                    (TabListRecyclerView) LayoutInflater.from(getActivity())
                            .inflate(R.layout.tab_list_recycler_view_layout, parentView, false);
            mContentView.setLayoutManager(new GridLayoutManager(getActivity(), 2));
            mToolbarView = (TabGroupUiToolbarView) LayoutInflater.from(getActivity())
                                   .inflate(R.layout.bottom_tab_grid_toolbar, mContentView, false);
            LayoutInflater.from(getActivity())
                    .inflate(R.layout.tab_grid_dialog_layout, parentView, true);
            mTabGridDialogView = parentView.findViewById(R.id.dialog_parent_view);
            mLeftButton = mToolbarView.findViewById(R.id.toolbar_left_button);
            mRightButton = mToolbarView.findViewById(R.id.toolbar_right_button);
            mTitleTextView = mToolbarView.findViewById(R.id.title);
            mMainContent = mToolbarView.findViewById(R.id.main_content);
            mScrimCoordinator = new ScrimCoordinator(getActivity(), null, parentView, Color.RED);
            mTabGridDialogView.setupScrimCoordinator(mScrimCoordinator);

            mModel = new PropertyModel(TabGridPanelProperties.ALL_KEYS);

            mMCP = PropertyModelChangeProcessor.create(mModel,
                    new TabGridPanelViewBinder.ViewHolder(
                            mToolbarView, mContentView, mTabGridDialogView),
                    TabGridPanelViewBinder::bind);
        });
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetCollapseClickListener() {
        AtomicBoolean leftButtonClicked = new AtomicBoolean();
        leftButtonClicked.set(false);
        mLeftButton.performClick();
        Assert.assertFalse(leftButtonClicked.get());

        mModel.set(TabGridPanelProperties.COLLAPSE_CLICK_LISTENER,
                (View view) -> leftButtonClicked.set(true));

        mLeftButton.performClick();
        Assert.assertTrue(leftButtonClicked.get());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetAddClickListener() {
        AtomicBoolean rightButtonClicked = new AtomicBoolean();
        rightButtonClicked.set(false);
        mRightButton.performClick();
        Assert.assertFalse(rightButtonClicked.get());

        mModel.set(TabGridPanelProperties.ADD_CLICK_LISTENER,
                (View view) -> rightButtonClicked.set(true));

        mRightButton.performClick();
        Assert.assertTrue(rightButtonClicked.get());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetHeaderTitle() {
        String title = "1024 tabs";
        Assert.assertNotEquals(title, mTitleTextView.getText());

        mModel.set(TabGridPanelProperties.HEADER_TITLE, title);

        Assert.assertEquals(title, mTitleTextView.getText().toString());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testContentTopMargin() {
        // Since setting content top margin is only used in sheet, we can assume that the parent is
        // a FrameLayout here.
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(0, 0);
        params.setMargins(0, 0, 0, 0);
        mContentView.setLayoutParams(new FrameLayout.LayoutParams(0, 0));
        Assert.assertEquals(
                0, ((ViewGroup.MarginLayoutParams) mContentView.getLayoutParams()).topMargin);

        mModel.set(TabGridPanelProperties.CONTENT_TOP_MARGIN, CONTENT_TOP_MARGIN);

        Assert.assertEquals(CONTENT_TOP_MARGIN,
                ((ViewGroup.MarginLayoutParams) mContentView.getLayoutParams()).topMargin);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetPrimaryColor() {
        int color = ContextCompat.getColor(getActivity(), R.color.modern_blue_300);
        Assert.assertNull(mMainContent.getBackground());
        Assert.assertNull(mContentView.getBackground());

        mModel.set(TabGridPanelProperties.PRIMARY_COLOR, color);

        Assert.assertEquals(color, ((ColorDrawable) mMainContent.getBackground()).getColor());
        Assert.assertEquals(color, ((ColorDrawable) mContentView.getBackground()).getColor());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetTint() {
        ColorStateList tint = ThemeUtils.getThemedToolbarIconTint(getActivity(), true);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Assert.assertNotEquals(tint, mLeftButton.getImageTintList());
            Assert.assertNotEquals(tint, mRightButton.getImageTintList());
        }
        Assert.assertNotEquals(tint, mTitleTextView.getTextColors());

        mModel.set(TabGridPanelProperties.TINT, tint);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Assert.assertEquals(tint, mLeftButton.getImageTintList());
            Assert.assertEquals(tint, mRightButton.getImageTintList());
        }
        Assert.assertEquals(tint, mTitleTextView.getTextColors());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetScrimViewObserver() {
        AtomicBoolean scrimViewClicked = new AtomicBoolean();
        scrimViewClicked.set(false);
        Runnable scrimClickRunnable = () -> scrimViewClicked.set(true);

        mModel.set(TabGridPanelProperties.SCRIMVIEW_CLICK_RUNNABLE, scrimClickRunnable);
        // Open the dialog to show the ScrimView.
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);
        View scrimView = mScrimCoordinator.getViewForTesting();
        scrimView.performClick();
        Assert.assertTrue(scrimViewClicked.get());
    }

    @Test
    @SmallTest
    public void testSetDialogVisibility() {
        Assert.assertNull(mTabGridDialogView.getCurrentDialogAnimatorForTesting());

        // Setup basic dialog animation and a dummy scrim view click runnable. These are always
        // initialized before the visibility of dialog is set.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGridDialogView.setupDialogAnimation(null);
            mTabGridDialogView.setScrimClickRunnable(() -> {});
        });

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true));

        if (areAnimatorsEnabled()) {
            Assert.assertNotNull(mTabGridDialogView.getCurrentDialogAnimatorForTesting());
        }
        Assert.assertEquals(View.VISIBLE, mTabGridDialogView.getVisibility());
        CriteriaHelper.pollUiThread(
                ()
                        -> Criteria.checkThat(
                                mTabGridDialogView.getCurrentDialogAnimatorForTesting(),
                                Matchers.nullValue()));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, false));

        if (areAnimatorsEnabled()) {
            Assert.assertNotNull(mTabGridDialogView.getCurrentDialogAnimatorForTesting());
        }
        CriteriaHelper.pollUiThread(
                ()
                        -> Criteria.checkThat(
                                mTabGridDialogView.getCurrentDialogAnimatorForTesting(),
                                Matchers.nullValue()));
        Assert.assertEquals(View.GONE, mTabGridDialogView.getVisibility());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetAnimationSourceView() {
        // Initially, the show animation set is empty.
        Assert.assertEquals(0,
                mTabGridDialogView.getShowDialogAnimationForTesting().getChildAnimations().size());

        // When set animation source view as null, the show animation is set to be basic fade-in
        // which contains only one animation in animation set.
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, null);
        Assert.assertEquals(1,
                mTabGridDialogView.getShowDialogAnimationForTesting().getChildAnimations().size());

        // Create a dummy source view to setup the dialog animation.
        View sourceView = new View(getActivity());

        // When set with a specific animation source view, the show animation contains 6 child
        // animations.
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, sourceView);
        Assert.assertEquals(6,
                mTabGridDialogView.getShowDialogAnimationForTesting().getChildAnimations().size());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetUngroupbarStatus() {
        // Default status for ungroup bar is hidden.
        Assert.assertEquals(TabGridDialogView.UngroupBarStatus.HIDE,
                mTabGridDialogView.getUngroupBarStatusForTesting());

        mModel.set(
                TabGridPanelProperties.UNGROUP_BAR_STATUS, TabGridDialogView.UngroupBarStatus.SHOW);
        Assert.assertEquals(TabGridDialogView.UngroupBarStatus.SHOW,
                mTabGridDialogView.getUngroupBarStatusForTesting());

        mModel.set(TabGridPanelProperties.UNGROUP_BAR_STATUS,
                TabGridDialogView.UngroupBarStatus.HOVERED);
        Assert.assertEquals(TabGridDialogView.UngroupBarStatus.HOVERED,
                mTabGridDialogView.getUngroupBarStatusForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetDialogBackgroundResource() {
        int normalResourceId = R.drawable.tab_grid_dialog_background;
        int incognitoResourceId = R.drawable.tab_grid_dialog_background_incognito;
        // Default setup is in normal mode.
        Assert.assertEquals(
                normalResourceId, mTabGridDialogView.getBackgroundDrawableResourceIdForTesting());

        mModel.set(TabGridPanelProperties.DIALOG_BACKGROUND_RESOURCE_ID, incognitoResourceId);

        Assert.assertEquals(incognitoResourceId,
                mTabGridDialogView.getBackgroundDrawableResourceIdForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetUngroupbarBackgroundColor() {
        int normalColorId = R.color.tab_grid_dialog_background_color;
        int incognitoColorId = R.color.tab_grid_dialog_background_color_incognito;
        // Default setup is in normal mode.
        Assert.assertEquals(normalColorId,
                mTabGridDialogView.getUngroupBarBackgroundColorResourceIdForTesting());

        mModel.set(TabGridPanelProperties.DIALOG_UNGROUP_BAR_BACKGROUND_COLOR_ID, incognitoColorId);

        Assert.assertEquals(incognitoColorId,
                mTabGridDialogView.getUngroupBarBackgroundColorResourceIdForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetUngroupbarHoveredBackgroundColor() {
        int normalColorId = R.color.tab_grid_card_selected_color;
        int incognitoColorId = R.color.tab_grid_card_selected_color_incognito;
        // Default setup is in normal mode.
        Assert.assertEquals(normalColorId,
                mTabGridDialogView.getUngroupBarHoveredBackgroundColorResourceIdForTesting());

        mModel.set(TabGridPanelProperties.DIALOG_UNGROUP_BAR_HOVERED_BACKGROUND_COLOR_ID,
                incognitoColorId);

        Assert.assertEquals(incognitoColorId,
                mTabGridDialogView.getUngroupBarHoveredBackgroundColorResourceIdForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetUngroupbarTextAppearance() {
        int normalStyleId = R.style.TextAppearance_TextMediumThick_Blue;
        int incognitoStyleId = R.style.TextAppearance_TextMediumThick_Blue_Light;
        // Default setup is in normal mode.
        Assert.assertEquals(
                normalStyleId, mTabGridDialogView.getUngroupBarTextAppearanceForTesting());

        mModel.set(TabGridPanelProperties.DIALOG_UNGROUP_BAR_TEXT_APPEARANCE, incognitoStyleId);

        Assert.assertEquals(
                incognitoStyleId, mTabGridDialogView.getUngroupBarTextAppearanceForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetMainContentVisibility() {
        mContentView.setVisibility(View.INVISIBLE);
        Assert.assertEquals(View.INVISIBLE, mContentView.getVisibility());

        mModel.set(TabGridPanelProperties.IS_MAIN_CONTENT_VISIBLE, true);

        Assert.assertEquals(View.VISIBLE, mContentView.getVisibility());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetTitleTextWatcher() {
        String title = "cool tabs";
        AtomicBoolean titleTextUpdated = new AtomicBoolean();
        titleTextUpdated.set(false);

        TextWatcher textWatcher = new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence charSequence, int i, int i1, int i2) {}

            @Override
            public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {}

            @Override
            public void afterTextChanged(Editable editable) {
                titleTextUpdated.set(true);
            }
        };
        mModel.set(TabGridPanelProperties.TITLE_TEXT_WATCHER, textWatcher);

        mTitleTextView.setText(title);
        Assert.assertTrue(titleTextUpdated.get());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetTitleTextOnFocusListener() {
        AtomicBoolean textFocusChanged = new AtomicBoolean();
        textFocusChanged.set(false);
        Assert.assertFalse(mTitleTextView.isFocused());

        View.OnFocusChangeListener listener = (view, b) -> textFocusChanged.set(true);
        mModel.set(TabGridPanelProperties.TITLE_TEXT_ON_FOCUS_LISTENER, listener);
        mTitleTextView.requestFocus();

        Assert.assertTrue(mTitleTextView.isFocused());
        Assert.assertTrue(textFocusChanged.get());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetCursorVisibility() {
        mTitleTextView.setCursorVisible(false);

        mModel.set(TabGridPanelProperties.TITLE_CURSOR_VISIBILITY, true);

        Assert.assertTrue(mTitleTextView.isCursorVisible());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetIsTitleTextFocused() {
        Assert.assertFalse(mTitleTextView.isFocused());

        mModel.set(TabGridPanelProperties.IS_TITLE_TEXT_FOCUSED, true);

        // Binder should ignore set focus signal to avoid duplicate setting.
        Assert.assertFalse(mTitleTextView.isFocused());

        mTitleTextView.requestFocus();
        Assert.assertTrue(mTitleTextView.isFocused());

        mModel.set(TabGridPanelProperties.IS_TITLE_TEXT_FOCUSED, false);

        Assert.assertFalse(mTitleTextView.isFocused());
    }

    @Override
    public void tearDownTest() throws Exception {
        mMCP.destroy();
        super.tearDownTest();
    }
}
