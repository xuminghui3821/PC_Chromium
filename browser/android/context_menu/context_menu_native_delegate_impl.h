// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTEXT_MENU_CONTEXT_MENU_NATIVE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ANDROID_CONTEXT_MENU_CONTEXT_MENU_NATIVE_DELEGATE_IMPL_H_

#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {
class WebContents;
}

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.contextmenu
enum ContextMenuImageFormat {
  JPEG = 0,
  PNG = 1,
  ORIGINAL = 2,
};

class ContextMenuNativeDelegateImpl {
 public:
  explicit ContextMenuNativeDelegateImpl(
      content::WebContents* const web_contents,
      content::ContextMenuParams* const context_menu_params);

  void RetrieveImageForContextMenu(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jrender_frame_host,
      const base::android::JavaParamRef<jobject>& jcallback,
      jint max_width_px,
      jint max_height_px);
  void RetrieveImageForShare(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jrender_frame_host,
      const base::android::JavaParamRef<jobject>& jcallback,
      jint max_width_px,
      jint max_height_px,
      jint j_image_type);
  void StartDownload(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     jboolean jis_link);
  void SearchForImage(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jrender_frame_host);

 protected:
  using ImageRetrieveCallback = base::OnceCallback<void(
      mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
          chrome_render_frame_ptr,
      const base::android::JavaRef<jobject>& jcallback,
      const std::vector<uint8_t>& thumbnail_data,
      const gfx::Size& max_dimen_px,
      const std::string& image_extension)>;

 private:
  void RetrieveImageInternal(
      JNIEnv* env,
      ImageRetrieveCallback retrieve_callback,
      const base::android::JavaParamRef<jobject>& jrender_frame_host,
      const base::android::JavaParamRef<jobject>& jcallback,
      jint max_width_px,
      jint max_height_px,
      chrome::mojom::ImageFormat image_format);

  content::WebContents* const web_contents_;
  content::ContextMenuParams* const context_menu_params_;
};

#endif  // CHROME_BROWSER_ANDROID_CONTEXT_MENU_CONTEXT_MENU_NATIVE_DELEGATE_IMPL_H_
