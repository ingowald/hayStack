// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayStack/loader/IsoDump.h"

namespace hs {
  namespace loader {
    extern bool verbose;
    
    IsoDumpContent::IsoDumpContent(const ResourceSpecifier &data,
                                   int thisPartID)
      : data(data),
        thisPartID(thisPartID)
    {}
    
    void IsoDumpContent::create(DataLoader *loader,
                                const ResourceSpecifier &data)
    {
      // cannot do auto-splitting on these files
      for (int i=0;i<data.numParts;i++)
        loader->addContent
          (new IsoDumpContent(data,i));
    }
    
    size_t IsoDumpContent::projectedSize() 
    {
      std::string src = data.where+std::to_string(thisPartID);
      size_t fileSizes = 
        + getFileSize(src+".vertex_coords.f3")
        + getFileSize(src+".vertex_scalars.f1")
        + getFileSize(src+".triangle_indices.i3");
      return 4*fileSizes;
    }

    // from dlafToPCR.cpp: 
    inline vec3f hue_to_rgb(float hue)
    {
      float s = saturate( hue ) * 6.0f;
      float r = saturate( fabsf(s - 3.f) - 1.0f );
      float g = saturate( 2.0f - fabsf(s - 2.0f) );
      float b = saturate( 2.0f - fabsf(s - 4.0f) );
      return vec3f(r, g, b);
    }

    inline vec3f temperature_to_rgb(float t)
    {
      float K = 4.0f / 6.0f;
      float h = K - K * t;
      float v = .5f + 0.5f * t;
      return v * hue_to_rgb(h);
    }


    vec3f colorMap(float f)
    {
      return temperature_to_rgb(f);
    }

    void   IsoDumpContent::executeLoad(OnePartition &dataGroup) 
    {
      range1f colorMapRange = {0.f,1.f};
      std::string colorMapRangeString = data.get("map_from","0.,1.");
      PING; PRINT(colorMapRangeString);
      int rc = sscanf(colorMapRangeString.c_str(),
                      "%f,%f",
                      &colorMapRange.lower,
                      &colorMapRange.upper);
      if (rc != 2) throw std::runtime_error("IsoDumpContent: could not parse 'map_from' parameter");

      hs::TriangleMesh::SP mesh = hs::TriangleMesh::create();
      std::string src = data.where+std::to_string(thisPartID);
      mesh->vertices
        = noHeader::loadVectorOf<vec3f>(src+".vertex_coords.f3");
      mesh->indices
        = noHeader::loadVectorOf<vec3i>(src+".triangle_indices.i3");
      std::vector<float> scalars
        = noHeader::loadVectorOf<float>(src+".vertex_scalars.f1");
      for (auto f : scalars) {
        f = (f - colorMapRange.lower)
          / (colorMapRange.upper - colorMapRange.lower);
        f = std::max(std::min(f,1.f),0.f);
        mesh->colors.push_back(colorMap(f));
      }
      if (verbose) {
        std::cout << "   ... done loading " << prettyNumber(mesh->indices.size())
                  << " triangles from " << data.where << std::endl << std::flush;
        fflush(0);
      }
    
      mini::Matte::SP mat = mini::Matte::create();
      mat->reflectance = 3.14f * vec3f(.8f);
      mesh->material = mat;
    
      dataGroup.triangleMeshes.push_back(mesh);
    }
  
  }
}
