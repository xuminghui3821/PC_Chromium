// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_WEBRTC_SIGNALING_MESSENGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_WEBRTC_SIGNALING_MESSENGER_H_

#include <string>

#include "base/callback.h"
#include "chrome/browser/nearby_sharing/instantmessaging/receive_messages_express.h"
#include "chrome/browser/nearby_sharing/instantmessaging/send_message_express.h"
#include "chrome/browser/nearby_sharing/instantmessaging/token_fetcher.h"
#include "chromeos/services/nearby/public/mojom/webrtc_signaling_messenger.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace signin {
class IdentityManager;
}  // namespace signin

class WebRtcSignalingMessenger
    : public sharing::mojom::WebRtcSignalingMessenger {
 public:
  WebRtcSignalingMessenger(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~WebRtcSignalingMessenger() override;

  // sharing::mojom::WebRtcSignalingMessenger:
  void SendMessage(const std::string& self_id,
                   const std::string& peer_id,
                   sharing::mojom::LocationHintPtr location_hint,
                   const std::string& message,
                   SendMessageCallback callback) override;
  void StartReceivingMessages(
      const std::string& self_id,
      sharing::mojom::LocationHintPtr location_hint,
      mojo::PendingRemote<sharing::mojom::IncomingMessagesListener>
          incoming_messages_listener,
      StartReceivingMessagesCallback callback) override;

 private:
  signin::IdentityManager* identity_manager_ = nullptr;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_WEBRTC_SIGNALING_MESSENGER_H_
