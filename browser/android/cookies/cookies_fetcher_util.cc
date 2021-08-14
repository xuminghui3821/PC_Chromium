// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/android/jni_headers/CookiesFetcher_jni.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

// Returns the cookie service at the client end of the mojo pipe.
network::mojom::CookieManager* GetCookieServiceClient() {
  // Since restoring Incognito CCT session from cookies is not supported, it is
  // safe to use the primary OTR profile here.
  return content::BrowserContext::GetDefaultStoragePartition(
             ProfileManager::GetPrimaryUserProfile()->GetPrimaryOTRProfile())
      ->GetCookieManagerForBrowserProcess();
}

// Passes the fetched |cookies| to the application so that can be saved in a
// file.
void OnCookiesFetchFinished(const net::CookieList& cookies) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jobjectArray> joa =
      Java_CookiesFetcher_createCookiesArray(env, cookies.size());

  int index = 0;
  for (auto i = cookies.cbegin(); i != cookies.cend(); ++i) {
    ScopedJavaLocalRef<jobject> java_cookie = Java_CookiesFetcher_createCookie(
        env, base::android::ConvertUTF8ToJavaString(env, i->Name()),
        base::android::ConvertUTF8ToJavaString(env, i->Value()),
        base::android::ConvertUTF8ToJavaString(env, i->Domain()),
        base::android::ConvertUTF8ToJavaString(env, i->Path()),
        i->CreationDate().ToDeltaSinceWindowsEpoch().InMicroseconds(),
        i->ExpiryDate().ToDeltaSinceWindowsEpoch().InMicroseconds(),
        i->LastAccessDate().ToDeltaSinceWindowsEpoch().InMicroseconds(),
        i->IsSecure(), i->IsHttpOnly(), static_cast<int>(i->SameSite()),
        i->Priority(), i->IsSameParty(), static_cast<int>(i->SourceScheme()),
        i->SourcePort());
    env->SetObjectArrayElement(joa.obj(), index++, java_cookie.obj());
  }

  Java_CookiesFetcher_onCookieFetchFinished(env, joa);
}

}  // namespace

// Fetches cookies for the off-the-record session (i.e. incognito mode). It is a
// no-op for the standard session. Typically associated with the #onPause of
// Android's activty lifecycle.
void JNI_CookiesFetcher_PersistCookies(JNIEnv* env) {
  if (!ProfileManager::GetPrimaryUserProfile()->HasPrimaryOTRProfile()) {
    // There is no work to be done. We might consider calling
    // the Java callback if needed.
    return;
  }

  GetCookieServiceClient()->GetAllCookies(
      base::BindOnce(&OnCookiesFetchFinished));
}

// Creates and sets a canonical cookie for the off-the-record session (i.e.
// incognito mode). It is a no-op for the standard session. Typically associated
// with the #onResume of Android's activty lifecycle.
static void JNI_CookiesFetcher_RestoreCookies(
    JNIEnv* env,
    const JavaParamRef<jstring>& name,
    const JavaParamRef<jstring>& value,
    const JavaParamRef<jstring>& domain,
    const JavaParamRef<jstring>& path,
    jlong creation,
    jlong expiration,
    jlong last_access,
    jboolean secure,
    jboolean httponly,
    jint same_site,
    jint priority,
    jboolean same_party,
    jint source_scheme,
    jint source_port) {
  if (!ProfileManager::GetPrimaryUserProfile()->HasPrimaryOTRProfile())
    return;  // Don't create it. There is nothing to do.

  std::string domain_str(base::android::ConvertJavaStringToUTF8(env, domain));
  std::string path_str(base::android::ConvertJavaStringToUTF8(env, path));

  std::unique_ptr<net::CanonicalCookie> cookie =
      net::CanonicalCookie::FromStorage(
          base::android::ConvertJavaStringToUTF8(env, name),
          base::android::ConvertJavaStringToUTF8(env, value), domain_str,
          path_str,
          base::Time::FromDeltaSinceWindowsEpoch(
              base::TimeDelta::FromMicroseconds(creation)),
          base::Time::FromDeltaSinceWindowsEpoch(
              base::TimeDelta::FromMicroseconds(expiration)),
          base::Time::FromDeltaSinceWindowsEpoch(
              base::TimeDelta::FromMicroseconds(last_access)),
          secure, httponly, static_cast<net::CookieSameSite>(same_site),
          static_cast<net::CookiePriority>(priority), same_party,
          static_cast<net::CookieSourceScheme>(source_scheme), source_port);
  if (!cookie)
    return;

  // Assume HTTPS - since the cookies are being restored from another store,
  // they have already gone through the strict secure check.
  //
  // Similarly, permit samesite cookies to be imported.
  net::CookieOptions options;
  options.set_include_httponly();
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());
  options.set_do_not_update_access_time();
  GetCookieServiceClient()->SetCanonicalCookie(
      *cookie,
      net::cookie_util::CookieDomainAndPathToURL(
          domain_str, path_str,
          static_cast<net::CookieSourceScheme>(source_scheme)),
      options, network::mojom::CookieManager::SetCanonicalCookieCallback());
}
