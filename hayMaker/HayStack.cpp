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

#include "HayStack.h"

namespace hs {

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
    return bounds;
  }

  box3f SphereSet::getBounds() const
  {
    box3f bounds;
    for (int i=0;i<origins.size();i++) {
      float r = (i<radii.size())?radii[i]:radius;
      vec3f o = origins[i];
      bounds.extend({o-r,o+r});
    }
    return bounds;
  }

  box3f Cylinders::getBounds() const
  {
    box3f bounds;
    int   numCylinders = indices.empty() ? (points.size()/2) : indices.size();
    for (int i=0;i<numCylinders;i++) {
      vec2i idx = indices.empty() ? (vec2i(2*i)+vec2i(0,1)) : indices[i];
      float r = radii.empty()?radius:radii[i];
      vec3f a = points[idx.x];
      vec3f b = points[idx.y];
      bounds.extend(min(a,b)-r);
      bounds.extend(max(a,b)+r);
    }
    return bounds;
  }
  
  
}
