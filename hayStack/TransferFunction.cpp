// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayStack/TransferFunction.h"
#include <fstream>

namespace hs {

  void TransferFunction::load(const std::string &fileName)
  {
    std::cout << "#hs: loading transfer function " << fileName << std::endl;
    // std::vector<vec4f> colorMap = { vec4f(1.f), vec4f(1.f) };
    // range1f domain = { 0.f, 0.f };
    // float   baseDensity = 1.f;
    static const size_t xfFileFormatMagic = 0x1235abc000;
    std::ifstream in(fileName.c_str(),std::ios::binary);
    size_t magic;
    in.read((char*)&magic,sizeof(xfFileFormatMagic));
    
    in.read((char*)&baseDensity,sizeof(baseDensity));
    // baseDensity = powf(1.1f,baseDensity - 100.f);
    
    range1f absDomain, relDomain;
    in.read((char*)&absDomain,sizeof(absDomain));
    in.read((char*)&relDomain,sizeof(relDomain));

    float absDomainSize = absDomain.upper - absDomain.lower;
    domain.lower = absDomain.lower + (relDomain.lower/100.f) * absDomainSize;
    domain.upper = absDomain.lower + (relDomain.upper/100.f) * absDomainSize;
    // ColorMap colorMap;
    int numColorMapValues;
    in.read((char*)&numColorMapValues,sizeof(numColorMapValues));
    colorMap.resize(numColorMapValues);
    in.read((char*)colorMap.data(),colorMap.size()*sizeof(colorMap[0]));
    // alphaEditor->setColorMap(colorMap,AlphaEditor::OVERWRITE_ALPHA);
  }
    
}
