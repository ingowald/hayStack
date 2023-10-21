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
      rankData(std::move(thisRankData)),
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

    std::vector<vec4f> xfValues;
    for (int i=0;i<100;i++)
      xfValues.push_back(vec4f(.5f,.5f,0.5f,
                               clamp(5.f-fabsf(i-30),0.f,1.f)));
    
    BNTransferFunction defaultXF
      = bnTransferFunctionCreate(barney,
                                 /* no domain: */0.f,0.f,
                                 (const float4*)xfValues.data(),
                                 xfValues.size(),
                                 /*density:*/10.f);

      
    std::vector<BNGeom>   rootGroupGeoms;
    std::vector<BNVolume> rootGroupVolumes;
    std::vector<BNGroup>  groups;
    std::vector<affine3f> xfms;

    // ------------------------------------------------------------------
    // render all SphereGeoms
    // ------------------------------------------------------------------
    auto &myData = rankData.dataGroups[dgID];
    if (!myData.sphereSets.empty()) {
      std::vector<BNGeom>  &geoms = rootGroupGeoms;
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
      // BNGroup spheresGroup
      //   = bnGroupCreate(barney,geoms.data(),geoms.size(),
      //                   nullptr,0);
      // bnGroupBuild(spheresGroup);
      // groups.push_back(spheresGroup);
      // xfms.push_back(affine3f());
    }

    // ------------------------------------------------------------------
    // render all (possibly instanced) triangle meshes from mini format
    // ------------------------------------------------------------------
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
    
    // ------------------------------------------------------------------
    // render all UMeshes
    // ------------------------------------------------------------------
    for (auto unst : myData.unsts) {
      BNMaterial material = BN_DEFAULT_MATERIAL;
      std::vector<vec4f> verts4f;
      const float *scalars = unst->perVertex->values.data();
      for (int i=0;i<unst->vertices.size();i++) {
        verts4f.push_back(vec4f(unst->vertices[i].x,
                                unst->vertices[i].y,
                                unst->vertices[i].z,
                                scalars[i]));
      }
      BNScalarField mesh = bnUMeshCreate
        (barney,
         // vertices: 4 floats each
         (const float *)verts4f.data(),verts4f.size(),
         // tets: 4 ints each
         (const int *)unst->tets.data(),unst->tets.size(),
         nullptr,0,
         nullptr,0,
         nullptr,0);
      // rootGroupGeoms.push_back(geom);
      BNVolume volume = bnVolumeCreate(barney,mesh,defaultXF);
      rootGroupVolumes.push_back(volume);
    }
    

    // ------------------------------------------------------------------
    // create a single instance for all 'root' geometry that isn't
    // explicitly instantitated itself
    // ------------------------------------------------------------------
    if (!rootGroupGeoms.empty() || !rootGroupVolumes.empty()) {
      BNGroup rootGroup
        = bnGroupCreate(barney,rootGroupGeoms.data(),rootGroupGeoms.size(),
                        rootGroupVolumes.data(),rootGroupVolumes.size());
      bnGroupBuild(rootGroup);
      groups.push_back(rootGroup);
      xfms.push_back(affine3f());
    }

      
    // ------------------------------------------------------------------
    // finally - specify top-level instances for all the stuff we
    // generated
    // ------------------------------------------------------------------
    bnModelSetInstances(barney,groups.data(),(BNTransform *)xfms.data(),
                        groups.size());
    // affine3f rootTransform;
    // bnModelSetInstances(barney,&rootGroup,(BNTransform *)&rootTransform,1);
    bnModelBuild(barney);
  }
  
}
