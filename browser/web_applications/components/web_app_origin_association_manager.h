// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_ORIGIN_ASSOCIATION_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_ORIGIN_ASSOCIATION_MANAGER_H_

#include <deque>
#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "components/webapps/services/web_app_origin_association/public/mojom/web_app_origin_association_parser.mojom.h"
#include "components/webapps/services/web_app_origin_association/web_app_origin_association_fetcher.h"
#include "mojo/public/cpp/bindings/remote.h"

class GURL;

namespace web_app {

// Callback type that sends back the valid |url_handlers|.
using OnDidGetWebAppOriginAssociations =
    base::OnceCallback<void(apps::UrlHandlers url_handlers)>;

// Fetch, parse, and validate web app origin association files.
class WebAppOriginAssociationManager {
 public:
  // Does the fetching, parsing, and validation work for a batch of url
  // handlers.
  class Task;

  WebAppOriginAssociationManager();
  WebAppOriginAssociationManager(const WebAppOriginAssociationManager&) =
      delete;
  WebAppOriginAssociationManager& operator=(
      const WebAppOriginAssociationManager&) = delete;
  virtual ~WebAppOriginAssociationManager();

  virtual void GetWebAppOriginAssociations(
      const GURL& manifest_url,
      apps::UrlHandlers url_handlers,
      OnDidGetWebAppOriginAssociations callback);
  void SetFetcherForTest(
      std::unique_ptr<webapps::WebAppOriginAssociationFetcher> fetcher);

 private:
  FRIEND_TEST_ALL_PREFIXES(WebAppOriginAssociationManagerTest, RunTasks);

  const mojo::Remote<webapps::mojom::WebAppOriginAssociationParser>&
  GetParser();
  webapps::WebAppOriginAssociationFetcher& GetFetcher();
  void MaybeStartNextTask();
  void OnTaskCompleted();

  std::deque<std::unique_ptr<Task>> pending_tasks_;
  bool task_in_progress_ = false;

  mojo::Remote<webapps::mojom::WebAppOriginAssociationParser> parser_;
  std::unique_ptr<webapps::WebAppOriginAssociationFetcher> fetcher_;
  base::WeakPtrFactory<WebAppOriginAssociationManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_ORIGIN_ASSOCIATION_MANAGER_H_
