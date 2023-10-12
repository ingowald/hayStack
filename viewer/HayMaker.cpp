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

#include "HayMaker.h"

namespace hs {

  HayMaker::HayMaker(Comm &world,
                     bool isActiveWorker,
                     bool verbose)
    : world(world),
      workers(world.split(isActiveWorker)),
      isActiveWorker(isActiveWorker),
      verbose(verbose)
  {}

  void HayMaker::createBarney()
  {
    if (isActiveWorker) {
      std::vector<int> dataGroupIDs;
      for (auto dg : rankData.dataGroups)
        dataGroupIDs.push_back(dg.dataGroupID);
      barney = bnMPIContextCreate(world.comm,/*data*/dataGroupIDs.data(),dataGroupIDs.size(),
                               /*gpus*/nullptr,-1);
    } else
      barney = bnMPIContextCreate(world.comm,/*data*/nullptr,0,/*gpus*/nullptr,0);

    fb = bnFrameBufferCreate(barney,0);
    model = bnModelCreate(barney);
  }
  
  void HayMaker::loadData(DynamicDataLoader &loader,
                          int numDataGroups,
                          int dataPerRank)
  {
    if (!isActiveWorker)
      return;
    if (dataPerRank == 0) {
      if (workers.size < numDataGroups) {
        dataPerRank = numDataGroups / workers.size;
      } else {
        dataPerRank = 1;
      }
    }

    if (numDataGroups % dataPerRank) {
      std::cout << "warning - num data groups is not a "
                << "multiple of data groups per rank?!" << std::endl;
      std::cout << "increasing num data groups to " << numDataGroups
                << " to ensure equal num data groups for each rank" << std::endl;
    }
  
    loader.assignGroups(numDataGroups);
    rankData.resize(dataPerRank);
    for (int i=0;i<dataPerRank;i++) {
      int dataGroupID = (workers.rank*dataPerRank+i) % numDataGroups;
      if (verbose) {
        for (int r=0;r<workers.rank;r++) 
          workers.barrier();
        std::cout << "#hv: worker #" << workers.rank
                  << " loading data group " << dataGroupID << " into slot " << workers.rank << "." << i << ":"
                  << std::endl << std::flush;
        usleep(100);
        fflush(0);
      }
      loader.loadDataGroup(rankData.dataGroups[i],
                           dataGroupID,
                           verbose);
      if (verbose) 
        for (int r=workers.rank;r<workers.size;r++) 
          workers.barrier();
    }
    if (verbose) {
      workers.barrier();
      if (workers.rank == 0)
        std::cout << "#hv: all workers done loading their data..." << std::endl;
      workers.barrier();
    }
  }
  
  box3f HayMaker::getWorldBounds() const
  {
    box3f bb = rankData.getBounds();
    bb.lower = world.allReduceMin(bb.lower);
    bb.upper = world.allReduceMax(bb.upper);
    return bb;
  }
  
  void HayMaker::resize(const vec2i &fbSize, uint32_t *hostRGBA)
  {
    assert(fb);
    this->fbSize = fbSize;
    bnFrameBufferResize(fb,fbSize.x,fbSize.y,(world.rank==0)?hostRGBA:nullptr);
  }
  
  void HayMaker::renderFrame()
  {
    bnRender(model,&camera,fb,nullptr);
  }

  void HayMaker::setCamera(const Camera &camera)
  {
    bnPinholeCamera(&this->camera,
                    (const float3&)camera.vp,
                    (const float3&)camera.vi,
                    (const float3&)camera.vu,
                    camera.fovy,
                    make_int2(fbSize.x,fbSize.y));
  }
  
  void HayMaker::buildDataGroup(int dgID)
  {
    BNDataGroup barney = bnGetDataGroup(model,dgID);
    std::vector<BNGeom> geoms;

    auto &myData = rankData.dataGroups[dgID];
    for (auto &sphereSet : myData.sphereSets) {
      BNMaterial material = BN_DEFAULT_MATERIAL;
      BNGeom geom
        = bnSpheresCreate(barney,
                          &material,
                          (float3*)sphereSet->origins.data(),
                          sphereSet->origins.size(),
                          sphereSet->radii.data(),
                          sphereSet->radius);
      geoms.push_back(geom);
    }
    
    BNGroup group
      = bnGroupCreate(barney,geoms.data(),geoms.size(),
                      nullptr,0);
    BNInstance rootInstance;
    rootInstance.group = group;
    (affine3f&)rootInstance.xfm = affine3f();
    bnModelSetInstances(barney,&rootInstance,1);
// #if 0
//     std::vector<OWLGeom> geoms;
//     auto &dg = dataGroups[dgID];

//     for (auto &sphereSet : dg.spheresSets) {
//       OWLGeom geom = owlGeomCreate
//         (dev->owl,dev->spheresDG);
//       OWLData origins
//         = owlDeviceBufferCreate(dev->owl,
//                                 OWL_FLOAT3,
//                                 sphereSet.origins.size(),
//                                 sphereSet.origins.data());
//       owlGeomSetData(geom,"origins",origins);
//       owlGeomSet1f(geom,"radius",dg.radius);
//       geoms.push_back(geom);
//     }
//     OWLGroup group
//       = owlUserGeomGroupCreate(dev->owl);
// #endif
  }
  
}
