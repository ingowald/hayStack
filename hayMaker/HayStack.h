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

/*! a hay-*stack* is a description of data-parallel data */

#pragma once

#include "miniScene/Scene.h"
#include "umesh/UMesh.h"

namespace hs {
  using namespace mini;
  
  struct SphereSet {
    typedef std::shared_ptr<SphereSet> SP;

    static SP create() { return std::make_shared<SphereSet>(); }
    box3f getBounds() const;
    
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
    box3f getBounds() const;
    
    std::vector<mini::Scene::SP>  minis;
    std::vector<umesh::UMesh::SP> unsts;
    std::vector<SphereSet::SP>    sphereSets;
    int                           dataGroupID = -1;
  };

  /*! data for one mpi rank - each mpi rank can still have multiple
      data groups (for local multi-gpu data parallel rendering, for
      example - so it has multiple data groups */
  struct ThisRankData {
    box3f getBounds() const;

    /*! returns whether this rank does *not* have any data; in this
        case it's a passive (head?-)node */
    bool empty() const { return dataGroups.empty(); }
    
    void resize(int numDataGroups)
    { dataGroups.resize(numDataGroups); }

    /*! returns the number of data groups *on this rank* */
    int size() const { return dataGroups.size(); }
    
    std::vector<DataGroup> dataGroups;
  };

 }
