// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "TAMRContent.h"
#include <fstream>
#include <tinyAMR/Model.h>

namespace hs {
  namespace loader {
  
    TAMRContent::TAMRContent(const std::string &fileName,
                             int thisPartID,
                             bool showBlockDebug,
                             float isoValue)
      : fileName(fileName),
        thisPartID(thisPartID),
        showBlockDebug(showBlockDebug),
        isoValue(isoValue)
    {}

    void TAMRContent::create(DataLoader *loader,
                             const ResourceSpecifier &dataURL)
    {
      if (dataURL.numParts > 1)
        throw std::runtime_error("on-demand splitting of TAMR files not yet supported");
      // std::string type = dataURL.get("type",dataURL.get("format",""));
    
      const bool showBlockDebug = dataURL.has("dbg");
      float isoValue = NAN;
      const std::string isoString = dataURL.get("iso", dataURL.get("isoValue", ""));
      if (!isoString.empty())
        isoValue = std::stof(isoString);
      for (int i=0;i<dataURL.numParts;i++) {
        loader->addContent(new TAMRContent(dataURL.where, i, showBlockDebug, isoValue));
      }
    }
  
    size_t TAMRContent::projectedSize()
    {
      return getFileSize(fileName) * 10;
    }
  
    void TAMRContent::executeLoad(OnePartition &dataGroup) 
    {
      tamr::Model::SP model = tamr::Model::load(fileName);
      dataGroup.amr.push_back(std::make_shared<TAMRVolume>(model, vec3f(0.f), vec3f(1.f), isoValue));
      if (showBlockDebug)
        dataGroup.cylinderSets.push_back(TAMRVolume::createBlockDebugCylinders(model));
    }
  
    std::string TAMRContent::toString() 
    {
      std::stringstream ss;
      ss << "TinyAMR{#" << thisPartID << ",fileName="<<fileName<<"}";
      return ss.str();
    }

  }
}
