// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_ICON_VIEW_H_

#include "chrome/browser/sharing/sharing_ui_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/metadata/metadata_header_macros.h"

namespace views {
// TODO(ellyjones): Remove this.
class BubbleDialogDelegateView;
}  // namespace views

// The location bar icon to show the sharing features bubble.
class SharingIconView : public PageActionIconView {
 public:
  METADATA_HEADER(SharingIconView);

  using GetControllerCallback =
      base::RepeatingCallback<SharingUiController*(content::WebContents*)>;

  using GetBubbleCallback =
      base::RepeatingCallback<views::BubbleDialogDelegateView*(SharingDialog*)>;

  SharingIconView(IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
                  PageActionIconView::Delegate* page_action_icon_delegate,
                  GetControllerCallback get_controller,
                  GetBubbleCallback get_bubble);
  SharingIconView(const SharingIconView&) = delete;
  SharingIconView& operator=(const SharingIconView&) = delete;
  ~SharingIconView() override;

  void StartLoadingAnimation();
  void StopLoadingAnimation();

 protected:
  // PageActionIconView:
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  bool IsTriggerableEvent(const ui::Event& event) override;
  const gfx::VectorIcon& GetVectorIconBadge() const override;
  views::BubbleDialogDelegate* GetBubble() const override;
  void UpdateImpl() override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  std::u16string GetTextForTooltipAndAccessibleName() const override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  SharingUiController* GetController() const;

  void UpdateInkDrop(bool activate);
  void UpdateOpacity();

 private:
  SharingUiController* last_controller_ = nullptr;
  bool loading_animation_ = false;
  bool should_show_error_ = false;
  GetControllerCallback get_controller_callback_;
  GetBubbleCallback get_bubble_callback_;
};
#endif  // CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_ICON_VIEW_H_
