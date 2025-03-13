// ======================================================================== //
// Copyright 2022-2024 Ingo Wald                                            //
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

#include "hayStack/HayStack.h"
#include "hayStack/Spheres.h"
#include "hayStack/Cylinders.h"
#include "hayStack/TriangleMesh.h"
#include "hayStack/Capsules.h"
#include "hayStack/StructuredVolume.h"
#include <miniScene/Scene.h>
#include <umesh/UMesh.h>

namespace hs {

  /*! smallest entity in which the entirety of the data to be rendered
      is goind to be split into. for multi-gpu data parallel multi-gpu
      rendering a single application process (or given mpi rank) could
      still have multiple such data groups */
  struct DataRank {
    /*! this is an optimization in particular for models (like lander)
        where one rank might get multiple "smaller" unstructured
        meshes -- if each of these become their own volumes, with
        their own acceleration strcutre, etc, then that may have some
        negative side effects on performance */
    void mergeUnstructuredMeshes();

    DataRank() {
      defaultMaterial = mini::DisneyMaterial::create();
    };
    BoundsData getBounds() const;
    
    mini::Material::SP                defaultMaterial;
    struct {
      std::vector<mini::DirLight>     directional;
    }                                 sharedLights;
    std::vector<mini::Scene::SP>      minis;
    /*! mesh AND domain. domain being empty means 'no clip box' */
    std::vector<std::pair<umesh::UMesh::SP,box3f>> unsts;
    std::vector<TriangleMesh::SP>     triangleMeshes;
    std::vector<SphereSet::SP>        sphereSets;
    std::vector<Cylinders::SP>        cylinderSets;
    std::vector<Capsules::SP>         capsuleSets;
    std::vector<StructuredVolume::SP> structuredVolumes;
    int                               dataGroupID = -1;
  };

} // ::hs
