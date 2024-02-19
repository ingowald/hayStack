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
#include <fstream>
#include <umesh/UMesh.h>
// #include <umesh/tetrahedralize.h>
#include <umesh/extractIsoSurface.h>
#include <miniScene/Scene.h>

namespace umesh {
  UMesh::SP tetrahedralize(UMesh::SP in,
                           int ownedTets,
                           int ownedPyrs,
                           int ownedWedges,
                           int ownedHexes);
}  
namespace hs {
  
  RAWVolumeContent::RAWVolumeContent(const std::string &fileName,
                                     int thisPartID,
                                     const box3i &cellRange,
                                     vec3i fullVolumeDims,
                                     BNTexelFormat texelFormat,
                                     int numChannels,
                                     float isoValue)
    : fileName(fileName),
      thisPartID(thisPartID),
      cellRange(cellRange),
      fullVolumeDims(fullVolumeDims),
      texelFormat(texelFormat),
      numChannels(numChannels),
      isoValue(isoValue)
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
    BNTexelFormat texelFormat;
    if (type == "uint8" || type == "byte")
      texelFormat = BN_TEXEL_FORMAT_R8; //scalarType = StructuredVolume::UINT8;
    else if (type == "float" || type == "f")
      texelFormat = BN_TEXEL_FORMAT_R32F; //scalarType = StructuredVolume::FLOAT;
    else if (type == "uint16")
      texelFormat = BN_TEXEL_FORMAT_R16; //scalarType = StructuredVolume::UINT16;
    else
      throw std::runtime_error("RAWVolumeContent: invalid type '"+type+"'");

    int numChannels = dataURL.get_int("channels",1);
    
    std::string dimsString = dataURL.get("dims","");
    if (dimsString.empty())
      throw std::runtime_error("RAWVolumeContent: 'dims' not specified");

    vec3i dims;
    int n = sscanf(dimsString.c_str(),"%i,%i,%i",&dims.x,&dims.y,&dims.z);
    if (n != 3) throw std::runtime_error("RAWVolumeContent:: could not parse dims from '"+dimsString+"'");

    box3i initRegion = { vec3i(0), dims-1 };
    std::string extractString = dataURL.get("extract");
    if (!extractString.empty()) {
      vec3i lower, size;
      int n = sscanf(extractString.c_str(),"%i,%i,%i,%i,%i,%i",
                     &lower.x,&lower.y,&lower.z,
                     &size.x,&size.y,&size.z);
      if (n != 6)
        throw std::runtime_error("RAWVolumeContent:: could not parse 'extract' value from '"
                                 +extractString
                                 +"' (should be 'f,f,f,f,f,f' format)");
      initRegion.lower = lower;
      initRegion.upper = lower+size-1;
    }

    std::vector<box3i> regions;
    splitKDTree(regions,initRegion,dataURL.numParts);
    // splitKDTree(regions,box3i(vec3i(0),dims-1),dataURL.numParts);
    if (regions.size() < dataURL.numParts)
      throw std::runtime_error("input data too small to split into indicated number of parts");

    if (loader->myRank() == 0) {
      std::cout << "RAW Volume: input data file of " << dims << " voxels will be read in the following bricks:" << std::endl;
      for (int i=0;i<regions.size();i++)
        std::cout << " #" << i << " : " << regions[i] << std::endl;
    }
    float isoValue = NAN;
    std::string isoString = dataURL.get("iso",dataURL.get("isoValue"));
    if (!isoString.empty())
      isoValue = std::stof(isoString);
    
