// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayMaker/TextureLibrary.h"

namespace hm {

  template<typename Backend>
  TextureLibrary<Backend>::TextureLibrary(typename Backend::Slot *backend)
    : backend(backend)
  {}
  
  template<typename Backend>
  typename Backend::TextureHandle
  TextureLibrary<Backend>::getOrCreate(mini::Texture::SP miniTex)
  {
    auto it = alreadyCreated.find(miniTex);
    if (it != alreadyCreated.end()) return it->second;

    auto bnTex = backend->create(miniTex);
    alreadyCreated[miniTex] = bnTex;
    return bnTex;
  }

}
