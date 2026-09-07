// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayStack/TransferFunction.h"
#include <fstream>

namespace hs {

  const size_t xfFileFormatMagic = 0x1235abc000;

  void TransferFunction::save(const std::string &fileName) const
  {
    std::ofstream out(fileName,std::ios::binary);
    out.write((char*)&xfFileFormatMagic,sizeof(xfFileFormatMagic));
    
    out.write((char*)&baseDensity,sizeof(baseDensity));
    out.write((char*)&absDomain,sizeof(absDomain));
    out.write((char*)&relDomain,sizeof(relDomain));

    // const auto &colorMap = getColorMap();
    int numColorMapValues = colorMap.size();
    out.write((char*)&numColorMapValues,sizeof(numColorMapValues));
    out.write((char*)colorMap.data(),colorMap.size()*sizeof(colorMap[0]));
    std::cout << "#hs.Transferfunction: saved xf to "
              <<  fileName << std::endl;
  }  

  range1f TransferFunction::getDomain() const
  {
    float absDomainSize = absDomain.upper - absDomain.lower;
    range1f domain;
    domain.lower = absDomain.lower + (relDomain.lower/100.f) * absDomainSize;
    domain.upper = absDomain.lower + (relDomain.upper/100.f) * absDomainSize;
    return domain;
  }

  void TransferFunction::load(const std::string &fileName)
  {
    std::cout << "#hs: loading transfer function " << fileName << std::endl;
    // std::vector<vec4f> colorMap = { vec4f(1.f), vec4f(1.f) };
    // range1f domain = { 0.f, 0.f };
    // float   baseDensity = 1.f;
    std::ifstream in(fileName.c_str(),std::ios::binary);
    size_t magic;
    in.read((char*)&magic,sizeof(xfFileFormatMagic));
    
    in.read((char*)&baseDensity,sizeof(baseDensity));
    // baseDensity = powf(1.1f,baseDensity - 100.f);
    
    in.read((char*)&absDomain,sizeof(absDomain));
    in.read((char*)&relDomain,sizeof(relDomain));

    // ColorMap colorMap;
    int numColorMapValues;
    in.read((char*)&numColorMapValues,sizeof(numColorMapValues));
    colorMap.resize(numColorMapValues);
    in.read((char*)colorMap.data(),colorMap.size()*sizeof(colorMap[0]));
    // alphaEditor->setColorMap(colorMap,AlphaEditor::OVERWRITE_ALPHA);
  }
    
}
