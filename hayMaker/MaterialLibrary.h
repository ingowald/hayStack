// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayMaker/Anari.h"

namespace hm {
  struct PerDevice;
  
  struct ScalarMapper {
    anari::Sampler sampler;
  };

  struct ColorMapper {
    anari::Sampler sampler;
  };
  
  /*! keeps track of which frontend materials have already been
    created on the backend, and returns handle to already created
    backend variant if it exists -- or creates one if this is not
    yet the case */
  struct MaterialLibrary {
    
    MaterialLibrary(PerDevice *device);
    ~MaterialLibrary();
    
    anari::Material getOrCreate(mini::Material::SP miniMat,
                                ColorMapper  *colorMapper  = nullptr,
                                ScalarMapper *scalarMapper = nullptr);

  private:
    std::map<
      /*key*/
      std::tuple<mini::Material::SP,
                 ColorMapper *,
                 ScalarMapper *>,
      /* value */
      anari::Material> alreadyCreated;
    
    anari::Device device;
  };
  
}

