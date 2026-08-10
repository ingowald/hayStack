// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/loader/DataLoader.h"

namespace hs {
  namespace loader {
    
  /*! a file of 'raw' spheres */
    struct SpheresFromFile : public LoadableContent {
      SpheresFromFile(const ResourceSpecifier &data,
                      int thisPartID,
                      float defaultRadius);
      static void create(DataLoader *loader,
                         const ResourceSpecifier &dataURL);
      size_t projectedSize() override;
      void   executeLoad(OnePartition &dataGroup) override;

      std::string toString() override;

      const float radius;
      const ResourceSpecifier data;
      const size_t fileSize;
      const int thisPartID = 0;
    };

    struct VMDSpheres : public LoadableContent {
      VMDSpheres(const ResourceSpecifier &data,
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
