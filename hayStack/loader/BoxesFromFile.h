// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/loader/DataLoader.h"

namespace hs {
  namespace loader {
  
    /*! a file of 'raw' boxes */
    struct BoxesFromFile : public LoadableContent {
      BoxesFromFile(const ResourceSpecifier &data,
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
