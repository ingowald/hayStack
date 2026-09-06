// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "TAMRContent.h"
#include <fstream>
#include <tinyAMR/Model.h>

namespace hs {
  namespace loader {
  
    TAMRContent::TAMRContent(const ResourceSpecifier &dataURL,
                             int thisPartID)
      : fileName(dataURL.where),
        thisPartID(thisPartID)
    {
      iso0 = dataURL.get_float("iso",iso0);
      iso0 = dataURL.get_float("iso0",iso0);
      iso1 = dataURL.get_float("iso1",iso1);
      iso2 = dataURL.get_float("iso2",iso2);
      iso3 = dataURL.get_float("iso3",iso3);

      color0 = dataURL.get_vec4f("color",color0);
      color0 = dataURL.get_vec4f("color0",color0);
      color1 = dataURL.get_vec4f("color1",color1);
      color2 = dataURL.get_vec4f("color2",color2);
      color3 = dataURL.get_vec4f("color3",color3);
      
      showVolume = isnan(iso0) || dataURL.has("showVolume");
      showBlockDebug = dataURL.has("dbg");
    }

    void TAMRContent::create(DataLoader *loader,
                             const ResourceSpecifier &dataURL)
    {
      if (dataURL.numParts > 1)
        throw std::runtime_error("on-demand splitting of TAMR files not yet supported");
      for (int i=0;i<dataURL.numParts;i++) {
        loader->addContent(new TAMRContent(dataURL, i));
      }
    }
  
    size_t TAMRContent::projectedSize()
    {
      return getFileSize(fileName) * 10;
    }
  
    void TAMRContent::executeLoad(OnePartition &dataGroup) 
    {
      tamr::Model::SP model = tamr::Model::load(fileName);
      TAMRVolume::SP content
        = std::make_shared<TAMRVolume>(model,
                                       iso0,iso1,iso2,iso3,
                                       color0,color1,color2,color3,
                                       showVolume);
      dataGroup.amr.push_back(content);
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
