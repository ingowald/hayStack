// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/loader/LoadableContent.h"

namespace hs {
  namespace loader {
    
    /*! IsoDump: surfaces generated from a separate iso-surace
      generation-and-dump-to-disk tool */
    struct IsoDumpContent : public DistributedContent {
      IsoDumpContent(const ResourceSpecifier &data,
                     int thisPartID);
      static void create(DataLoader *loader,
                         const ResourceSpecifier &dataURL);
      size_t projectedSize() override;
      void   executeLoad(OnePartition &dataGroup) override;

      std::string toString() override
      {
        return "IsoDumpContent{fileName="+data.where+", part "+std::to_string(thisPartID)+
          ", proj size "
          +prettyNumber(projectedSize())+"B}";
      }
      const ResourceSpecifier data;
      const int thisPartID = 0;
    };
  
  }
}
