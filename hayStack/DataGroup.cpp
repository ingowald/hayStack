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

#include "hayStack/DataGroup.h"

namespace hs {

  /*! this is an optimization in particular for models (like lander)
    where one rank might get multiple "smaller" unstructured
    meshes -- if each of these become their own volumes, with
    their own acceleration strcutre, etc, then that may have some
    negative side effects on performance */
  void DataGroup::mergeUnstructuredMeshes()
  {
    if (unsts.empty())
      // dont have any unstructured meshes - done.
      return;
    
    // umesh::UMesh::SP mergedMesh = unsts[0];
    // for (int i=1;i<unsts.size();i++)
    //   mergedMesh->append(unsts[i]);
    // unsts.clear();
    // unsts.push_back(mergedMesh);
    umesh::UMesh::SP merged = umesh::mergeMeshes(unsts);
    unsts.clear();
    unsts.push_back(merged);
  }
      
  BoundsData ThisRankData::getBounds() const
  {
    BoundsData bounds;
    for (auto &dg : dataGroups)
      bounds.extend(dg.getBounds());
    return bounds;
  }

  BoundsData DataGroup::getBounds() const
  {
    BoundsData bounds;
    for (auto mini : minis)
      if (mini)
        bounds.spatial.extend(mini->getBounds());
    for (auto unst : unsts)
      if (unst) {
        umesh::box3f bb = unst->getBounds();
        bounds.spatial.extend((const box3f&)bb);
        umesh::range1f sr = unst->getValueRange();
        bounds.scalars.extend((const range1f&)sr);
      }
    for (auto sphereSet : sphereSets)
      if (sphereSet)
        bounds.spatial.extend(sphereSet->getBounds());
    for (auto cylinderSet : cylinderSets)
      if (cylinderSet)
        bounds.spatial.extend(cylinderSet->getBounds());
    for (auto volume : structuredVolumes) {
      bounds.spatial.extend(volume->getBounds());
      bounds.scalars.extend(volume->getValueRange());
    }
    PING; PRINT(bounds.scalars);
    return bounds;
  }

} // ::hs
