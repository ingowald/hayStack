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

  TSTriContent::TSTriContent(const ResourceSpecifier &data,
                             size_t fileSize,
                             int thisPartID)
    : data(data),
      fileSize(fileSize),
      thisPartID(thisPartID)
  {}
    
  void TSTriContent::create(DataLoader *loader,
                            const ResourceSpecifier &data)
  {
    for (int i=0;i<data.numParts;i++)
      loader->addContent
        (new TSTriContent(data,getFileSize(data.where),i));
  }
    
  size_t TSTriContent::projectedSize() 
  {
    size_t sizeOfTri = 3*sizeof(vec3f);
    
    size_t numTrisTotal = fileSize / sizeOfTri;
    numTrisTotal = data.get_size("count",numTrisTotal);

    int numPartsToSplitInto = data.numParts;
    size_t my_begin = (numTrisTotal * (thisPartID+0)) / numPartsToSplitInto;
    size_t my_end = (numTrisTotal * (thisPartID+1)) / numPartsToSplitInto;
    size_t my_count = my_end - my_begin;

    return 50*my_count;
    // return 100*my_count;
  }
    
  void   TSTriContent::executeLoad(DataRank &dataGroup, bool verbose) 
  {
    size_t sizeOfTri = 3*sizeof(vec3f);
    size_t numTrisTotal = fileSize / sizeOfTri;

    int numPartsToSplitInto = data.numParts;
    numTrisTotal = data.get_size("count",numTrisTotal);
    size_t my_begin = (numTrisTotal * (thisPartID+0)) / numPartsToSplitInto;
    size_t my_end = (numTrisTotal * (thisPartID+1)) / numPartsToSplitInto;
    size_t my_count = my_end - my_begin;

    mini::Mesh::SP mesh = mini::Mesh::create();
    mesh->indices.resize(my_count);
#if 1
    std::vector<std::pair<vec3f,int>> vertices;
    FILE *file = fopen(data.where.c_str(),"rb");
    fseek(file,my_begin*sizeOfTri,SEEK_SET);
    for (int i=0;i<my_count;i++) {
      // vec3f tri[3];
      // int rc = fread(tri,sizeOfTri,1,file);
      // assert(rc == 1);
      std::pair<vec3f,int> v;
      for (int j=0;j<3;j++) {
        v.second = vertices.size();
        int rc = fread(&v.first,sizeof(vec3f),1,file);
        assert(rc == 1);
        // vertices.push_back({tri[j],(int)vertices.size()});
        vertices.push_back(v);
      }
    }
    std::sort(vertices.begin(),vertices.end());
      std::cout.precision(15);
    // for (int i=0;i<100;i++)
    //   std::cout << " vtx " << i << " v " << vertices[i].first << " idx " << vertices[i].second << std::endl;
    int currentVertexID = -1;
    // PING;
    for (int i=0;i<vertices.size();i++) {
      if (i == 0 || vertices[i].first != vertices[i-1].first) {
        currentVertexID = mesh->vertices.size();
        mesh->vertices.push_back(vertices[i].first);
      }
      if (vertices[i].second >= mesh->indices.size()*3)
        throw std::runtime_error("invalid index");
      ((int*)mesh->indices.data())[vertices[i].second] = currentVertexID;
    }
    // for (int i=0;i<10;i++)
    //   PRINT(mesh->indices[i]);
    // for (int i=0;i<10;i++)
    //   PRINT(mesh->indices[mesh->indices.size()-1-i]);
    // PING;
    // PRINT(mesh->vertices.size());
    // PRINT(mesh->indices.size());
    vertices.clear();
#else
    mesh->vertices.resize(3*my_count);
    
    FILE *file = fopen(data.where.c_str(),"rb");
    fseek(file,my_begin*sizeOfTri,SEEK_SET);
    size_t numRead = fread(mesh->vertices.data(),sizeOfTri,my_count,file);
    assert(numRead == my_count);
    fclose(file);

    mesh->indices.resize(my_count);
    for (int i=0;i<my_count;i++)
      mesh->indices[i] = 3*i + vec3i(0,1,2);
#endif

    if (verbose) {
      std::cout << "   ... done loading " << prettyNumber(my_count)
                << " triangles from " << data.where << std::endl << std::flush;
      fflush(0);
    }

#if 0
    mini::DisneyMaterial::SP mat = std::make_shared<mini::DisneyMaterial>();;
    mat->metallic = .1f;
    mat->ior = 1.f;
    mat->roughness = .25f;
    mat->baseColor = .5f;
#else
    mini::Matte::SP mat = mini::Matte::create();
    mat->reflectance = 3.14f * vec3f(.8f);
#endif
    mesh->material = mat;
    

    mini::Object::SP object = mini::Object::create({mesh});
    mini::Scene::SP scene = mini::Scene::create({mini::Instance::create(object)});
    dataGroup.minis.push_back(scene);
  }
  
}
