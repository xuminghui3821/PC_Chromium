// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_DEVTOOLS_LISTENER_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_DEVTOOLS_LISTENER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/values.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"

namespace file_manager {

// Collects code coverage from a WebContents, during a browser test
// for example, using Chrome Devtools Protocol (CDP).
class DevToolsListener : public content::DevToolsAgentHostClient {
 public:
  // Attaches to host and enables CDP.
  DevToolsListener(content::DevToolsAgentHost* host, uint32_t uuid);
  ~DevToolsListener() override;

  // Host navigation starts code coverage.
  void Navigated(content::DevToolsAgentHost* host);

  // Returns true if host has started code coverage.
  bool HasCoverage(content::DevToolsAgentHost* host);

  // If host HasCoverage(), collect it and save it in |store|.
  void GetCoverage(content::DevToolsAgentHost* host,
                   const base::FilePath& store,
                   const std::string& test);

  // Detaches from host.
  void Detach(content::DevToolsAgentHost* host);

  // Returns a unique host identifier, with optional |prefix|.
  static std::string HostString(content::DevToolsAgentHost* host,
                                const std::string& prefix = {});

  // Creates coverage output directory and subdirectories.
  static void SetupCoverageStore(const base::FilePath& store_path);

 private:
  // Starts CDP session on host.
  void Start(content::DevToolsAgentHost* host);

  // Starts JavaScript (JS) code coverage on host.
  bool StartJSCoverage(content::DevToolsAgentHost* host);

  // Collects JavaScript coverage from host and saves it in |store|.
  void StopAndStoreJSCoverage(content::DevToolsAgentHost* host,
                              const base::FilePath& store,
                              const std::string& test);

  // Stores JS scripts used during code execution on host.
  void StoreScripts(content::DevToolsAgentHost* host,
                    const base::FilePath& store);

  // Sends CDP commands to host.
  void SendCommandMessage(content::DevToolsAgentHost* host,
                          const std::string& command);

  // Awaits CDP response to command |id|.
  void AwaitCommandResponse(int id);

  // Receives CDP messages from host.
  void DispatchProtocolMessage(content::DevToolsAgentHost* host,
                               base::span<const uint8_t> message) override;

  // Returns true if URL should be attached to.
  bool MayAttachToURL(const GURL& url, bool is_webui) override;

  // Called if host was shut down (closed).
  void AgentHostClosed(content::DevToolsAgentHost* host) override;

 private:
  std::vector<std::unique_ptr<base::DictionaryValue>> script_;
  std::unique_ptr<base::DictionaryValue> script_coverage_;
  std::map<std::string, std::string> script_hash_map_;
  std::map<std::string, std::string> script_id_map_;

  base::OnceClosure value_closure_;
  std::unique_ptr<base::DictionaryValue> value_;
  int value_id_ = 0;

  const std::string uuid_;
  bool navigated_ = false;
  bool attached_ = true;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_DEVTOOLS_LISTENER_H_
