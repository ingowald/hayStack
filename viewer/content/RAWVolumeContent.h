// ======================================================================== //
// Copyright 2022-2023 Ingo Wald                                            //
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
#include "hayStack/StructuredVolume.h"

namespace hs {
  
  /*! a file of 'raw' spheres */
  struct RAWVolumeContent : public LoadableContent {
    using ScalarType = StructuredVolume::ScalarType;
    
    RAWVolumeContent(const std::string &fileName,
                     int thisPartID,
                     const box3i &cellRange,
                     vec3i fullVolumeDims,
                     ScalarType type,
                     int numChannels);
    
    static void create(DataLoader *loader,
                       const ResourceSpecifier &dataURL);
    size_t projectedSize() override;
    void   executeLoad(DataGroup &dataGroup, bool verbose) override;

    std::string toString() override;

    const std::string fileName;
    const int         thisPartID;
    const vec3i       fullVolumeDims;
    const box3i       cellRange;
    const int         numChannels;
    const ScalarType  scalarType;
    const box3i       myCells;
  };
  
}
