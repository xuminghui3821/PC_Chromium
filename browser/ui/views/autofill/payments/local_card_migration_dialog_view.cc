// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/local_card_migration_dialog_view.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/i18n/message_formatter.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/payments/local_card_migration_dialog_factory.h"
#include "chrome/browser/ui/autofill/payments/local_card_migration_dialog_state.h"
#include "chrome/browser/ui/autofill/payments/payments_ui_constants.h"
#include "chrome/browser/ui/views/autofill/payments/migratable_card_view.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace autofill {

namespace {

// Create the title label container for the migration dialogs. The title
// text depends on the |view_state| of the dialog.
std::unique_ptr<views::Label> CreateTitle(
    LocalCardMigrationDialogState view_state,
    LocalCardMigrationDialogView* dialog_view,
    int card_list_size) {
  int message_id;
  switch (view_state) {
    case LocalCardMigrationDialogState::kOffered:
      message_id = IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_TITLE_OFFER;
      break;
    case LocalCardMigrationDialogState::kFinished:
      message_id = IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_TITLE_DONE;
      break;
    case LocalCardMigrationDialogState::kActionRequired:
      message_id = IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_TITLE_FIX;
      break;
  }

  auto title = std::make_unique<views::Label>(
      l10n_util::GetPluralStringFUTF16(message_id, card_list_size));
  constexpr int kMigrationDialogTitleFontSize = 8;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  constexpr int kMigrationDialogTitleMarginTop = 0;
#else
  constexpr int kMigrationDialogTitleMarginTop = 12;
#endif
  title->SetBorder(views::CreateEmptyBorder(
      /*top=*/kMigrationDialogTitleMarginTop,
      /*left=*/kMigrationDialogInsets.left(), /*bottom=*/0,
      /*right=*/kMigrationDialogInsets.right()));
  title->SetFontList(gfx::FontList().Derive(kMigrationDialogTitleFontSize,
                                            gfx::Font::NORMAL,
                                            gfx::Font::Weight::NORMAL));
  title->SetEnabledColor(dialog_view->GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_LabelEnabledColor));
  constexpr int kMigrationDialogTitleLineHeight = 20;
  title->SetMultiLine(true);
  title->SetLineHeight(kMigrationDialogTitleLineHeight);
  return title;
}

// Create the explanation text label with |user_email| for the migration
// dialogs. The text content depends on the |view_state| of the dialog and the
// |card_list_size|.
std::unique_ptr<views::Label> CreateExplanationText(
    LocalCardMigrationDialogState view_state,
    int card_list_size,
    const std::u16string& user_email) {
  auto explanation_text = std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  switch (view_state) {
    case LocalCardMigrationDialogState::kOffered:
      DCHECK(!user_email.empty());
      explanation_text->SetText(
          base::i18n::MessageFormatter::FormatWithNumberedArgs(
              l10n_util::GetStringFUTF16(
                  IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_MESSAGE_OFFER,
                  user_email),
              card_list_size));
      break;
    case LocalCardMigrationDialogState::kFinished:
      explanation_text->SetText(l10n_util::GetPluralStringFUTF16(
          card_list_size == 0
              ? IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_MESSAGE_INVALID_CARD_REMOVED
              : IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_MESSAGE_DONE,
          card_list_size));
      break;
    case LocalCardMigrationDialogState::kActionRequired:
      explanation_text->SetText(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_MESSAGE_FIX));
      break;
  }
  explanation_text->SetMultiLine(true);
  explanation_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return explanation_text;
}

// Create the scroll view of cards in |migratable_credit_cards|, and each
// row in the scroll view is a MigratableCardView. |dialog_view|
// will be notified whenever the checkbox or the trash can button
// (if any) in any row is clicked. The content and the layout of the
// scroll view depends on |should_show_checkbox|.
std::unique_ptr<views::ScrollView> CreateCardList(
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    LocalCardMigrationDialogView* dialog_view,
    bool should_show_checkbox) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  auto card_list_view = std::make_unique<views::View>();
  constexpr int kCardListSmallVerticalDistance = 8;
  auto* card_list_view_layout =
      card_list_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(),
          should_show_checkbox
              ? kCardListSmallVerticalDistance
              : provider->GetDistanceMetric(
                    views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  card_list_view_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  for (size_t index = 0; index < migratable_credit_cards.size(); ++index) {
    card_list_view->AddChildView(new MigratableCardView(
        migratable_credit_cards[index], dialog_view, should_show_checkbox));
  }

  auto card_list_scroll_view = std::make_unique<views::ScrollView>();
  card_list_scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  card_list_scroll_view->SetContents(std::move(card_list_view));
  card_list_scroll_view->SetDrawOverflowIndicator(false);
  constexpr int kCardListScrollViewHeight = 140;
  card_list_scroll_view->ClipHeightTo(0, kCardListScrollViewHeight);
  return card_list_scroll_view;
}

