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
// ======================================================================== //a

#pragma once

#include "nanovdb/GridHandle.h"
#include "hayStack/HayStack.h"

namespace hs {

  struct NanoVDB {
    typedef std::shared_ptr<NanoVDB> SP;

    NanoVDB(std::vector<float> &gridData);

    box3f getBounds() const;
    range1f getValueRange() const;

    float *getData() const
    { return alignedPtr; }

    size_t elemCount() const
    { return rawSize / sizeof(rawData[0]); }

    size_t sizeInBytes() const
    { return rawSize; }

    // and another copy as a nanovdb handle, to implement
    // getBounds() and getValueRange():
    nanovdb::GridHandle<nanovdb::HostBuffer> gridHandle;
   private:
    // We store two copies of the data, one for the backends:
    float *rawData{nullptr};
    float *alignedPtr{nullptr};
    size_t rawSize{0}; // in bytes
  };
}
