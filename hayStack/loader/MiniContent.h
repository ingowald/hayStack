// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/loader/LoadableContent.h"

namespace hs {
  namespace loader {
    
    /*! a 'umesh' file of unstructured mesh data */
    struct MiniContent : public DistributedContent {
      MiniContent(const std::string &fileName);

      static void create(DataLoader *loader,
                         const std::string &dataURL);
    
      std::string toString() override;
      size_t projectedSize() override;
      void   executeLoad(OnePartition &dataGroup) override;

      const std::string fileName;
      const size_t      fileSize;
    };

  }
}
