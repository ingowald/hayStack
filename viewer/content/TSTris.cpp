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
#include "viewer/content/TSTris.h"

namespace hs {

  TSTriContent::TSTriContent(const std::string &fileName,
                                   size_t fileSize,
                                   int thisPartID,
                                   int numPartsToSplitInto)
    : fileName(fileName),
      fileSize(fileSize),
      thisPartID(thisPartID),
      numPartsToSplitInto(numPartsToSplitInto)
  {}
    
  void TSTriContent::create(DataLoader *loader,
                            const ResourceSpecifier &data)
  {
    for (int i=0;i<data.numParts;i++)
      loader->addContent
        (new TSTriContent(data.where,getFileSize(data.where),i,
                          data.numParts));
  }
    
  size_t TSTriContent::projectedSize() 
  {
    int numTriangles = divRoundUp(divRoundUp(fileSize,3*sizeof(vec3f)),(size_t)numPartsToSplitInto);
    return 100*numTriangles;
  }
    
  void   TSTriContent::executeLoad(DataGroup &dataGroup, bool verbose) 
  {
    size_t sizeOfTri = 3*sizeof(vec3f);
    size_t numTrisTotal = fileSize / sizeOfTri;
    size_t my_begin = (numTrisTotal * (thisPartID+0)) / numPartsToSplitInto;
    size_t my_end = (numTrisTotal * (thisPartID+1)) / numPartsToSplitInto;
    size_t my_count = my_end - my_begin;

    mini::Mesh::SP mesh = mini::Mesh::create();
    mesh->vertices.resize(3*my_count);
    
    FILE *file = fopen(fileName.c_str(),"rb");
    fseek(file,my_begin*sizeOfTri,SEEK_SET);
    size_t numRead = fread(mesh->vertices.data(),sizeOfTri,my_count,file);
    assert(numRead == my_count);
    fclose(file);

    if (verbose) {
      std::cout << "   ... done loading " << prettyNumber(my_count)
                << " triangles from " << fileName << std::endl << std::flush;
      fflush(0);
    }
    mesh->indices.resize(my_count);
    for (int i=0;i<my_count;i++)
      mesh->indices[i] = 3*i + vec3i(0,1,2);

    mini::Object::SP object = mini::Object::create({mesh});
    mini::Scene::SP scene = mini::Scene::create({mini::Instance::create(object)});
    dataGroup.minis.push_back(scene);
  }
  
}