// Create the view containing the |tip_message| shown to the user.
std::unique_ptr<views::View> CreateTip(
    const std::u16string& tip_message,
    LocalCardMigrationDialogView* dialog_view) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  // Set up the tip text container with inset, background and a solid border.
  auto tip_text_container = std::make_unique<views::View>();
  gfx::Insets container_insets =
      provider->GetInsetsMetric(views::INSETS_DIALOG_SUBSECTION);
  int container_child_space =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL);

  tip_text_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(container_insets),
      container_child_space));
  tip_text_container->SetBackground(views::CreateSolidBackground(
      dialog_view->GetNativeTheme()->ShouldUseDarkColors()
          ? gfx::kGoogleGrey800
          : gfx::kGoogleGrey050));

  // Do not add the border if it is not using dark colors.
  if (!dialog_view->GetNativeTheme()->ShouldUseDarkColors()) {
    constexpr int kTipValuePromptBorderThickness = 1;
    tip_text_container->SetBorder(views::CreateSolidBorder(
        kTipValuePromptBorderThickness, gfx::kGoogleGrey100));
  }

  auto* lightbulb_outline_image = new views::ImageView();
  constexpr int kTipImageSize = 16;
  lightbulb_outline_image->SetImage(gfx::CreateVectorIcon(
      vector_icons::kLightbulbOutlineIcon, kTipImageSize,
      dialog_view->GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_AlertSeverityMedium)));
  lightbulb_outline_image->SetVerticalAlignment(
      views::ImageView::Alignment::kLeading);
  tip_text_container->AddChildView(lightbulb_outline_image);

  auto* tip = new views::Label(tip_message, CONTEXT_DIALOG_BODY_TEXT_SMALL,
                               views::style::STYLE_SECONDARY);
  tip->SetMultiLine(true);
  // If it is in dark mode, set the font color to GG200 since it is on a lighter
  // shade of grey background.
  if (dialog_view->GetNativeTheme()->ShouldUseDarkColors())
    tip->SetEnabledColor(gfx::kGoogleGrey200);
  tip->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  tip->SizeToFit(
      provider->GetDistanceMetric(DISTANCE_LARGE_MODAL_DIALOG_PREFERRED_WIDTH) -
      kMigrationDialogInsets.width() - container_insets.width() -
      kTipImageSize - container_child_space);
  tip_text_container->AddChildView(tip);

  return tip_text_container;
}

// Create the feedback main content view composed of
// title, explanation text, card list, and the tip (if present).
std::unique_ptr<views::View> CreateFeedbackContentView(
    LocalCardMigrationDialogController* controller,
    LocalCardMigrationDialogView* dialog_view) {
  DCHECK(controller->GetViewState() != LocalCardMigrationDialogState::kOffered);

  auto feedback_view = std::make_unique<views::View>();
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  feedback_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  feedback_view->SetBorder(views::CreateEmptyBorder(kMigrationDialogInsets));

  LocalCardMigrationDialogState view_state = controller->GetViewState();
  const std::vector<MigratableCreditCard>& card_list =
      controller->GetCardList();
  const int card_list_size = card_list.size();

  feedback_view->AddChildView(
      CreateExplanationText(view_state, card_list_size, std::u16string())
          .release());

  if (card_list_size > 0) {
    feedback_view->AddChildView(
        CreateCardList(card_list, dialog_view, false).release());

    // If there are no more than two cards in the finished dialog, show the tip.
    constexpr int kShowTipMessageCardNumberLimit = 2;
    if (view_state == LocalCardMigrationDialogState::kFinished &&
        card_list_size <= kShowTipMessageCardNumberLimit) {
      feedback_view->AddChildView(
          CreateTip(controller->GetTipMessage(), dialog_view).release());
    }
  }

  return feedback_view;
}

}  // namespace

