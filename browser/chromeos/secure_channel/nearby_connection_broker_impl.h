// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SECURE_CHANNEL_NEARBY_CONNECTION_BROKER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_SECURE_CHANNEL_NEARBY_CONNECTION_BROKER_IMPL_H_

#include <memory>
#include <ostream>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/secure_channel/nearby_connection_broker.h"
#include "chrome/browser/chromeos/secure_channel/util/histogram_util.h"
#include "chromeos/services/nearby/public/mojom/nearby_connections.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

namespace chromeos {
namespace secure_channel {

class NearbyEndpointFinder;

// NearbyConnectionBroker implementation which utilizes NearbyEndpointFinder to
// find an endpoint, then uses Nearby Connections to create and maintain a
// connection. The overall process consists of:
//   (1) Finding an endpoint via NearbyEndpointFinder.
//   (2) Requesting a connection using that endpoint.
//   (3) Accepting a connection.
//   (4) Exchanging messages over the connection.
//
// Deleting an instance of this class tears down any active connection and
// performs cleanup if necessary.
class NearbyConnectionBrokerImpl
    : public NearbyConnectionBroker,
      public location::nearby::connections::mojom::ConnectionLifecycleListener,
      public location::nearby::connections::mojom::PayloadListener {
 public:
  class Factory {
   public:
    static std::unique_ptr<NearbyConnectionBroker> Create(
        const std::vector<uint8_t>& bluetooth_public_address,
        NearbyEndpointFinder* endpoint_finder,
        mojo::PendingReceiver<mojom::NearbyMessageSender>
            message_sender_receiver,
        mojo::PendingRemote<mojom::NearbyMessageReceiver>
            message_receiver_remote,
        const mojo::SharedRemote<
            location::nearby::connections::mojom::NearbyConnections>&
            nearby_connections,
        base::OnceClosure on_connected_callback,
        base::OnceClosure on_disconnected_callback,
        std::unique_ptr<base::OneShotTimer> timer =
            std::make_unique<base::OneShotTimer>());
    static void SetFactoryForTesting(Factory* test_factory);

    virtual ~Factory() = default;

   protected:
    virtual std::unique_ptr<NearbyConnectionBroker> CreateInstance(
        const std::vector<uint8_t>& bluetooth_public_address,
        NearbyEndpointFinder* endpoint_finder,
        mojo::PendingReceiver<mojom::NearbyMessageSender>
            message_sender_receiver,
        mojo::PendingRemote<mojom::NearbyMessageReceiver>
            message_receiver_remote,
        const mojo::SharedRemote<
            location::nearby::connections::mojom::NearbyConnections>&
            nearby_connections,
        base::OnceClosure on_connected_callback,
        base::OnceClosure on_disconnected_callback,
        std::unique_ptr<base::OneShotTimer> timer) = 0;
  };

  ~NearbyConnectionBrokerImpl() override;

 private:
  enum class ConnectionStatus {
    kUninitialized,
    kDiscoveringEndpoint,
    kRequestingConnection,
    kAcceptingConnection,
    kWaitingForConnectionToBeAcceptedByRemoteDevice,
    kConnected,
    kDisconnecting,
    kDisconnected,
  };
  friend std::ostream& operator<<(
      std::ostream& stream,
      NearbyConnectionBrokerImpl::ConnectionStatus status);

  NearbyConnectionBrokerImpl(
      const std::vector<uint8_t>& bluetooth_public_address,
      NearbyEndpointFinder* endpoint_finder,
      mojo::PendingReceiver<mojom::NearbyMessageSender> message_sender_receiver,
      mojo::PendingRemote<mojom::NearbyMessageReceiver> message_receiver_remote,
      const mojo::SharedRemote<
          location::nearby::connections::mojom::NearbyConnections>&
          nearby_connections,
      base::OnceClosure on_connected_callback,
      base::OnceClosure on_disconnected_callback,
      std::unique_ptr<base::OneShotTimer> timer);

  void TransitionToStatus(ConnectionStatus connection_status);
  void Disconnect(util::NearbyDisconnectionReason reason);
  void TransitionToDisconnectedAndInvokeCallback();

  void OnEndpointDiscovered(
      const std::string& endpoint_id,
      location::nearby::connections::mojom::DiscoveredEndpointInfoPtr info);
  void OnDiscoveryFailure();

  void OnRequestConnectionResult(
      location::nearby::connections::mojom::Status status);
  void OnAcceptConnectionResult(
      location::nearby::connections::mojom::Status status);
  void OnSendPayloadResult(SendMessageCallback callback,
                           location::nearby::connections::mojom::Status status);
  void OnDisconnectFromEndpointResult(
      location::nearby::connections::mojom::Status status);
  void OnConnectionStatusChangeTimeout();

  // NearbyConnectionBroker:
  void OnMojoDisconnection() override;

  // mojom::NearbyMessageSender:
  void SendMessage(const std::string& message,
                   SendMessageCallback callback) override;

  // location::nearby::connections::mojom::ConnectionLifecycleListener:
  void OnConnectionInitiated(
      const std::string& endpoint_id,
      location::nearby::connections::mojom::ConnectionInfoPtr info) override;
  void OnConnectionAccepted(const std::string& endpoint_id) override;
  void OnConnectionRejected(
      const std::string& endpoint_id,
      location::nearby::connections::mojom::Status status) override;
  void OnDisconnected(const std::string& endpoint_id) override;
  void OnBandwidthChanged(
      const std::string& endpoint_id,
      location::nearby::connections::mojom::Medium medium) override;

  // location::nearby::connections::mojom::PayloadListener:
  void OnPayloadReceived(
      const std::string& endpoint_id,
      location::nearby::connections::mojom::PayloadPtr payload) override;
  // Note: Intentionally left empty; SecureChannel messages are always sent as
  // bytes and do not require transfer updates.
  void OnPayloadTransferUpdate(
      const std::string& endpoint_id,
      location::nearby::connections::mojom::PayloadTransferUpdatePtr update)
      override {}

  NearbyEndpointFinder* endpoint_finder_;
  mojo::SharedRemote<location::nearby::connections::mojom::NearbyConnections>
      nearby_connections_;
  std::unique_ptr<base::OneShotTimer> timer_;

  mojo::Receiver<
      location::nearby::connections::mojom::ConnectionLifecycleListener>
      connection_lifecycle_listener_receiver_{this};
  mojo::Receiver<location::nearby::connections::mojom::PayloadListener>
      payload_listener_receiver_{this};

  ConnectionStatus connection_status_ = ConnectionStatus::kUninitialized;

  // Starts empty, then set in OnEndpointDiscovered().
  std::string remote_endpoint_id_;

  // Starts as false and changes to true when WebRTC upgrade occurs.
  bool has_upgraded_to_webrtc_ = false;

  // Whether or not a metric has been logged to note that a metric has been
  // logged indicated that Disconnect() was called before a WebRTC upgrade
  // occurred.
  bool has_recorded_no_webrtc_metric_ = false;

  // Starts as false; set to true in OnConnectionInitiated() and back to false
  // in OnDisconnected().
  bool need_to_disconnect_endpoint_ = false;

  // Starts as null; set in OnConnectionAccepted().
  base::Time time_when_connection_accepted_;

  bool has_disconnect_reason_been_logged_ = false;

  base::WeakPtrFactory<NearbyConnectionBrokerImpl> weak_ptr_factory_{this};
};

std::ostream& operator<<(std::ostream& stream,
                         NearbyConnectionBrokerImpl::ConnectionStatus status);

}  // namespace secure_channel
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SECURE_CHANNEL_NEARBY_CONNECTION_BROKER_IMPL_H_
