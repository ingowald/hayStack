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

#include "nanovdb/io/IO.h"
#include "NanoVDBContent.h"

namespace hs {
  

  NanoVDBContent::NanoVDBContent(const std::string &fileName) : fileName(fileName)
  {}

  void NanoVDBContent::create(DataLoader *loader,
                              const std::string &dataURL)
  {
    loader->addContent(new NanoVDBContent(/* this is a plain filename for umesh:*/dataURL));
  }

  std::string NanoVDBContent::toString()
  {
    return "NanoVDB{fileName="+fileName+", proj size "
      +prettyNumber(projectedSize())+"B}";
  }

  size_t NanoVDBContent::projectedSize()
  {
    if (!vdb) return 0;
    return 2 * vdb->sizeInBytes();
  }

  void   NanoVDBContent::executeLoad(DataRank &dataRank, bool verbose)
  {
    auto grid = nanovdb::io::readGrid(fileName);
    std::vector<float> gridData(grid.size());
    memcpy((uint8_t *)gridData.data(), grid.data(), grid.size());
    vdb = std::make_shared<NanoVDB>(gridData);
    dataRank.vdbs.push_back(vdb);
  }
}
