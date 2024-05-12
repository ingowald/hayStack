// ======================================================================== //
// Copyright 2022-2024 Ingo Wald                                            //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "viewer/DataLoader.h"
#include <fstream>

namespace hs {

  /*! a 'umesh' file of unstructured mesh data */
  struct UMeshContent : public LoadableContent {
    UMeshContent(const std::string &fileName);
    static void create(DataLoader *loader,
                       const std::string &dataURL);
    std::string toString() override;
    size_t projectedSize() override;
    void   executeLoad(DataRank &dataGroup, bool verbose) override;

    const std::string fileName;
    const size_t      fileSize;
  };

  /*! a 'umesh' file of unstructured mesh data */
  struct SpatiallyPartitionedUMeshContent : public LoadableContent {
    SpatiallyPartitionedUMeshContent(const std::string umeshFileName,
                                     const box3f &domain);
    static void create(DataLoader *loader,
                       const ResourceSpecifier &dataURL);
    std::string toString() override;
    size_t      projectedSize() override;
    void        executeLoad(DataRank &dataGroup, bool verbose) override;

    const std::string fileName;
    const size_t      fileSize;
    const box3f       domain;
  };

}
