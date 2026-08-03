// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/loader/DataLoader.h"

namespace hs {
  namespace loader {
    
    /*! a 'umesh' file of unstructured mesh data */
    struct USDContent : public LoadableContent {
      USDContent(const std::string &fileName)
        : fileName(fileName),
          fileSize(getFileSize(fileName))
      {}

      std::string toString() override
      static void create(DataLoader *loader,
                         const std::string &dataURL);

      size_t projectedSize() override
      { return 2 * fileSize; }
    
      void   executeLoad(DataRank &dataGroup) override;

      const std::string fileName;
      const size_t      fileSize;
    };

  }
}
