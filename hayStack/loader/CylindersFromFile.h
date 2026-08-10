// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/loader/DataLoader.h"

namespace hs {
  namespace loader {
    
    /*! a file of 'raw' spheres */
    struct CylindersFromFile : public LoadableContent {
      CylindersFromFile(const std::string &fileName,
                        size_t fileSize,
                        int thisPartID,
                        int numPartsToSplitInto,
                        float radius,
                        vec3f shift,
                        vec3f scale);
      static void create(DataLoader *loader,
                         const ResourceSpecifier &dataURL);
      size_t projectedSize() override;
      void   executeLoad(OnePartition &dataGroup) override;
    
      std::string toString() override
      {
        return "Cylinders{fileName="+fileName+", part "+std::to_string(thisPartID)+" of "
          + std::to_string(numPartsToSplitInto)+", proj size "
          +prettyNumber(projectedSize())+"B}";
      }

      const vec3f shift = 1.f;// = vec3f(-10.5,-5,0);
      const vec3f scale = 1.f;
    
      const float radius;
      const std::string fileName;
      const size_t fileSize;
      const int thisPartID = 0;
      const int numPartsToSplitInto = 1;
    };


    /*! simple position/normal/color/index triangle meshes in binary format */
    struct VMDCyls : public LoadableContent {
      VMDCyls(const ResourceSpecifier &data,
              int thisPartID);
      static void create(DataLoader *loader,
                         const ResourceSpecifier &dataURL);
      size_t projectedSize() override;
      void   executeLoad(OnePartition &dataGroup) override;
    
      std::string toString() override;

      const ResourceSpecifier data;
      const size_t fileSize;
      const int thisPartID = 0;
    };
    
  }  
}
