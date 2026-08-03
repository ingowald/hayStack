// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayStack/loader/GESTS.h"
#include <fstream>
#include <umesh/UMesh.h>
#include <umesh/extractIsoSurface.h>
#include <miniScene/Scene.h>

namespace hs {
  namespace loader {
  
    GESTSVolumeContent::GESTSVolumeContent(const std::string &filePrefix,
                                           int thisPartID,
                                           int cubeDims,
                                           int cubesPerFile)
      : filePrefix(filePrefix),
        thisPartID(thisPartID),
        cubeDims(cubeDims),
        cubesPerFile(cubesPerFile)
    {}

    void GESTSVolumeContent::create(DataLoader *loader,
                                    const ResourceSpecifier &dataURL)
    {
      int cubesPerFile = dataURL.get_int("perFile",8);
      int cubeDims = dataURL.get_int("dims",8);
    
      for (int i=0;i<dataURL.numParts;i++) {
        loader->addContent(new GESTSVolumeContent
                           (dataURL.where,i,cubeDims,cubesPerFile));
      }
    }
  
    size_t GESTSVolumeContent::projectedSize()
    {
      vec3i numVoxels(1024);
      return numVoxels.x*size_t(numVoxels.y)*numVoxels.z*sizeof(float);
    }
  
    void GESTSVolumeContent::executeLoad(OnePartition &dataGroup)
    {
      vec3i numVoxels = vec3i(1024);
      size_t numScalars = 
        size_t(numVoxels.x)*size_t(numVoxels.y)*size_t(numVoxels.z);
      int fileID = thisPartID / cubesPerFile;
      std::string fileName = filePrefix + std::to_string(fileID);
      int cubeID = thisPartID % cubesPerFile;
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
      gridOrigin.x = 1024 * ((thisPartID) % cubeDims);
      gridOrigin.y = 1024 * ((thisPartID / cubeDims) % cubeDims);
      gridOrigin.z = 1024 * ((thisPartID / cubeDims / cubeDims) % cubeDims);
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
}
