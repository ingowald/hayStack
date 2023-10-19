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
#include <map>

namespace hs {

  HayMaker::HayMaker(Comm &world,
                     Comm &workers,
                     ThisRankData &thisRankData,
                     bool verbose)
    : world(world),
      workers(workers),
      isActiveWorker(!thisRankData.empty()),
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
  
  // void HayMaker::loadData(DynamicDataLoader &loader,
  //                         int numDataGroups,
  //                         int dataPerRank)
  // {
  //   if (!isActiveWorker)
  //     return;
  //   if (dataPerRank == 0) {
  //     if (workers.size < numDataGroups) {
  //       dataPerRank = numDataGroups / workers.size;
  //     } else {
  //       dataPerRank = 1;
  //     }
  //   }

  //   if (numDataGroups % dataPerRank) {
  //     std::cout << "warning - num data groups is not a "
  //               << "multiple of data groups per rank?!" << std::endl;
  //     std::cout << "increasing num data groups to " << numDataGroups
  //               << " to ensure equal num data groups for each rank" << std::endl;
  //   }
  
  //   loader.assignGroups(numDataGroups);
  //   rankData.resize(dataPerRank);
  //   for (int i=0;i<dataPerRank;i++) {
  //     int dataGroupID = (workers.rank*dataPerRank+i) % numDataGroups;
  //     if (verbose) {
  //       for (int r=0;r<workers.rank;r++) 
  //         workers.barrier();
  //       std::cout << "#hv: worker #" << workers.rank
  //                 << " loading global data group ID " << dataGroupID
  //                 << " into slot " << workers.rank << "." << i << ":"
  //                 << std::endl << std::flush;
  //       usleep(100);
  //       fflush(0);
  //     }
  //     loader.loadDataGroup(rankData.dataGroups[i],
  //                          dataGroupID,
  //                          verbose);
  //     if (verbose) 
  //       for (int r=workers.rank;r<workers.size;r++) 
  //         workers.barrier();
  //   }
  //   if (verbose) {
  //     workers.barrier();
  //     if (workers.rank == 0)
  //       std::cout << "#hv: all workers done loading their data..." << std::endl;
  //     workers.barrier();
  //   }
  // }
  
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

    std::vector<BNGroup> groups;
    std::vector<affine3f> xfms;

    auto &myData = rankData.dataGroups[dgID];
    if (!myData.sphereSets.empty()) {
      std::vector<BNGeom>  geoms;
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
      BNGroup spheresGroup
        = bnGroupCreate(barney,geoms.data(),geoms.size(),
                        nullptr,0);
      bnGroupBuild(spheresGroup);
      groups.push_back(spheresGroup);
      xfms.push_back(affine3f());
    }

    for (auto mini : myData.minis) {
      BNMaterial material = BN_DEFAULT_MATERIAL;
      std::map<mini::Object::SP, BNGroup> miniGroups;
      for (auto inst : mini->instances) {
        if (!miniGroups[inst->object]) {
          std::vector<BNGeom> geoms;
          for (auto mesh : inst->object->meshes) {
            BNGeom geom
              = bnTriangleMeshCreate
              (barney,&material,
               (int3*)mesh->indices.data(),mesh->indices.size(),
               (float3*)mesh->vertices.data(),mesh->vertices.size(),
               nullptr,nullptr);
            geoms.push_back(geom);
          }
          BNGroup meshGroup
            = bnGroupCreate(barney,geoms.data(),geoms.size(),
                            nullptr,0);
          bnGroupBuild(meshGroup);
          miniGroups[inst->object] = meshGroup;
        }
        groups.push_back(miniGroups[inst->object]);
        xfms.push_back((const affine3f&)inst->xfm);
      }
    }
    
    bnModelSetInstances(barney,groups.data(),(BNTransform *)xfms.data(),
                        groups.size());
    // affine3f rootTransform;
    // bnModelSetInstances(barney,&rootGroup,(BNTransform *)&rootTransform,1);
    bnModelBuild(barney);
  }
  
}
