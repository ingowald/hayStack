// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/loader/DataLoader.h"

namespace hs {
  namespace loader {

    /*! A dependency-free reader for VTK XML PolyData files with
        little-endian, raw appended arrays. */
    struct VTPContent : public LoadableContent {
      VTPContent(const std::string &fileName);
      static void create(DataLoader *loader, const std::string &fileName);

      std::string toString() override;
      size_t projectedSize() override;
      void executeLoad(OnePartition &dataGroup) override;

      const std::string fileName;
      const size_t fileSize;
    };

  }
}
