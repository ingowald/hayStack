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

#pragma once

#include "miniScene/Scene.h"
#include "umesh/UMesh.h"

namespace hs {
  using namespace mini;
  
  struct SphereSet {
    typedef std::shared_ptr<SphereSet> SP;

    static SP create() { return std::make_shared<SphereSet>(); }
    
    std::vector<vec3f> origins;
    
    /*! array of radii - can be empty (in which case `radius` applies
        for all spheres equally), but if non-empty it has to be the
        same size as `origins` */
    std::vector<float> radii;
    
    float radius = .1f;
  };

  /*! smallest entity in which the entirety of the data to be rendered
      is goind to be split into. for multi-gpu data parallel multi-gpu
      rendering a single application process (or given mpi rank) could
      still have multiple such data groups */
  struct DataGroup {
    std::vector<mini::Scene::SP>  geometry;
    std::vector<umesh::UMesh::SP> unsts;
    std::vector<SphereSet::SP>    sphereSets;
    int                           dataGroupID = -1;
  };

  /*! data for one mpi rank - each mpi rank can still have multiple
      data groups (for local multi-gpu data parallel rendering, for
      example - so it has multiple data groups */
  struct ThisRankData {
    ThisRankData(int dataPerRank)
      : dataGroups(dataPerRank)
    {}
    std::vector<DataGroup> dataGroups;
  };

  struct BarnConfig {
    typedef enum { WORKER, HEAD_NODE, DISPLAY } Role;
    bool  hasHeadNode   = false;
    Role  role          = WORKER;
    vec2i fbSize        = vec2i{ 800, 600 };
    int   workerRank    = 0;
    int   numWorkers    = 1;

    /*! how many (mpi?-)ranks there are. this will be '1' for local
        non-mpi mode, and total number of mpi ranks (workers plus head
        node, if applicable) for mpi mode */
    int   numRanks      = 1;
    
    /*! how many different data groups there are, across all ranks */
    int   numDataGroups = 1;
  };
  
  struct HayStack {
    HayStack(BarnConfig *config,
             const ThisRankData &thisRankData);
    
    BarnConfig *config;
    const ThisRankData thisRankData;
  };
  
}
