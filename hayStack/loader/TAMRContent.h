// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/loader/DataLoader.h"

namespace hs {
  namespace loader {
  
    /*! a file of 'TinyAMR' (tamr) AMR files */
    struct TAMRContent : public LoadableContent {
    
      TAMRContent(const std::string &fileName,
                  int thisPartID);
    
      static void create(DataLoader *loader,
                         const ResourceSpecifier &dataURL);
      size_t projectedSize() override;
      void   executeLoad(OnePartition &dataGroup) override;

      std::string toString() override;

      const std::string   fileName;
      const int           thisPartID;
    };

  }
}
