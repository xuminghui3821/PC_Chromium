// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_SERVER_WIN_SERVER_H_
#define CHROME_UPDATER_APP_SERVER_WIN_SERVER_H_

#include <windows.h>

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/app/app_server.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_service_internal.h"

namespace updater {

class Configurator;

// The COM objects involved in this server are free threaded. Incoming COM calls
// arrive on COM RPC threads. Outgoing COM calls are posted from a blocking
// sequenced task runner in the thread pool. Calls to the services hosted
// in this server occur in the main sequence, which is bound to the main
// thread of the process.
//
// If such a COM object has state which is visible to multiple threads, then the
// access to the shared state of the object must be synchronized. This is done
// by using a lock, internal to the object. Since the code running on the
// main sequence can't use synchronization primitives, another task runner is
// typically used to sequence the callbacks.
//
// This class is responsible for the lifetime of the COM server, as well as
// class factory registration.
//
// The instance of the this class is managed by a singleton and it leaks at
// runtime.
class ComServerApp : public AppServer {
 public:
  ComServerApp();

  scoped_refptr<base::SequencedTaskRunner> main_task_runner() {
    return main_task_runner_;
  }
  scoped_refptr<UpdateService> update_service() {
    CHECK(update_service_);
    return update_service_;
  }
  scoped_refptr<UpdateServiceInternal> update_service_internal() {
    CHECK(update_service_internal_);
    return update_service_internal_;
  }

 private:
  ~ComServerApp() override;

  // Overrides for App.
  void InitializeThreadPool() override;

  // Overrides for AppServer
  void ActiveDuty(scoped_refptr<UpdateService> update_service) override;
  void ActiveDutyInternal(
      scoped_refptr<UpdateServiceInternal> update_service_internal) override;
  bool SwapRPCInterfaces() override;
  void UninstallSelf() override;

  // Registers and unregisters the out-of-process COM class factories.
  HRESULT RegisterClassObjects();
  HRESULT RegisterInternalClassObjects();
  void UnregisterClassObjects();

  // Waits until the last COM object is released.
  void WaitForExitSignal();

  // Called when the last object is released.
  void SignalExit();

  // Creates an out-of-process WRL Module.
  void CreateWRLModule();

  // Handles COM setup and registration.
  void Start(base::OnceCallback<HRESULT()> register_callback);

  // Handles object unregistration then triggers program shutdown. This
  // function runs on a COM RPC thread when the WRL module is destroyed.
  void Stop();

  // Identifier of registered class objects used for unregistration.
  std::vector<DWORD> cookies_;

  // While this object lives, COM can be used by all threads in the program.
  base::win::ScopedCOMInitializer com_initializer_;

  // Task runner bound to the main sequence.
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // These services run the in-process code, which is delegating to the
  // |update_client| component.
  scoped_refptr<UpdateService> update_service_;
  scoped_refptr<UpdateServiceInternal> update_service_internal_;
};

// Returns a singleton application object bound to this COM server.
scoped_refptr<ComServerApp> AppServerSingletonInstance();

}  // namespace updater

#endif  // CHROME_UPDATER_APP_SERVER_WIN_SERVER_H_
