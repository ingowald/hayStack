// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/loader/DataLoader.h"

namespace hs {
  namespace loader {

    struct VTPContent : public LoadableContent {
      VTPContent(const ResourceSpecifier &data);
      static void create(DataLoader *loader,
                         const ResourceSpecifier &data);

      std::string toString() override;
      size_t projectedSize() override;
      void executeLoad(OnePartition &dataGroup) override;

      const ResourceSpecifier data;
      const size_t fileSize;
    };
    
  }
}
