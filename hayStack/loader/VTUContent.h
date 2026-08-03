// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/loader/DataLoader.h"

namespace hs {
  namespace loader {
    
    /*! a VTK .vtu file of unstructured grid data, including polyhedral cells */
    struct VTUContent : public LoadableContent {
      VTUContent(const std::string &fileName);
      static void create(DataLoader *loader,
                         const std::string &dataURL);
      std::string toString() override;
      size_t projectedSize() override;
      void   executeLoad(DataRank &dataGroup, bool verbose) override;

      const std::string fileName;
      const size_t      fileSize;
    };

  }
}
