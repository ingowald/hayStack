// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/loader/DataLoader.h"

namespace hs {
  namespace loader {
    
    /*! a 'umesh' file of unstructured mesh data */
    struct OBJContent : public LoadableContent {
      OBJContent(const std::string &fileName)
        : fileName(fileName),
          fileSize(getFileSize(fileName))
      {}

      static void create(DataLoader *loader,
                         const std::string &dataURL)
      {
        loader->addContent(new OBJContent(/* this is a plain filename for umesh:*/dataURL));
      }
    
      std::string toString() override
      {
        return "OBJ{fileName="+fileName+", proj size "
          +prettyNumber(projectedSize())+"B}";
      }
      size_t projectedSize() override
      { return 2 * fileSize; }
    
      void   executeLoad(OnePartition &dataGroup) override;

      const std::string fileName;
      const size_t      fileSize;
    };

  }
}