// A view composed of the main contents for local card migration dialog
// including title, explanatory message, migratable credit card list,
// horizontal separator, and legal message. It is used by
// LocalCardMigrationDialogView class when it offers the user the
// option to upload all browser-saved credit cards.
class LocalCardMigrationOfferView : public views::View {
 public:
  METADATA_HEADER(LocalCardMigrationOfferView);
  LocalCardMigrationOfferView(LocalCardMigrationDialogController* controller,
                              LocalCardMigrationDialogView* dialog_view)
      : controller_(controller) {
    ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(),
        kMigrationDialogMainContainerChildSpacing));

    auto* contents_container = new views::View();
    contents_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(),
        provider->GetDistanceMetric(
            views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
    // Don't set bottom since there is a legal message view in the offer dialog.
    contents_container->SetBorder(views::CreateEmptyBorder(
        0, kMigrationDialogInsets.left(), 0, kMigrationDialogInsets.right()));

    const std::vector<MigratableCreditCard>& card_list =
        controller->GetCardList();
    int card_list_size = card_list.size();

    contents_container->AddChildView(
        CreateExplanationText(controller_->GetViewState(), card_list_size,
                              base::UTF8ToUTF16(controller_->GetUserEmail()))
            .release());

    std::unique_ptr<views::ScrollView> scroll_view =
        CreateCardList(card_list, dialog_view, card_list_size != 1);
    card_list_view_ = scroll_view->contents();
    contents_container->AddChildView(scroll_view.release());

    AddChildView(contents_container);

    AddChildView(new views::Separator());

    legal_message_container_ = AddChildView(std::make_unique<LegalMessageView>(
        controller->GetLegalMessageLines(),
        base::BindRepeating(
            &LocalCardMigrationDialogController::OnLegalMessageLinkClicked,
            base::Unretained(controller_))));
    legal_message_container_->SetBorder(
        views::CreateEmptyBorder(kMigrationDialogInsets));
  }

  LocalCardMigrationOfferView(const LocalCardMigrationOfferView&) = delete;
  LocalCardMigrationOfferView& operator=(const LocalCardMigrationOfferView&) =
      delete;
  ~LocalCardMigrationOfferView() override = default;

  const std::vector<std::string> GetSelectedCardGuids() const {
    std::vector<std::string> selected_cards;
    for (views::View* child : card_list_view_->children()) {
      DCHECK(views::IsViewClass<MigratableCardView>(child));
      auto* card = static_cast<MigratableCardView*>(child);
      if (card->GetSelected())
        selected_cards.push_back(card->GetGuid());
    }
    return selected_cards;
  }

 private:
  friend class LocalCardMigrationDialogView;

  LocalCardMigrationDialogController* controller_;

  views::View* card_list_view_ = nullptr;

  // The view that contains legal message and handles legal message links
  // clicking.
  LegalMessageView* legal_message_container_ = nullptr;
};

BEGIN_METADATA(LocalCardMigrationOfferView, views::View)
ADD_READONLY_PROPERTY_METADATA(std::vector<std::string>, SelectedCardGuids)
END_METADATA

LocalCardMigrationDialogView::LocalCardMigrationDialogView(
    LocalCardMigrationDialogController* controller,
    content::WebContents* web_contents)
    : controller_(controller), web_contents_(web_contents) {
  SetButtons(controller_->AllCardsInvalid()
                 ? ui::DIALOG_BUTTON_OK
                 : ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
  SetButtonLabel(ui::DIALOG_BUTTON_OK, GetOkButtonLabel());
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, GetCancelButtonLabel());
  SetCancelCallback(
      base::BindOnce(&LocalCardMigrationDialogView::OnDialogCancelled,
                     base::Unretained(this)));
  SetAcceptCallback(base::BindOnce(
      &LocalCardMigrationDialogView::OnDialogAccepted, base::Unretained(this)));
  RegisterWindowClosingCallback(base::BindOnce(
      &LocalCardMigrationDialogView::OnWindowClosing, base::Unretained(this)));
  // This should be a modal dialog blocking the browser since we don't want
  // users to lose progress in the migration workflow until they are done.
  SetModalType(ui::MODAL_TYPE_WINDOW);
  set_close_on_deactivate(false);
  set_margins(gfx::Insets());
  set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_LARGE_MODAL_DIALOG_PREFERRED_WIDTH));
  SetShowCloseButton(false);
}

LocalCardMigrationDialogView::~LocalCardMigrationDialogView() {}

void LocalCardMigrationDialogView::ShowDialog() {
  ConstructView();
  constrained_window::CreateBrowserModalDialogViews(
      this, web_contents_->GetTopLevelNativeWindow())
      ->Show();
}

void LocalCardMigrationDialogView::CloseDialog() {
  controller_ = nullptr;
  GetWidget()->Close();
}

void LocalCardMigrationDialogView::OnDialogAccepted() {
  switch (controller_->GetViewState()) {
    case LocalCardMigrationDialogState::kOffered:
      DCHECK(offer_view_);
      controller_->OnSaveButtonClicked(offer_view_->GetSelectedCardGuids());
      break;
    case LocalCardMigrationDialogState::kFinished:
    case LocalCardMigrationDialogState::kActionRequired:
      controller_->OnDoneButtonClicked();
      break;
  }
}

