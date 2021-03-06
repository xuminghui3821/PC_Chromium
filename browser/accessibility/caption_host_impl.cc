// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/caption_host_impl.h"

#include <memory>
#include <utility>

#include "chrome/browser/accessibility/caption_controller.h"
#include "chrome/browser/accessibility/caption_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace captions {

// static
void CaptionHostImpl::Create(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<chrome::mojom::CaptionHost> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<CaptionHostImpl>(frame_host),
                              std::move(receiver));
}

CaptionHostImpl::CaptionHostImpl(content::RenderFrameHost* frame_host)
    : frame_host_(frame_host) {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return;
  Observe(web_contents);
}

CaptionHostImpl::~CaptionHostImpl() {
  CaptionController* caption_controller = GetCaptionController();
  if (caption_controller)
    caption_controller->OnAudioStreamEnd(this);
}

void CaptionHostImpl::OnTranscription(
    chrome::mojom::TranscriptionResultPtr transcription_result,
    OnTranscriptionCallback reply) {
  CaptionController* caption_controller = GetCaptionController();
  if (!caption_controller) {
    std::move(reply).Run(false);
    return;
  }
  std::move(reply).Run(
      caption_controller->DispatchTranscription(this, transcription_result));
}

void CaptionHostImpl::OnLanguageIdentificationEvent(
    media::mojom::LanguageIdentificationEventPtr event) {
  CaptionController* caption_controller = GetCaptionController();
  if (!caption_controller)
    return;

  caption_controller->OnLanguageIdentificationEvent(std::move(event));
}

void CaptionHostImpl::OnError() {
  CaptionController* caption_controller = GetCaptionController();
  if (caption_controller)
    caption_controller->OnError(this);
}

void CaptionHostImpl::RenderFrameDeleted(content::RenderFrameHost* frame_host) {
  if (frame_host == frame_host_)
    frame_host_ = nullptr;
}

content::WebContents* CaptionHostImpl::GetWebContents() {
  if (!frame_host_)
    return nullptr;
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame_host_);
  if (!web_contents)
    frame_host_ = nullptr;
  return web_contents;
}

CaptionController* CaptionHostImpl::GetCaptionController() {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return nullptr;
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile)
    return nullptr;
  return CaptionControllerFactory::GetForProfile(profile);
}

}  // namespace captions
