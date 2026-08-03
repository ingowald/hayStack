// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/loader/DataLoader.h"

namespace hs {
  namespace loader {
    
    /*! a file of 'raw' spheres */
    struct MaterialsTest : public LoadableContent {
      enum { gridRes = 10 };
    
      MaterialsTest(const ResourceSpecifier &data,
                    int thisPartID);
      static void create(DataLoader *loader,
                         const ResourceSpecifier &dataURL);
      size_t projectedSize() override;
      void   executeLoad(OnePartition &dataGroup) override;
    
      std::string toString() override;
    
      const ResourceSpecifier data;
      const int thisPartID = 0;
    };
  
  }
}
