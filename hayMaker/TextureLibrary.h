// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayMaker/common.h"

namespace hm {

  struct AnariDeviceRenderer;
  
  /*! keeps track of which frontend textures have already been
    created on the backend, and returns handle to already created
    backend variant if it exists -- or creates one if this is not
    yet the case */
  struct TextureLibrary
  {
    TextureLibrary(AnariDeviceRenderer *renderer);
    anari::Sampler getOrCreate(mini::Texture::SP miniTex);
    
  private:
    anari::Sampler
    create(mini::Texture::SP miniTex);
    
    AnariDeviceRenderer *const renderer;
    std::map<mini::Texture::SP,anari::Sampler> alreadyCreated;
  };

}


