// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/loader/DataLoader.h"
#include <fstream>

namespace hs {
  namespace loader {

    /*! a 'umesh' file of unstructured mesh data */
    struct UMeshContent : public LoadableContent {
      UMeshContent(const ResourceSpecifier &dataURL);
      static void create(DataLoader *loader,
                         const ResourceSpecifier &dataURL);
      std::string toString() override;
      size_t projectedSize() override;
      void   executeLoad(OnePartition &dataGroup) override;

      const ResourceSpecifier dataURL;
      const std::string fileName;
      const size_t      fileSize;
      /*! name of transfer function to use for this volume */
      const std::string xfName;
    };

    /*! a 'umesh' file of unstructured mesh data */
    struct SpatiallyPartitionedUMeshContent : public LoadableContent {
      SpatiallyPartitionedUMeshContent(const std::string umeshFileName,
                                       const box3f &domain,
                                       /*! name of transfer function to use for this volume */
                                       const std::string &xfName);
      static void create(DataLoader *loader,
                         const ResourceSpecifier &dataURL);
      std::string toString() override;
      size_t      projectedSize() override;
      void        executeLoad(OnePartition &dataGroup) override;

      const std::string fileName;
      const size_t      fileSize;
      const box3f       domain;
      const std::string xfName;
    };

  }
}
