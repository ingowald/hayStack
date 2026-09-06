// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/loader/DataLoader.h"

namespace hs {
  namespace loader {
  
    /*! a file of 'TinyAMR' (tamr) AMR files */
    struct TAMRContent : public LoadableContent {
    
      TAMRContent(const ResourceSpecifier &dataURL,
                  int thisPartID);
      
      static void create(DataLoader *loader,
                         const ResourceSpecifier &dataURL);
      size_t projectedSize() override;
      void   executeLoad(OnePartition &dataGroup) override;
      
      std::string toString() override;

      const std::string fileName;
      const int         thisPartID;
      bool              showBlockDebug = false;
      float             iso0 = NAN;
      float             iso1 = NAN;
      float             iso2 = NAN;
      float             iso3 = NAN;
      vec4f             color0 {.8f,.1f,.1f,.5f};
      vec4f             color1 {.1f,.6f,.3f,.5f};
      vec4f             color2 {.2f,.1f,.8f,.5f};
      vec4f             color3 {.3f,.7f,.3f,.5f};
      bool              showVolume = true;
    };

  }
}
