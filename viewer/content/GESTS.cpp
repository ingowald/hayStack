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

#include "GESTS.h"
#include <fstream>
#include <umesh/UMesh.h>
#include <umesh/extractIsoSurface.h>
#include <miniScene/Scene.h>

namespace hs {
  
  GESTSVolumeContent::GESTSVolumeContent(const std::string &filePrefix,
                                         int thisPartID)
    : filePrefix(filePrefix),
      thisPartID(thisPartID)
  {}

  void GESTSVolumeContent::create(DataLoader *loader,
                                const ResourceSpecifier &dataURL)
  {
    for (int i=0;i<dataURL.numParts;i++) {
      loader->addContent(new GESTSVolumeContent(dataURL.where,i));
    }
  }
  
  size_t GESTSVolumeContent::projectedSize()
  {
    vec3i numVoxels(1024);
    return numVoxels.x*size_t(numVoxels.y)*numVoxels.z*sizeof(float);
  }
  
  void GESTSVolumeContent::executeLoad(DataRank &dataGroup, bool verbose)
  {
    vec3i numVoxels = vec3i(1024);
    size_t numScalars = 
      size_t(numVoxels.x)*size_t(numVoxels.y)*size_t(numVoxels.z);
    int fileID = thisPartID / 8;
    std::string fileName = filePrefix + std::to_string(fileID);
    int cubeID = thisPartID % 8;
    size_t cubeOffset = cubeID*sizeof(float)*numScalars;
    
    std::vector<uint8_t> rawData(numScalars*sizeof(float));
    char *dataPtr = (char *)rawData.data();

    std::ifstream in(fileName.c_str(),std::ios::binary);
    if (!in.good())
      throw std::runtime_error
        ("hs::GESTSVolumeContent: could not open '"+fileName+"'");
    in.seekg(cubeOffset);
    in.read(dataPtr,numScalars*sizeof(float));
    if (!in.good())
      throw std::runtime_error("read partial data...");
    
    vec3f gridOrigin;
    gridOrigin.x = 1024 * ((thisPartID) % 8);
    gridOrigin.y = 1024 * ((thisPartID / 8) % 8);
    gridOrigin.z = 1024 * ((thisPartID / 8 / 8) % 8);
    vec3f gridSpacing(1.f);

    std::vector<uint8_t> rawDataRGB;
    dataGroup.structuredVolumes.push_back
      (std::make_shared<StructuredVolume>(numVoxels,"float",rawData,rawDataRGB,
                                          gridOrigin,gridSpacing));
  }
  
  std::string GESTSVolumeContent::toString() 
  {
    std::stringstream ss;
    ss << "GESTSVolumeContext{#" << thisPartID << ",fileName="<<filePrefix<< "}";
    return ss.str();
  }

  
}
