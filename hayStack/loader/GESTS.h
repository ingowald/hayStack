// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/loader/DataLoader.h"
#include "hayStack/StructuredVolume.h"

namespace hs {
  namespace loader {
  
    /*! a file of 'raw' spheres */
    struct GESTSVolumeContent : public LoadableContent {
      // using ScalarType = StructuredVolume::ScalarType;
    
      GESTSVolumeContent(const std::string &filePrefix,
                         int thisPartID,
                         int cubeDims,
                         int cubesPerFile);
    
      static void create(DataLoader *loader,
                         const ResourceSpecifier &dataURL);
      size_t projectedSize() override;
      void   executeLoad(OnePartition &dataGroup) override;

      std::string toString() override;

      const std::string   filePrefix;
      const int           thisPartID;
      const int           cubeDims;
      const int           cubesPerFile;
    };

  }
}
