// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayStack/loader/DataLoader.h"
#include "hayStack/loader/TSTris.h"
#include "hayStack/ColorMap.h"
#include "hayStack/TriangleMesh.h"

namespace hs {
  namespace loader {
    extern bool verbose;

    int ColorMappingMetaContent::currentIdx = 0;
    vec2f ColorMappingMetaContent::currentDomain = {0.f,0.f};

    TSTriContent::TSTriContent(const ResourceSpecifier &data,
                               size_t fileSize,
                               int thisPartID,
                               int default_mappedValues)
      : data(data),
        fileSize(fileSize),
        thisPartID(thisPartID),
        default_mappedValues(default_mappedValues)
    {}

    std::string TSTriContent::toString() 
    {
      return
        "Tim-Triangles{fileName="
        +data.where
        +", part "
        +std::to_string(thisPartID)
        +", proj size "
        +prettyNumber(projectedSize())+"B}";
    }
    
    void TSTriContent::create(DataLoader *loader,
                              const ResourceSpecifier &data,
                              int default_mappedValues)
    {
      for (int i=0;i<data.numParts;i++)
        loader->addContent
          (new TSTriContent(data,getFileSize(data.where),i,
                            default_mappedValues));
    }
    
    size_t TSTriContent::projectedSize() 
    {
      int mappedScalars = data.get_size("mapped",default_mappedValues);
      size_t sizeOfTri = 3*(sizeof(vec3f)+mappedScalars*sizeof(float));
    
      size_t numTrisTotal = fileSize / sizeOfTri;
      numTrisTotal = data.get_size("count",numTrisTotal);

      int numPartsToSplitInto = data.numParts;
      size_t my_begin = (numTrisTotal * (thisPartID+0)) / numPartsToSplitInto;
      size_t my_end = (numTrisTotal * (thisPartID+1)) / numPartsToSplitInto;
      size_t my_count = my_end - my_begin;

      return 50*my_count;
      // return 100*my_count;
    }
    
    void   TSTriContent::executeLoad(OnePartition &dataGroup) 
    {
      int mappedScalars = data.get_size("mapped",default_mappedValues);
      size_t sizeOfTri = 3*(sizeof(vec3f)+mappedScalars*sizeof(float));
      size_t numTrisTotal = fileSize / sizeOfTri;

      int numPartsToSplitInto = data.numParts;
      numTrisTotal = data.get_size("count",numTrisTotal);
      size_t my_begin = (numTrisTotal * (thisPartID+0)) / numPartsToSplitInto;
      size_t my_end = (numTrisTotal * (thisPartID+1)) / numPartsToSplitInto;
      size_t my_count = my_end - my_begin;

      TriangleMesh::SP mesh = TriangleMesh::create();
      mesh->indices.resize(my_count);
      std::vector<vec4f> xf;
      vec2f domain = data.get_vec2f("map_domain",ColorMappingMetaContent::currentDomain);
      if (mappedScalars > 0) {
        ColorMap::init();
        int mapIdx = data.get_int("map",ColorMappingMetaContent::currentIdx);
        xf = ColorMap::get(mapIdx);
      }
      const auto &mapScalar = [&](float s)
      {
        if (domain.x != domain.y) {
          s = std::max(s,domain.x);
          s = std::min(s,domain.y);
          s = (s - domain.x) / (domain.y-domain.x);
        }
        s = std::max(0.f,s);
        s = std::min(1.f,s);
        s = s * (xf.size()-1);
        int idx = int(s);
        float f = s - idx;
        idx = std::max(0,std::min(int(xf.size()-2),idx));
        vec4f mapped = (1.f-f)*xf[idx]+f*xf[idx+1];
        return (const vec3f&)mapped;
      };

      if (my_count == 0) return;
      
      mesh->vertices.resize(3*my_count);
      if (mappedScalars > 0)
        mesh->colors.resize(3*my_count);
      
      FILE *file = fopen(data.where.c_str(),"rb");
      fseek(file,my_begin*sizeOfTri,SEEK_SET);
      for (int i=0;i<my_count;i++) {
        for (int j=0;j<3;j++) {
          fread(&mesh->vertices[3*i+j],sizeof(vec3f),1,file);
          switch(mappedScalars) {
          case 0:
            break;
          case 1: {
            float scalar;
            fread(&scalar,sizeof(float),1,file);
            mesh->colors[3*i+j] = mapScalar(scalar);
          } break;
          case 3:
            fread(&mesh->colors[3*i+j],sizeof(vec3f),1,file);
            break;
          default: throw std::runtime_error("invalid num mapped scalars");
          }
        }
      }
      size_t numRead = fread(mesh->vertices.data(),sizeOfTri,my_count,file);
      assert(numRead == my_count);
      fclose(file);

      mesh->indices.resize(my_count);
      for (int i=0;i<my_count;i++)
        mesh->indices[i] = 3*i + vec3i(0,1,2);

      if (verbose) {
        std::cout << "   ... done loading " << prettyNumber(my_count)
                  << " triangles from " << data.where << std::endl << std::flush;
        fflush(0);
      }

      mini::Matte::SP mat = mini::Matte::create();
      mat->reflectance = 3.14f * vec3f(.8f);
      mesh->material = mat;
    
      // mini::Object::SP object = mini::Object::create({mesh});
      // mini::Scene::SP scene = mini::Scene::create({mini::Instance::create(object)});
      // dataGroup.minis.push_back(scene);
      dataGroup.triangleMeshes.push_back(mesh);
    }
  
  }
}
