// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_UTILS_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_UTILS_H_

#include <functional>
#include <map>
#include <string>

#include "base/containers/flat_map.h"
#include "chrome/common/extensions/api/passwords_private.h"

namespace password_manager {
struct PasswordForm;
}  // namespace password_manager

namespace extensions {

// Obtains a collection of URLs from the passed in form. This includes an origin
// URL used for internal logic, a human friendly string shown to the user as
// well as a URL that is linked to.
api::passwords_private::UrlCollection CreateUrlCollectionFromForm(
    const password_manager::PasswordForm& form);

// This class is an id generator for an arbitrary key type. It is used by both
// PasswordManagerPresenter and PasswordCheckDelegate to create ids send to the
// UI. It is similar to base::IDMap, but has the following important
// differences:
// - IdGenerator owns a copy of the key data, so that clients don't need to
//   worry about dangling pointers.
// - Repeated calls to GenerateId with the same |key| are no-ops, and return the
//   same ids.
template <typename KeyT,
          typename IdT = int32_t,
          typename KeyCompare = std::less<>>
class IdGenerator {
 public:
  // This method generates an id corresponding to |key|. Additionally it
  // remembers ids generated in the past, so that this method is idempotent.
  // Furthermore, it is guaranteed that different ids are returned for different
  // |key| arguments. This implies GenerateId(a) == GenerateId(b) if and only if
  // a == b.
  IdT GenerateId(const KeyT& key) {
    auto result = key_cache_.emplace(key, next_id_);
    if (result.second) {
      // In case we haven't seen |key| before, add a pointer to the inserted key
      // and the corresponding id to the |id_cache_|. This insertion should
      // always succeed.
      auto iter = id_cache_.emplace_hint(id_cache_.end(), next_id_,
                                         &result.first->first);
      DCHECK_EQ(&result.first->first, iter->second);
      ++next_id_;
    }

    return result.first->second;
  }

  // This method tries to return the key corresponding to |id|. In case |id| was
  // not generated by GenerateId() before, this method returns nullptr.
  // Otherwise it returns a pointer p to a key, such that |id| ==
  // GenerateId(*p).
  const KeyT* TryGetKey(IdT id) const {
    auto it = id_cache_.find(id);
    return it != id_cache_.end() ? it->second : nullptr;
  }

 private:
  std::map<KeyT, IdT, KeyCompare> key_cache_;
  base::flat_map<IdT, const KeyT*> id_cache_;
  IdT next_id_ = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_UTILS_H_