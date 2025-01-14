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

#include "nanovdb/io/IO.h"
#include "hayStack/NanoVDB.h"

namespace hs {

  NanoVDB::NanoVDB(std::vector<float> &gridData)
  {
    // deep copy for back-ends:
    rawSize = sizeof(gridData[0])*gridData.size();
    rawData = (float *)malloc(rawSize+NANOVDB_DATA_ALIGNMENT);
    alignedPtr = nanovdb::alignPtr(rawData);
    memcpy(getData(), gridData.data(), sizeInBytes());

    // nanovdb 'copy', for bounds, valueRange, etc.
    auto buffer = nanovdb::HostBuffer::createFull(elemCount(),getData());
    gridHandle = std::move(buffer);
  }

  NanoVDB::~NanoVDB()
  {
    free(rawData);
  }

  box3f NanoVDB::getBounds() const
  {
    auto bbox = gridHandle.gridMetaData()->indexBBox();
    auto lower = bbox.min();
    auto upper = bbox.max();
    return box3f{
      {(float)lower[0], (float)lower[1], (float)lower[2]},
      {(float)upper[0], (float)upper[1], (float)upper[2]}
    };
  }

  range1f NanoVDB::getValueRange() const
  {
    auto bbox = gridHandle.gridMetaData()->indexBBox();

    auto grid = gridHandle.grid<float>();
    auto acc = grid->getAccessor();

    range1f range;
    for (nanovdb::CoordBBox::Iterator iter = bbox.begin(); iter; ++iter) {
      float value = acc.getValue(*iter);
      range.extend(value);
    }
    std::cout << range << '\n';
    return range;
  }
}
