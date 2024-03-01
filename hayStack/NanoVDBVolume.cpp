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

/*! a hay-*stack* is a description of data-parallel data */

#include "hayStack/NanoVDBVolume.h"
#include <nanovdb/NanoVDB.h>

namespace hs {

  box3f NanoVDBVolume::getBounds() const
  {
    box3f bb;
    bb.lower = gridOrigin;
    bb.upper = gridSpacing * vec3f(dims);
    return bb;
  }

  range1f NanoVDBVolume::getValueRange() const
  {
    size_t numScalars = dims.x*(size_t)dims.y*dims.z;
    range1f range;
    switch (texelFormat) {
    case BN_TEXEL_FORMAT_NANOVDB_FLOAT:
    {
        nanovdb::NanoGrid<float>* const grid = (nanovdb::NanoGrid<float> *)rawData.data();        
        range.extend(grid->tree().root().data()->getMin());
        range.extend(grid->tree().root().data()->getMax());
    }
    break;
    default:
      HAYSTACK_NYI();
    }
    return range;
  }
  
}