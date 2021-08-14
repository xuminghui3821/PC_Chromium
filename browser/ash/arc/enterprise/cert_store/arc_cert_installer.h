// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ENTERPRISE_CERT_STORE_ARC_CERT_INSTALLER_H_
#define CHROME_BROWSER_ASH_ARC_ENTERPRISE_CERT_STORE_ARC_CERT_INSTALLER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/core/common/remote_commands/remote_commands_queue.h"
#include "crypto/rsa_private_key.h"
#include "net/cert/scoped_nss_types.h"

class Profile;

namespace content {

class BrowserContext;

}  // namespace content

namespace arc {

struct CertDescription {
  CertDescription(crypto::RSAPrivateKey* placeholder_key,
                  CERTCertificate* nss_cert);
  CertDescription(CertDescription&& other);
  CertDescription(const CertDescription& other) = delete;
  CertDescription& operator=(CertDescription&& other);
  CertDescription& operator=(const CertDescription& other) = delete;
  ~CertDescription();

  std::unique_ptr<crypto::RSAPrivateKey> placeholder_key;
  net::ScopedCERTCertificate nss_cert;
};

// This class manages the certificates, available to ARC.
// It keeps track of the certificates and installs missing ones via
// ARC remote commands.
class ArcCertInstaller : public policy::RemoteCommandsQueue::Observer {
 public:
  explicit ArcCertInstaller(content::BrowserContext* context);

  // This constructor should be used only for testing.
  ArcCertInstaller(Profile* profile,
                   std::unique_ptr<policy::RemoteCommandsQueue> queue);
  ~ArcCertInstaller() override;

  using InstallArcCertsCallback = base::OnceCallback<void(bool result)>;

  // Install missing certificates via ARC remote commands.
  //
  // Return map of certificate names required being installed on ARC to dummy
  // SPKI.
  // The dummy SPKI may be empty if the key is not installed during this call
  // (either error or already installed key pair).
  // Return false via |callback| in case of any error, and true otherwise.
  // Made virtual for override in test.
  virtual std::map<std::string, std::string> InstallArcCerts(
      std::vector<CertDescription> certificates,
      InstallArcCertsCallback callback);

 private:
  // Install ARC certificate if not installed yet.
  // Return RSA public key material for the NSS cert encoded in base 64
  // or an empty string if the key is not installed during this call
  // (either error or already installed key pair).
  std::string InstallArcCert(const std::string& name,
                             const CertDescription& certificate);

  // RemoteCommandsQueue::Observer overrides:
  void OnJobStarted(policy::RemoteCommandJob* command) override {}
  void OnJobFinished(policy::RemoteCommandJob* command) override;

  Profile* profile_;  // not owned

  // A valid callback when the caller of |InstallArcCerts| method is awaiting
  // for a response.
  InstallArcCertsCallback callback_;

  // Status of a pending certificate installation query.
  // True by default.
  // False if the installation failed.
  // The |pending_status_| is returned via |callback_|.
  bool pending_status_ = true;

  // Names of certificates installed or pending to be installed on ARC.
  std::set<std::string> known_cert_names_;

  // Map from unique_id of the remote command to the corresponding cert name.
  std::map<int, std::string> pending_commands_;

  // Remote commands queue.
  std::unique_ptr<policy::RemoteCommandsQueue> queue_;

  // The next remote command unique id. Should be increased after every usage.
  int next_id_ = 1;

  base::WeakPtrFactory<ArcCertInstaller> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcCertInstaller);
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ENTERPRISE_CERT_STORE_ARC_CERT_INSTALLER_H_
