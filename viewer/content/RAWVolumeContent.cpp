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

#include "RAWVolumeContent.h"

namespace hs {
  
  RAWVolumeContent::RAWVolumeContent(const std::string &fileName,
                                     int thisPartID,
                                     const box3i &cellRange,
                                     vec3i fullVolumeDims,
                                     ScalarType scalarType,
                                     int numChannels)
    : fileName(fileName),
      thisPartID(thisPartID),
      cellRange(cellRange),
      fullVolumeDims(fullVolumeDims),
      scalarType(scalarType),
      numChannels(numChannels)
  {}

  void splitKDTree(std::vector<box3i> &regions,
                   box3i cellRange,
                   int numParts)
  {
    if (numParts == 1) {
      regions.push_back(cellRange);
      return;
    }
    
    vec3i size = cellRange.size();
    int dim = arg_max(size);
    if (size[dim] < 2) {
      regions.push_back(cellRange);
      return;
    }

    int nRight = numParts/2;
    int nLeft  = numParts - nRight;
    
    box3i lBox = cellRange, rBox = cellRange;
    lBox.upper[dim]
      = rBox.lower[dim]
      = cellRange.lower[dim] + (cellRange.size()[dim]*nLeft)/numParts;
    splitKDTree(regions,lBox,nLeft);
    splitKDTree(regions,rBox,nRight);
  }
  
  void RAWVolumeContent::create(DataLoader *loader,
                                const ResourceSpecifier &dataURL)
  {
    std::string type = dataURL.get("type",dataURL.get("format",""));
    if (type.empty())
      throw std::runtime_error("RAWVolumeContent: 'type' not specified");
    ScalarType scalarType;
    if (type == "uint8" || type == "byte")
      scalarType = StructuredVolume::UINT8;
    else if (type == "float" || type == "f")
      scalarType = StructuredVolume::FLOAT;
    else
      throw std::runtime_error("RAWVolumeContent: invalid type '"+type+"'");

    int numChannels = dataURL.get_int("channels",1);
    
    std::string dimsString = dataURL.get("dims","");
    if (dimsString.empty())
      throw std::runtime_error("RAWVolumeContent: 'dims' not specified");

    vec3i dims;
    int n = sscanf(dimsString.c_str(),"%i,%i,%i",&dims.x,&dims.y,&dims.z);
    if (n != 3) throw std::runtime_error("RAWVolumeContent:: could not parse dims from '"+dimsString+"'");

    PRINT(dims);
    std::vector<box3i> regions;
    splitKDTree(regions,box3i(vec3i(0),dims-1),dataURL.numParts);
    if (regions.size() < dataURL.numParts)
      throw std::runtime_error("input data too small to split into indicated number of parts");

    std::cout << "RAW Volume: input data file of " << dims << " voxels will be read in the following bricks:" << std::endl;
    for (int i=0;i<regions.size();i++)
      std::cout << " #" << i << " : " << regions[i] << std::endl;
    for (int i=0;i<dataURL.numParts;i++) {
      loader->addContent(new RAWVolumeContent(dataURL.where,i,
                                              regions[i],
                                              dims,scalarType,
                                              numChannels));
    }
  }
  
  size_t RAWVolumeContent::projectedSize()
  {
    vec3i numVoxels = myCells.size()+1;
    return numVoxels.x*size_t(numVoxels.y)*numVoxels.z*numChannels*sizeOf(scalarType);
  }
  
  void   RAWVolumeContent::executeLoad(DataGroup &dataGroup, bool verbose)
  {
  }
  
  std::string RAWVolumeContent::toString() 
  {
    return "RAWVolumeContent{}";
  }

  
}