    for (int i=0;i<dataURL.numParts;i++) {
      loader->addContent(new RAWVolumeContent(dataURL.where,i,
                                              regions[i],
                                              dims,texelFormat,//scalarType,
                                              numChannels,
                                              isoValue));
    }
  }
  
  size_t RAWVolumeContent::projectedSize()
  {
    vec3i numVoxels = cellRange.size()+1;
    return numVoxels.x*size_t(numVoxels.y)*numVoxels.z*numChannels*sizeOf(texelFormat);
  }
  
  void RAWVolumeContent::executeLoad(DataGroup &dataGroup, bool verbose)
  {
    vec3i numVoxels = (cellRange.size()+1);
    size_t numScalars = numChannels*size_t(numVoxels.x)*size_t(numVoxels.y)*size_t(numVoxels.z);
    std::vector<uint8_t> rawData(numScalars*sizeOf(texelFormat));
    char *dataPtr = (char *)rawData.data();
    std::ifstream in(fileName.c_str(),std::ios::binary);
    size_t texelSize = sizeOf(texelFormat);
    for (int c=0;c<numChannels;c++) {
      for (int iz=cellRange.lower.z;iz<=cellRange.upper.z;iz++)
        for (int iy=cellRange.lower.y;iy<=cellRange.upper.y;iy++) {
          int ix = cellRange.lower.x;
          size_t ofsInScalars
            = ix
            + iy*size_t(fullVolumeDims.x)
            + iz*size_t(fullVolumeDims.x)*size_t(fullVolumeDims.y)
            + c *size_t(fullVolumeDims.x)*size_t(fullVolumeDims.y)*size_t(fullVolumeDims.z);
          in.seekg(ofsInScalars*texelSize);
          in.read(dataPtr,numVoxels.x*texelSize);
          if (!in.good())
            throw std::runtime_error("read partial data...");
          dataPtr += numVoxels.x*texelSize;
        }
    }
    vec3f gridOrigin(cellRange.lower);
    vec3f gridSpacing(1.f);

    bool doIso = !isnan(isoValue);
    if (doIso) {
      umesh::UMesh::SP
        volume = std::make_shared<umesh::UMesh>();
      volume->perVertex = std::make_shared<umesh::Attribute>();
      
      for (int iz=0;iz<numVoxels.z;iz++)
        for (int iy=0;iy<numVoxels.y;iy++)
          for (int ix=0;ix<numVoxels.x;ix++) {
            volume->vertices.push_back(umesh::vec3f(umesh::vec3i(ix,iy,iz))*(const umesh::vec3f&)gridSpacing+(const umesh::vec3f&)gridOrigin);
            size_t idx = ix+size_t(numVoxels.x)*(iy+size_t(numVoxels.y)*iz);
            float scalar;
            switch(texelFormat) {
            case BN_TEXEL_FORMAT_R32F:
              scalar = ((const float*)rawData.data())[idx];
              break;
            case BN_TEXEL_FORMAT_R16:
              scalar = ((const uint16_t*)rawData.data())[idx]*(1.f/((1<<16)-1));
              break;
            case BN_TEXEL_FORMAT_R8:
              scalar = ((const uint8_t*)rawData.data())[idx]*(1.f/((1<<8)-1));
              break;
            default:throw std::runtime_error("not implemented...");
            };
            
              // = ( == StructuredVolume::FLOAT)
              // ? ((const float*)rawData.data())[idx]
              // : (1.f/255.f*((const uint8_t*)rawData.data())[idx]);
              
            volume->perVertex->values.push_back(scalar);
          }
      if (size_t(numVoxels.x)*size_t(numVoxels.y)*size_t(numVoxels.z) > (1ull<<30))
        throw std::runtime_error("volume dims too large to extract iso-surface via umesh");
      volume->finalize();
      for (int iz=0;iz<numVoxels.z-1;iz++)
        for (int iy=0;iy<numVoxels.y-1;iy++)
          for (int ix=0;ix<numVoxels.x-1;ix++) {
            umesh::Hex hex;
            int i000 = (ix+0)+int(numVoxels.x)*((iy+0)+int(numVoxels.y)*(iz+0));
            int i001 = (ix+1)+int(numVoxels.x)*((iy+0)+int(numVoxels.y)*(iz+0));
            int i010 = (ix+0)+int(numVoxels.x)*((iy+1)+int(numVoxels.y)*(iz+0));
            int i011 = (ix+1)+int(numVoxels.x)*((iy+1)+int(numVoxels.y)*(iz+0));
            int i100 = (ix+0)+int(numVoxels.x)*((iy+0)+int(numVoxels.y)*(iz+1));
            int i101 = (ix+1)+int(numVoxels.x)*((iy+0)+int(numVoxels.y)*(iz+1));
            int i110 = (ix+0)+int(numVoxels.x)*((iy+1)+int(numVoxels.y)*(iz+1));
            int i111 = (ix+1)+int(numVoxels.x)*((iy+1)+int(numVoxels.y)*(iz+1));
            hex.base = { i000,i001,i011,i010 };
            hex.top  = { i100,i101,i111,i110 };
            volume->hexes.push_back(hex);
          }

      // volume = umesh::tetrahedralize(volume,0,0,0,volume->hexes.size());
      // PRINT(volume->toString());
      umesh::UMesh::SP surf = umesh::extractIsoSurface(volume,isoValue);
      surf->finalize();
      PRINT(surf->toString());

      mini::Mesh::SP mesh = mini::Mesh::create();
      for (auto vtx : surf->vertices)
        mesh->vertices.push_back((const vec3f&)vtx);
      for (auto idx : surf->triangles)
        mesh->indices.push_back((const vec3i&)idx);
      mini::Object::SP obj = mini::Object::create({mesh});
      mini::Instance::SP inst = mini::Instance::create(obj);
      mini::Scene::SP model = mini::Scene::create({inst});
      
      if (!mesh->indices.empty())
        dataGroup.minis.push_back(model);
    } else {
      dataGroup.structuredVolumes.push_back
        (std::make_shared<StructuredVolume>(numVoxels,texelFormat,rawData,
                                            gridOrigin,gridSpacing));
    }
  }
  
  std::string RAWVolumeContent::toString() 
  {
    std::stringstream ss;
    ss << "RAWVolumeContext{#" << thisPartID << ",fileName="<<fileName<<",cellRange="<<cellRange<< "}";
    return ss.str();
  }

  
}
