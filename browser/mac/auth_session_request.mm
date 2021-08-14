// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/auth_session_request.h"

#import <AuthenticationServices/AuthenticationServices.h>
#import <Foundation/Foundation.h>

#include <memory>
#include <string>

#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/mac/url_conversions.h"

namespace {

// A navigation throttle that calls a closure when a navigation to a specified
// scheme is seen.
class AuthNavigationThrottle : public content::NavigationThrottle {
 public:
  using SchemeURLFoundCallback = base::OnceCallback<void(const GURL&)>;

  AuthNavigationThrottle(content::NavigationHandle* handle,
                         const std::string& scheme,
                         SchemeURLFoundCallback scheme_found)
      : content::NavigationThrottle(handle),
        scheme_(scheme),
        scheme_found_(std::move(scheme_found)) {
    DCHECK(!scheme_found_.is_null());
  }
  ~AuthNavigationThrottle() override = default;

  ThrottleCheckResult WillStartRequest() override { return HandleRequest(); }

  ThrottleCheckResult WillRedirectRequest() override { return HandleRequest(); }

  const char* GetNameForLogging() override { return "AuthNavigationThrottle"; }

 private:
  ThrottleCheckResult HandleRequest() {
    GURL url = navigation_handle()->GetURL();
    if (!url.SchemeIs(scheme_))
      return PROCEED;

    // Paranoia; if the callback was already fired, ignore all further
    // navigations that somehow get through before the WebContents deletion
    // happens.
    if (scheme_found_.is_null())
      return CANCEL_AND_IGNORE;

    // Post the callback; triggering the deletion of the WebContents that owns
    // the navigation that is in the middle of being throttled would likely not
    // be the best of ideas.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(scheme_found_), url));

    return CANCEL_AND_IGNORE;
  }

  // The scheme to watch for.
  std::string scheme_;

  // The closure to call once the scheme has been seen.
  SchemeURLFoundCallback scheme_found_;
};

}  // namespace

AuthSessionRequest::~AuthSessionRequest() {
  std::string uuid = base::SysNSStringToUTF8(request_.get().UUID.UUIDString);

  auto iter = GetMap().find(uuid);
  if (iter == GetMap().end())
    return;

  GetMap().erase(iter);
}

// static
void AuthSessionRequest::StartNewAuthSession(
    ASWebAuthenticationSessionRequest* request,
    Profile* profile) {
  // Create a Browser with an empty tab.
  Browser* browser = CreateBrowser(request, profile);
  if (!browser) {
    // It's not clear what error to return here. -cancelWithError:'s
    // documentation says that it has to be an NSError with the domain as
    // specified below and a "suitable" ASWebAuthenticationSessionErrorCode, but
    // none of those codes really is good for "something went wrong while trying
    // to start the authentication session". PresentationContextInvalid will
    // have to do.
    NSError* error = [NSError
        errorWithDomain:ASWebAuthenticationSessionErrorDomain
                   code:
                       ASWebAuthenticationSessionErrorCodePresentationContextInvalid
               userInfo:@{
                 NSDebugDescriptionErrorKey :
                     @"Failed to create a WebContents to present the "
                     @"authorization session."
               }];
    [request cancelWithError:error];
    return;
  }

  // Then create the auth session that owns that browser and will intercept
  // navigation requests.
  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();
  AuthSessionRequest::CreateForWebContents(contents, browser, request);

  // Only then actually load the requested page, to make sure that if the very
  // first navigation is the one that authorizes the login, it's caught.
  // https://crbug.com/1195202
  contents->GetController().LoadURL(net::GURLWithNSURL(request.URL),
                                    content::Referrer(),
                                    ui::PAGE_TRANSITION_LINK, std::string());
}

// static
void AuthSessionRequest::CancelAuthSession(
    ASWebAuthenticationSessionRequest* request) {
  std::string uuid = base::SysNSStringToUTF8(request.UUID.UUIDString);

  auto iter = GetMap().find(uuid);
  if (iter == GetMap().end())
    return;

  iter->second->CancelAuthSession();
}

std::unique_ptr<content::NavigationThrottle> AuthSessionRequest::CreateThrottle(
    content::NavigationHandle* handle) {
  if (!handle->IsInMainFrame())
    return nil;

  std::string scheme =
      base::SysNSStringToUTF8(request_.get().callbackURLScheme);

  // base::Unretained is safe because throttles are owned by the
  // NavigationRequest, which won't outlive the WebContents, whose lifetime this
  // is tied to.
  auto scheme_found = base::BindOnce(&AuthSessionRequest::SchemeWasNavigatedTo,
                                     base::Unretained(this));

  return std::make_unique<AuthNavigationThrottle>(handle, scheme,
                                                  std::move(scheme_found));
}

