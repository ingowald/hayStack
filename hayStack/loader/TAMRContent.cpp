// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "TAMRContent.h"
#include <fstream>
#include <tinyAMR/Model.h>

namespace hs {
  namespace loader {
  
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
  
    void TAMRContent::executeLoad(OnePartition &dataGroup)
    {
      tamr::Model::SP model = tamr::Model::load(fileName);
      dataGroup.amr.push_back(std::make_shared<TAMRVolume>(model));
    }
  
    std::string TAMRContent::toString() 
    {
      std::stringstream ss;
      ss << "TinyAMR{#" << thisPartID << ",fileName="<<fileName<<"}";
      return ss.str();
    }

  }
}