void LocalCardMigrationDialogView::OnDialogCancelled() {
  switch (controller_->GetViewState()) {
    case LocalCardMigrationDialogState::kOffered:
      controller_->OnCancelButtonClicked();
      break;
    case LocalCardMigrationDialogState::kFinished:
    case LocalCardMigrationDialogState::kActionRequired:
      controller_->OnViewCardsButtonClicked();
      break;
  }
}

void LocalCardMigrationDialogView::OnWindowClosing() {
  if (controller_) {
    controller_->OnDialogClosed();
    controller_ = nullptr;
  }
}

bool LocalCardMigrationDialogView::GetEnableOkButton() const {
  if (controller_->GetViewState() == LocalCardMigrationDialogState::kOffered) {
    DCHECK(offer_view_) << "This method can't be called before ConstructView";
    return !offer_view_->GetSelectedCardGuids().empty();
  }
  return true;
}

void LocalCardMigrationDialogView::DeleteCard(const std::string& guid) {
  controller_->DeleteCard(guid);
  ConstructView();
  UpdateLayout();
}

void LocalCardMigrationDialogView::OnCardCheckboxToggled() {
  SetButtonEnabled(ui::DIALOG_BUTTON_OK, GetEnableOkButton());
}

// TODO(crbug.com/913571): Figure out a way to avoid two consecutive layouts.
void LocalCardMigrationDialogView::UpdateLayout() {
  Layout();
  // Since the dialog does not have anchor view or arrow, cannot use
  // SizeToContents() for now. TODO(crbug.com/867194): Try to fix the
  // BubbleDialogDelegateView::GetBubbleBounds() when there is no anchor
  // view or arrow.
  GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
}

void LocalCardMigrationDialogView::ConstructView() {
  DCHECK(controller_->GetViewState() !=
             LocalCardMigrationDialogState::kOffered ||
         children().empty());

  RemoveAllChildViews(/*delete_children=*/true);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kMigrationDialogMainContainerChildSpacing));

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  auto* image = new views::ImageView();
  constexpr int kImageBorderBottom = 8;
  image->SetBorder(views::CreateEmptyBorder(0, 0, kImageBorderBottom, 0));
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  image->SetImage(
      rb.GetImageSkiaNamed(GetNativeTheme()->ShouldUseDarkColors()
                               ? IDR_AUTOFILL_MIGRATION_DIALOG_HEADER_DARK
                               : IDR_AUTOFILL_MIGRATION_DIALOG_HEADER));
  image->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_GOOGLE_PAY_LOGO_ACCESSIBLE_NAME));
  AddChildView(image);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  LocalCardMigrationDialogState view_state = controller_->GetViewState();
  AddChildView(CreateTitle(view_state, this, controller_->GetCardList().size())
                   .release());

  if (view_state == LocalCardMigrationDialogState::kOffered) {
    offer_view_ = new LocalCardMigrationOfferView(controller_, this);
    offer_view_->SetID(DialogViewId::MAIN_CONTENT_VIEW_MIGRATION_OFFER_DIALOG);
    card_list_view_ = offer_view_->card_list_view_;
    AddChildView(offer_view_);
    SetButtonEnabled(ui::DIALOG_BUTTON_OK, GetEnableOkButton());
  } else {
    AddChildView(CreateFeedbackContentView(controller_, this).release());
  }
}

std::u16string LocalCardMigrationDialogView::GetOkButtonLabel() const {
  switch (controller_->GetViewState()) {
    case LocalCardMigrationDialogState::kOffered:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_BUTTON_LABEL_SAVE);
    case LocalCardMigrationDialogState::kFinished:
    case LocalCardMigrationDialogState::kActionRequired:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_BUTTON_LABEL_DONE);
  }
}

std::u16string LocalCardMigrationDialogView::GetCancelButtonLabel() const {
  switch (controller_->GetViewState()) {
    case LocalCardMigrationDialogState::kOffered:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_BUTTON_LABEL_CANCEL);
    case LocalCardMigrationDialogState::kFinished:
    case LocalCardMigrationDialogState::kActionRequired:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_BUTTON_LABEL_VIEW_CARDS);
  }
}

LocalCardMigrationDialog* CreateLocalCardMigrationDialogView(
    LocalCardMigrationDialogController* controller,
    content::WebContents* web_contents) {
  return new LocalCardMigrationDialogView(controller, web_contents);
}

BEGIN_METADATA(LocalCardMigrationDialogView, views::BubbleDialogDelegateView)
ADD_READONLY_PROPERTY_METADATA(bool, EnableOkButton)
ADD_READONLY_PROPERTY_METADATA(std::u16string, OkButtonLabel)
ADD_READONLY_PROPERTY_METADATA(std::u16string, CancelButtonLabel)
END_METADATA

}  // namespace autofill
