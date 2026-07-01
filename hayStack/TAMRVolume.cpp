// ======================================================================== //
// Copyright 2025++ Ingo Wald                                               //
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

#include "TAMRVolume.h"

namespace hs {

  namespace {
    static vec3f blockDebugColor(int level)
    {
      static const vec3f colors[] = {
        {1.f, .2f, .2f},
        { .2f, 1.f, .2f},
        { .2f, .4f, 1.f},
        {1.f, 1.f, .2f},
        {1.f, .2f, 1.f},
        { .2f, 1.f, 1.f},
      };
      return colors[level % (sizeof(colors)/sizeof(colors[0]))];
    }

    static void addBoxWireframe(Cylinders::SP cs,
                                const box3f &bb,
                                float radius,
                                const vec3f &color)
    {
      const vec3f lo = bb.lower;
      const vec3f hi = bb.upper;
      const vec3f p[8] = {
        {lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z},
        {lo.x, hi.y, lo.z}, {hi.x, hi.y, lo.z},
        {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z},
        {lo.x, hi.y, hi.z}, {hi.x, hi.y, hi.z},
      };
      static const int edges[12][2] = {
        {0,1}, {2,3}, {0,2}, {1,3},
        {4,5}, {6,7}, {4,6}, {5,7},
        {0,4}, {1,5}, {2,6}, {3,7},
      };
      for (int i = 0; i < 12; ++i) {
        cs->vertices.push_back(p[edges[i][0]]);
        cs->vertices.push_back(p[edges[i][1]]);
        cs->colors.push_back(color);
        cs->colors.push_back(color);
      }
      cs->radius = radius;
    }
  }

  Cylinders::SP TAMRVolume::createBlockDebugCylinders(const tamr::Model::SP &model)
  {
    Cylinders::SP cs = Cylinders::create();
    box3f sceneBounds;
    for (auto grid : model->grids) {
      const float cellSize = powf(2.f, -(float)grid.level);
      const vec3i org = (const vec3i &)grid.origin;
      const vec3i dim = (const vec3i &)grid.dims;
      box3f bb;
      bb.lower = cellSize * (vec3f(org) - .5f);
      bb.upper = cellSize * (vec3f(org + dim) + .5f);
      sceneBounds.extend(bb.lower);
      sceneBounds.extend(bb.upper);
    }
    const float radius = std::max(length(sceneBounds.size()) * 0.002f, 1e-4f);
    cs->material = mini::Matte::create();
    for (auto grid : model->grids) {
      const float cellSize = powf(2.f, -(float)grid.level);
      const vec3i org = (const vec3i &)grid.origin;
      const vec3i dim = (const vec3i &)grid.dims;
      box3f bb;
      bb.lower = cellSize * (vec3f(org) - .5f);
      bb.upper = cellSize * (vec3f(org + dim) + .5f);
      addBoxWireframe(cs, bb, radius, blockDebugColor(grid.level));
    }
    return cs;
  }

  box3f TAMRVolume::getBounds() const
  {
    box3f bb;
    for (auto grid : model->grids) {
      const float cellSize = powf(2.f, -(float)grid.level);
      vec3i org = (const vec3i &)grid.origin;
      vec3i dim = (const vec3i &)grid.dims;
      bb.extend(cellSize * (vec3f(org) - .5f));
      bb.extend(cellSize * (vec3f(org + dim) + .5f));
    }
    return bb;
  }
  
  range1f TAMRVolume::getValueRange() const
  {
    range1f r;
    for (int i=0;i<model->numCellsAcrossAllGrids;i++)
      r.extend(model->scalars[i]);
    return r;
  }

}
