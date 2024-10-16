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

#include "HayMaker.h"
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
    baseDensity = powf(1.1f,baseDensity - 100.f);
    
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
