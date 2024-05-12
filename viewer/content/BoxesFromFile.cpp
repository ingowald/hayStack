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

#include "viewer/DataLoader.h"
#include "viewer/content/BoxesFromFile.h"

namespace hs {
  
  BoxesFromFile::BoxesFromFile(const ResourceSpecifier &data,
                               int thisPartID)
    : data(data),
      fileSize(getFileSize(data.where)),
      thisPartID(thisPartID)
  {}

  std::string BoxesFromFile::toString() 
  {
    return "Boxes{fileName="+data.where
      +", part "+std::to_string(thisPartID)+" of "
      + std::to_string(data.numParts)+", proj size "
      +prettyNumber(projectedSize())+"B}";
  }

  
  void BoxesFromFile::create(DataLoader *loader,
                             const ResourceSpecifier &dataURL)
  {
    for (int i=0;i<dataURL.numParts;i++)
      loader->addContent(new BoxesFromFile(dataURL,i));
  }
    
  size_t BoxesFromFile::projectedSize() 
  { return (100/12) * divRoundUp(fileSize, (size_t)data.numParts); }
    
  void   BoxesFromFile::executeLoad(DataRank &dataGroup, bool verbose) 
  {
    std::vector<box3f> boxes;
    FILE *file = fopen(data.where.c_str(),"rb");
    assert(file);
    size_t sizeOfBox = sizeof(box3f);
    int64_t numBoxesInFile = fileSize / sizeOfBox;
    int64_t numBoxesToLoad
      = std::min((int64_t)data.get_size("count",numBoxesInFile),
                 (int64_t)(numBoxesInFile-data.get_size("begin",0)));
    if (numBoxesToLoad <= 0)
      throw std::runtime_error("no boxes to load for these begin/count values!?");
  
    size_t my_begin
      = data.get_size("begin",0)
      + (numBoxesToLoad * (thisPartID+0)) / data.numParts;
    size_t my_end
      = data.get_size("begin",0)
      + (numBoxesToLoad * (thisPartID+1)) / data.numParts;
    size_t my_count = my_end - my_begin;
    // boxes->origins.resize(my_count);
    boxes.resize(my_count);
    fseek(file,my_begin*sizeOfBox,SEEK_SET);
    // const std::string format = data.get("format","xyz");
    // if (format =="xyzf") {
    //   vec4f v;
    //   for (size_t i=0;i<my_count;i++) {
    //     int rc = fread((char*)&v,sizeof(v),1,file);
    //     assert(rc);
    //     boxes->origins.push_back(vec3f{v.x,v.y,v.z});
    //   }
    // } else if (format == "xyz") {
    //   vec3f v;
    //   for (size_t i=0;i<my_count;i++) {
    //     int rc = fread((char*)&v,sizeof(v),1,file);
    //     assert(rc);
    //     boxes->origins.push_back(vec3f{v.x,v.y,v.z});
    //   }
    // } else
    //   throw std::runtime_error("un-recognized boxes format '"+format+"'");

    std::map<vec3f,int> vertexIDs;
    mini::Mesh::SP mesh = mini::Mesh::create();

    int unitBoxIndices[]
      = {
      0,1,3, 2,0,3,
      5,7,6, 5,6,4,
      0,4,5, 0,5,1,
      2,3,7, 2,7,6,
      1,5,7, 1,7,3,
      4,0,2, 4,2,6
    };
    
    int rc = fread((char*)boxes.data(),sizeof(boxes[0]),my_count,file);
    assert(rc);
    
    auto getVertex = [&](vec3f v) -> int {
#if 0
      int ID = mesh->vertices.size();
      mesh->vertices.push_back(v);
      return ID;
#else
      auto it = vertexIDs.find(v);
      if (it == vertexIDs.end()) {
        int ID = mesh->vertices.size();
        mesh->vertices.push_back(v);
        vertexIDs[v] = ID;
        return ID;
      }
      return it->second;
#endif
    };
    for (size_t j=0;j<my_count;j++) {
      box3f box = boxes[j];
      int boxIndices[8];
      for (int iz=0;iz<2;iz++)
        for (int iy=0;iy<2;iy++)
          for (int ix=0;ix<2;ix++) {
            vec3f v(ix ? box.upper.x : box.lower.x,
                    iy ? box.upper.y : box.lower.y,
                    iz ? box.upper.z : box.lower.z);
            boxIndices[4*iz+2*iy+ix] = getVertex(v);
          }
      for (int i=0;i<12;i++)
        mesh->indices.push_back(vec3i(boxIndices[unitBoxIndices[3*i+0]],
                                      boxIndices[unitBoxIndices[3*i+1]],
                                      boxIndices[unitBoxIndices[3*i+2]]));
    }
    if (verbose) {
      std::cout << "   ... done loading " << prettyNumber(my_count)
                << " boxes from " << data.where << std::endl << std::flush;
      fflush(0);
    }
    fclose(file);

    mini::Object::SP
      obj      = mini::Object::create({mesh});
    mini::Instance::SP
      instance = mini::Instance::create(obj);
    mini::Scene::SP
      scene    = mini::Scene::create({instance});
    dataGroup.minis.push_back(scene);
  }
  
}
