// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/loader/DataLoader.h"

namespace hs {
  namespace loader {

    /*! "Tim Sandstrom" type ".tri" files */
    struct TSTriContent : public LoadableContent {
      enum { vertices_only, vertex_and_scalar, vertex_and_color } Type;
      TSTriContent(const ResourceSpecifier &data,
                   size_t fileSize,
                   int thisPartID,
                   int default_mappedValues);
      static void create(DataLoader *loader,
                         const ResourceSpecifier &dataURL,
                         int default_mappedValues = 0);
      size_t projectedSize() override;
      void   executeLoad(OnePartition &dataGroup) override;

      std::string toString() override;
      const ResourceSpecifier data;
      const size_t fileSize;
      const int default_mappedValues;
      const int thisPartID = 0;
    };
  
  }
}
