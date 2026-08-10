// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/loader/DataLoader.h"

namespace hs {
  namespace loader {

    /*! "Tim Sandstrom" type ".tri" files */
    struct TSTriContent : public LoadableContent {
      TSTriContent(const ResourceSpecifier &data,
                   size_t fileSize,
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
