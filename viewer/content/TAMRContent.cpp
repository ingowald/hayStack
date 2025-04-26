// ======================================================================== //
// Copyright 2025++ Ingo Wald                                               //
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

#include "TAMRContent.h"
#include <fstream>
#include <tinyAMR/Model.h>

namespace hs {
  
  TAMRContent::TAMRContent(const std::string &fileName,
                           int thisPartID)
    : fileName(fileName),
      thisPartID(thisPartID)
  {}

  void TAMRContent::create(DataLoader *loader,
                           const ResourceSpecifier &dataURL)
  {
    if (dataURL.numParts > 1)
      throw std::runtime_error("on-demand splitting of TAMR files not yet supported");
    // std::string type = dataURL.get("type",dataURL.get("format",""));
    
    for (int i=0;i<dataURL.numParts;i++) {
      loader->addContent(new TAMRContent(dataURL.where,i));
    }
  }
  
  size_t TAMRContent::projectedSize()
  {
    return getFileSize(fileName) * 10;
  }
  
  void TAMRContent::executeLoad(DataRank &dataGroup, bool verbose)
  {
    tamr::Model::SP model = tamr::Model::load(fileName);
    dataGroup.amrVolumes.push_back(model);
  }
  
  std::string TAMRContent::toString() 
  {
    std::stringstream ss;
    ss << "TinyAMR{#" << thisPartID << ",fileName="<<fileName<<"}";
    return ss.str();
  }
  
}
