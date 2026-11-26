// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/loader/LoadableContent.h"
#include "hayStack/StructuredVolume.h"

namespace hs {
  namespace loader {
  
    /*! a file of 'raw' spheres */
    struct RAWVolumeContent : public DistributedContent {
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
                       const float isoValue,
                       /*! name of transfer function to use for this volume */
                       const std::string &xfName,
                       /*! world-space position of the full volume's
                           first voxel */
                       vec3f gridOrigin,
                       /*! world-space distance between two
                           neighboring voxels */
                       vec3f gridSpacing);
    
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
      const std::string   xfName;
      const vec3f         gridOrigin;
      const vec3f         gridSpacing;
    };
  
  }
}
