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

namespace hs {
  
  /*! a file of 'raw' spheres */
  struct SpheresFromFile : public LoadableContent {
    SpheresFromFile(const std::string &fileName,
                    size_t fileSize,
                    int thisPartID,
                    int numPartsToSplitInto,
                    float radius);
    static void create(DataLoader *loader,
                       const std::string &dataURL);
    size_t projectedSize() override;
    void   executeLoad(DataGroup &dataGroup, bool verbose) override;

    std::string toString() override
    {
      return "Spheres{fileName="+fileName+", part "+std::to_string(thisPartID)+" of "
        + std::to_string(numPartsToSplitInto)+", proj size "
        +prettyNumber(projectedSize())+"B}";
    }

    const float radius;
    const std::string fileName;
    const size_t fileSize;
    const int thisPartID = 0;
    const int numPartsToSplitInto = 1;
  };
  
}
