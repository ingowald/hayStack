// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayStack/loader/DataLoader.h"
#include "hayStack/loader/TSTris.h"
#include "hayStack/loader/ColorMapper.h"
#include "hayStack/ColorMap.h"
#include "hayStack/TriangleMesh.h"

namespace hs {
  namespace loader {
    extern bool verbose;

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
      /* number of scalar values for each sphere, first three are
         always x,y,z position */
      int stride = data.get_int("stride",3);

      size_t sizeOfTriangle = 3 * stride * sizeof(float);
      size_t skipBytes = data.get_size("skip",0);
      size_t numTrianglesInFile = (fileSize-skipBytes) / sizeOfTriangle;
      // ------------------------------------------------------------------
      // compute *full* [begin,end] range of all particles in the file
      // that *all* parts of this loader together are supposed to read
      // ------------------------------------------------------------------
      size_t begin = data.get_size("begin",0);
      size_t count = std::min(data.get_size("count",0),
                              numTrianglesInFile-begin);
      int64_t end = std::min(numTrianglesInFile,
                             data.get_size("end",
                                           count
                                           ? begin+count
                                           : numTrianglesInFile));

      return (end-begin)*/* "some" estimate of cost of one triangle...*/50;
    }
    
    void   TSTriContent::executeLoad(OnePartition &dataGroup) 
    {
      TriangleMesh::SP mesh = TriangleMesh::create();
      // in dataloader.cpp, in case we need to add more material params:
      mesh->material = parseAnariMaterial(data);

      /* number of scalar values for each sphere, first three are
         always x,y,z position */
      int stride = data.get_int("stride",3);
      
      /* index of scalar to be used for color mapping; '-1' means 'no
         color mapping' */
      int sidx   = data.get_int("sidx",-1);
      assert(sidx < stride);

      /* index of rgb 'c'olor to be used for color mapping; '-1' means 'no
         color in particle' */
      int cidx   = data.get_int("cidx",-1);

      FILE *file = fopen(data.where.c_str(),"rb");
      ColorMapper colorMapper(data);
      if (sidx >= 0 && !colorMapper.valid())
        throw std::runtime_error
          ("triangles: requested scalar mapping, but no 'map' range specified");

      size_t sizeOfTriangle = 3 * stride * sizeof(float);
      size_t skipBytes = data.get_size("skip",0);
      size_t numTrianglesInFile = (fileSize-skipBytes) / sizeOfTriangle;
      // ------------------------------------------------------------------
      // compute *full* [begin,end] range of all particles in the file
      // that *all* parts of this loader together are supposed to read
      // ------------------------------------------------------------------
      size_t begin = data.get_size("begin",0);
      size_t count = std::min(data.get_size("count",0),
                              numTrianglesInFile-begin);
      int64_t end = std::min(numTrianglesInFile,
                             data.get_size("end",
                                           count
                                           ? begin+count
                                           : numTrianglesInFile));

      // ------------------------------------------------------------------
      // now compute *this* part's range of particles to read
      // ------------------------------------------------------------------
      size_t numTrianglesToLoad = end-begin;
      size_t my_begin
        = begin//data.get_size("begin",0)
        + (numTrianglesToLoad * (thisPartID+0)) / data.numParts;
      size_t my_end
        = begin//data.get_size("end",0)
        + (numTrianglesToLoad * (thisPartID+1)) / data.numParts;
      size_t my_count = my_end-my_begin;
      // ------------------------------------------------------------------
      // now: read ...
      // ------------------------------------------------------------------
      if (verbose) {
        std::cout << "   ... done loading " << prettyNumber(my_count)
                  << " triangles from " << data.where << std::endl << std::flush;
        fflush(0);
      }

      fseek(file,skipBytes+my_begin*sizeOfTriangle,SEEK_SET);

      std::vector<float> scalars(stride);
      range1f inputRange;
      for (size_t triID=my_begin;triID<my_end;triID++) {
        for (int vtx=0;vtx<3;vtx++) {
          int rc = fread((char*)scalars.data(),scalars.size()*sizeof(float),1,file);
          assert(rc);
          vec3f pos(scalars[0],scalars[1],scalars[2]);
          mesh->vertices.push_back(pos);
        
          if (sidx >= 0) {
            inputRange.extend(scalars[sidx]);
            // just for informational purposes:
            mesh->colors.push_back
              (colorMapper.map3f(scalars[sidx]));
          } else if (cidx >= 0)
            mesh->colors.push_back
              (vec3f(scalars[cidx],scalars[cidx+1],scalars[cidx+2]));
          else
            /* no color mapping */
            ;
        }
      }
      
      if (my_count >= (1ull<<31))
        throw std::runtime_error
          ("triangles: index overrun - too many triangles in a single file");

      mesh->indices.resize(my_count);
      for (int i=0;i<(int)my_count;i++)
        mesh->indices[i] = 3*i + vec3i(0,1,2);

      mini::ANARIMaterial *mm = (mini::ANARIMaterial *)mesh->material.get();
      dataGroup.triangleMeshes.push_back(mesh);
    }
  
  }
}
