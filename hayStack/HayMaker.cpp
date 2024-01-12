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
  {
    perDG.resize(rankData.size());
  }

  void HayMaker::createBarney()
  {
    if (isActiveWorker) {
      std::vector<int> dataGroupIDs;
      for (auto dg : rankData.dataGroups)
        dataGroupIDs.push_back(dg.dataGroupID);
      barney = bnMPIContextCreate
        (world.comm,
         /*data*/dataGroupIDs.data(),dataGroupIDs.size(),
         /*gpus*/nullptr,-1);
    } else
      barney = bnMPIContextCreate
        (world.comm,/*data*/nullptr,0,/*gpus*/nullptr,0);

    fb = bnFrameBufferCreate(barney,0);
    model = bnModelCreate(barney);
  }
  
  BoundsData HayMaker::getWorldBounds() const
  {
    BoundsData bb = rankData.getBounds();
    bb.spatial.lower = world.allReduceMin(bb.spatial.lower);
    bb.spatial.upper = world.allReduceMax(bb.spatial.upper);
    bb.scalars.lower = world.allReduceMin(bb.scalars.lower);
    bb.scalars.upper = world.allReduceMax(bb.scalars.upper);
    return bb;
  }
  
  void HayMaker::resize(const vec2i &fbSize, uint32_t *hostRGBA)
  {
    assert(fb);
    this->fbSize = fbSize;
    bnFrameBufferResize(fb,fbSize.x,fbSize.y,(world.rank==0)?hostRGBA:nullptr);
  }

  void HayMaker::setTransferFunction(const TransferFunction &xf) 
  {
    for (int dgID=0;dgID<rankData.size();dgID++) {
      auto &dg = perDG[dgID];
      if (dg.createdVolumes.empty())
        continue;

      PING; PRINT(xf.domain);
      BNDataGroup barney = bnGetDataGroup(model,dgID);
      for (auto volume : dg.createdVolumes)
        bnVolumeSetXF(volume,
                      (float2&)xf.domain,
                      (const float4*)xf.colorMap.data(),
                      xf.colorMap.size(),
                      xf.baseDensity);
    }
    parallel_for
      ((int)rankData.size(),
       [&](int dgID)
       {
         auto &dg = perDG[dgID];
         if (dg.createdVolumes.empty())
           return;
         bnGroupBuild(dg.volumeGroup);

         BNDataGroup barney = bnGetDataGroup(model,dgID);
         bnBuild(barney);
       });
  }
  
  void HayMaker::renderFrame()
  {
    bnRender(model,&camera,fb);
  }

  void HayMaker::resetAccumulation()
  {
    bnAccumReset(fb);
  }

  void HayMaker::setCamera(const Camera &camera)
  {
    bnPinholeCamera(&this->camera,
                    (const float3&)camera.vp,
                    (const float3&)camera.vi,
                    (const float3&)camera.vu,
                    camera.fovy,
                    fbSize.x / float(fbSize.y));
  }
  
  void HayMaker::buildDataGroup(int dgID)
  {
    BNDataGroup barney = bnGetDataGroup(model,dgID);

    std::vector<vec4f> xfValues;
    for (int i=0;i<100;i++)
      xfValues.push_back(vec4f(.5f,.5f,0.5f,
                               clamp(5.f-fabsf(i-40),0.f,1.f)));

    auto &dg = perDG[dgID];
      
    std::vector<BNGeom>   rootGroupGeoms;
    std::vector<BNGroup>  groups;
    std::vector<affine3f> xfms;
    auto &myData = rankData.dataGroups[dgID];

    // ------------------------------------------------------------------
    // render all SphereGeoms
    // ------------------------------------------------------------------
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
    }

    // ------------------------------------------------------------------
    // render all CylinderGeoms
    // ------------------------------------------------------------------
    if (!myData.cylinderSets.empty()) {
      std::vector<BNGeom>  &geoms = rootGroupGeoms;
      for (auto &cylinderSet : myData.cylinderSets) {
        BNMaterial material = BN_DEFAULT_MATERIAL;
        BNGeom geom
          = bnCylindersCreate(barney,
                              &material,
                              (float3*)cylinderSet->points.data(),
                              cylinderSet->points.size(),
                              (int2*)cylinderSet->indices.data(),
                              cylinderSet->indices.size(),
                              cylinderSet->radii.data(),
                              cylinderSet->radius);
        geoms.push_back(geom);
      }
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
      std::vector<int> gridOffsets;
      std::vector<vec3i> gridDims;
      std::vector<box4f> gridDomains;
      const std::vector<float> &gridScalars = unst->gridScalars;
      const float *scalars = unst->perVertex->values.data();

      for (int i=0;i<unst->grids.size();i++) {
        const umesh::Grid &grid = unst->grids[i];
        gridDims.push_back((vec3i &)grid.numCells);
        gridDomains.push_back((box4f &)grid.domain);
        gridOffsets.push_back(grid.scalarsOffset);
      }

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
         (const int *)unst->tets.data(),(int)unst->tets.size(),
         (const int *)unst->pyrs.data(),(int)unst->pyrs.size(),
         (const int *)unst->wedges.data(),(int)unst->wedges.size(),
         (const int *)unst->hexes.data(),(int)unst->hexes.size(),
         (int)unst->grids.size(),
         gridOffsets.data(),
         (const int *)gridDims.data(),
         (const float *)gridDomains.data(),
         gridScalars.data(), (int)gridScalars.size());
      // rootGroupGeoms.push_back(geom);
      BNVolume volume = bnVolumeCreate(barney,mesh);
      dg.createdVolumes.push_back(volume);
    }
    

    // ------------------------------------------------------------------
    // render all UMeshes
    // ------------------------------------------------------------------
    for (auto vol : myData.structuredVolumes) {
      BNScalarType scalarType;
      switch(vol->scalarType) {
      case StructuredVolume::UINT8:
        scalarType = BN_UINT8;
        break;
      case StructuredVolume::FLOAT:
        scalarType = BN_FLOAT;
        break;
      default: throw std::runtime_error("Unknown scalar type");
      }
      BNMaterial material = BN_DEFAULT_MATERIAL;
      PING; PRINT(vol->dims);
      BNScalarField bnVol = bnStructuredDataCreate
        (barney,
         (const int3&)vol->dims,
         scalarType,
         vol->rawData.data(),
         (const float3&)vol->gridOrigin,
         (const float3&)vol->gridSpacing);
      dg.createdVolumes.push_back(bnVolumeCreate(barney,bnVol));
    }
    

    // ------------------------------------------------------------------
    // create a single instance for all 'root' geometry that isn't
    // explicitly instantitated itself
    // ------------------------------------------------------------------
    if (!rootGroupGeoms.empty()) {
      BNGroup rootGroup
        = bnGroupCreate(barney,rootGroupGeoms.data(),rootGroupGeoms.size(),
                        nullptr,0);
      bnGroupBuild(rootGroup);
      groups.push_back(rootGroup);
      xfms.push_back(affine3f());
    }
    // ------------------------------------------------------------------
    // create a single instance for all 'root' volume(s)
    // ------------------------------------------------------------------
    if (!dg.createdVolumes.empty()) {
      BNGroup rootGroup
        = bnGroupCreate(barney,nullptr,0,
                        dg.createdVolumes.data(),
                        dg.createdVolumes.size());
      bnGroupBuild(rootGroup);
      groups.push_back(rootGroup);
      xfms.push_back(affine3f());
      dg.volumeGroup = rootGroup;
    }

      
    // ------------------------------------------------------------------
    // finally - specify top-level instances for all the stuff we
    // generated
    // -----------------------------------------------------------------
    bnSetInstances(barney,groups.data(),(BNTransform *)xfms.data(),
                        groups.size());
    bnBuild(barney);
  }
  
}