AuthSessionRequest::AuthSessionRequest(
    content::WebContents* web_contents,
    Browser* browser,
    ASWebAuthenticationSessionRequest* request)
    : content::WebContentsObserver(web_contents),
      browser_(browser),
      request_(request, base::scoped_policy::RETAIN) {
  std::string uuid = base::SysNSStringToUTF8(request.UUID.UUIDString);
  GetMap()[uuid] = this;
}

// static
Browser* AuthSessionRequest::CreateBrowser(
    ASWebAuthenticationSessionRequest* request,
    Profile* profile) {
  if (!profile)
    return nullptr;

  if (request.shouldUseEphemeralSession)
    profile = profile->GetPrimaryOTRProfile();
  if (!profile)
    return nullptr;

  // Note that this creates a popup-style window to do the signin. This is a
  // specific choice motivated by security concerns, and must *not* be changed
  // without consultation with the security team.
  //
  // The UX concern here is that an ordinary tab is not the right tool. This is
  // a magical WebContents that will dismiss itself when a valid login happens
  // within it, and so an ordinary tab can't be used as it invites a user to
  // navigate by putting a new URL or search into the omnibox. The location
  // information must be read-only.
  //
  // But the critical security concern is that the window *must have* a location
  // indication. This is an OS API for which UI needs to be created to allow the
  // user to log into a website by providing credentials. Chromium must provide
  // the user with an indication of where they are using the credentials.
  //
  // Having a location indicator that is present but read-only is satisfied with
  // a popup window. That must not be changed.

  Browser* browser = Browser::Create(
      Browser::CreateParams(Browser::TYPE_POPUP, profile, true));
  chrome::AddTabAt(browser, GURL("about:blank"), -1, true);
  browser->window()->Show();

  return browser;
}

// static
AuthSessionRequest::UUIDToSessionRequestMap& AuthSessionRequest::GetMap() {
  static base::NoDestructor<UUIDToSessionRequestMap> map;
  return *map;
}

void AuthSessionRequest::DestroyWebContents() {
  // Detach the WebContents that owns this object from the tab strip. Because
  // the Browser is a TYPE_POPUP, there will only be one tab (tab index 0). This
  // will cause the browser window to dispose of itself once it realizes that it
  // has no tabs left. Close the tab this way (as opposed to, say,
  // TabStripModel::CloseWebContentsAt) so that the web page will no longer be
  // able to show any dialogs, particularly a `beforeunload` one.
  std::unique_ptr<content::WebContents> this_contents =
      browser_->tab_strip_model()->DetachWebContentsAt(0);
  // Leaving this function will cause the destruction of the WebContents,
  // triggering a call to WebContentsDestroyed() below.
}

void AuthSessionRequest::CancelAuthSession() {
  // macOS has requested that this authentication session be canceled. Close the
  // browser window and call it a day.

  perform_cancellation_callback_ = false;

  DestroyWebContents();
  // `DestroyWebContents` triggered the death of this object; perform no more
  // work.
}

void AuthSessionRequest::SchemeWasNavigatedTo(const GURL& url) {
  perform_cancellation_callback_ = false;

  [request_ completeWithCallbackURL:net::NSURLWithGURL(url)];

  DestroyWebContents();
  // `DestroyWebContents` triggered the death of this object; perform no more
  // work.
}

void AuthSessionRequest::WebContentsDestroyed() {
  // This function can be called through one of three code paths:
  //
  // 1. The user closed the window, in which case the "user canceled" callback
  //    must be made.
  // 2. The user successfully logged in, in which case the closure of the page
  //    was triggered above in SchemeWasNavigatedTo().
  // 3. The OS asked for cancellation, in which case the closure of the page was
  //    triggered above in CancelAuthSession().
  //
  // In case 2, the success callback was already made; in case 3, no callback
  // should be made. `perform_cancellation_callback_` is set to false in those
  // cases. If `perform_cancellation_callback_` is true, then it was never
  // changed after initialization, and that distinguishes case 1.

  if (perform_cancellation_callback_) {
    NSError* error = [NSError
        errorWithDomain:ASWebAuthenticationSessionErrorDomain
                   code:ASWebAuthenticationSessionErrorCodeCanceledLogin
               userInfo:nil];
    [request_ cancelWithError:error];
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AuthSessionRequest)

std::unique_ptr<content::NavigationThrottle> MaybeCreateAuthSessionThrottleFor(
    content::NavigationHandle* handle) API_AVAILABLE(macos(10.15)) {
  AuthSessionRequest* request =
      AuthSessionRequest::FromWebContents(handle->GetWebContents());
  if (!request)
    return nullptr;

  return request->CreateThrottle(handle);
}
