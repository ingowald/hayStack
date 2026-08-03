// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/loader/DataLoader.h"
#include "hayStack/StructuredVolume.h"

namespace hs {
  namespace loader {
  
    /*! a file of 'raw' spheres */
    struct RAWVolumeContent : public LoadableContent {
      // using ScalarType = StructuredVolume::ScalarType;
    
      RAWVolumeContent(const std::string &fileName,
                       int thisPartID,
                       const box3i &cellRange,
                       vec3i fullVolumeDims,
                       const std::string &texelFormat,
                       int numChannels,
                       /*! if not NaN, we'll actually not store the
                         volume, but run iso-value extraction and use
                         the resulting surface(s) */
                       const float isoValue);
    
      static void create(DataLoader *loader,
                         const ResourceSpecifier &dataURL);
      size_t projectedSize() override;
      void   executeLoad(OnePartition &dataGroup) override;

      std::string toString() override;

      const std::string   fileName;
      const int           thisPartID;
      const vec3i         fullVolumeDims;
      const box3i         cellRange;
      const int           numChannels;
      const std::string   texelFormat;
      const float         isoValue;
    };
  
  }
}
