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

#pragma once

#include "HayMaker.h"
#if HS_FAKE_MPI
# include <barney/barney.h>
#else
# include <barney/barney.h>
# include <barney/barney_mpi.h>
#endif

namespace hs {

  /*! c++ helper function */
  inline void bnSetAndRelease(BNObject target, const char *paramName,
                              BNObject value)
  { bnSetObject(target,paramName,value); bnRelease(value); }
  
  /*! c++ helper function */
  inline void bnSetAndRelease(BNObject target, const char *paramName,
                              BNData value)
  { bnSetData(target,paramName,value); bnRelease(value); }
  

  struct BarneyBackend {
    typedef BNMaterial MaterialHandle;
    typedef BNSampler  TextureHandle;
    typedef BNGroup    GroupHandle;
    typedef BNLight    LightHandle;
    typedef BNVolume   VolumeHandle;
    typedef BNGeom     GeomHandle;
    
    struct Global {
      Global(HayMaker *base);

      void init();
      void resize(const vec2i &fbSize, uint32_t *hostRgba);
      void renderFrame();
      void resetAccumulation();
      void setCamera(const Camera &camera);
      void finalizeRender();
      /*! clean up and shut down */
      void terminate();
      
      HayMaker *const base;
      BNContext     context  = 0;
      BNModel       model    = 0;
      BNRenderer    renderer = 0;
      BNFrameBuffer fb       = 0;
      BNCamera      camera   = 0;
      vec2i         fbSize;
      uint32_t     *hostRGBA = 0;
    };
    
    void resize(const vec2i &fbSize, uint32_t *hostRgba);
    void renderFrame();
    void resetAccumulation();
    void setCamera(const Camera &camera);
    

    // void buildDataGroup(int dgID);
    
    struct Slot {
      Slot(Global *global, int slot,
           typename HayMakerT<BarneyBackend>::Slot *impl)
        : global(global), slot(slot), impl(impl) {}
    
      void applyTransferFunction(const TransferFunction &xf);
      // void setTransferFunction(const std::vector<VolumeHandle> &volumes,
      //                          const TransferFunction &xf);

      BNLight create(const mini::QuadLight &ml);
      BNLight create(const mini::DirLight &ml);
      BNLight create(const mini::EnvMapLight &ml);

      BNGroup    createGroup(const std::vector<BNGeom> &geoms,
                             const std::vector<BNVolume> &volumes);
      BNMaterial create(mini::Plastic::SP plastic, bool colorMapped);
      BNMaterial create(mini::Velvet::SP velvet, bool colorMapped);
      BNMaterial create(mini::Matte::SP matte, bool colorMapped);
      BNMaterial create(mini::Metal::SP metal, bool colorMapped);
      BNMaterial create(mini::BlenderMaterial::SP blender, bool colorMapped);
      BNMaterial create(mini::ThinGlass::SP thinGlass, bool colorMapped);
      BNMaterial create(mini::Dielectric::SP dielectric, bool colorMapped);
      BNMaterial create(mini::MetallicPaint::SP metallicPaint, bool colorMapped);
      BNMaterial create(mini::DisneyMaterial::SP disney, bool colorMapped);
      BNMaterial create(mini::Material::SP miniMat, bool colorMapped);

      std::vector<BNGeom>
      createSpheres(SphereSet::SP content,
                    MaterialLibrary<BarneyBackend> *materialLib);
      std::vector<BNGeom>
      createCylinders(Cylinders::SP content);

      std::vector<BNGeom>
      createCapsules(hs::Capsules::SP caps,
                     MaterialLibrary<BarneyBackend> *materialLib);
      
      BNSampler create(mini::Texture::SP miniTex);
      GeomHandle create(mini::Mesh::SP miniMesh,
                        MaterialLibrary<BarneyBackend> *materialLib);

      BNVolume create(const StructuredVolume::SP &v);
      BNVolume create(const std::pair<umesh::UMesh::SP,box3f> &v);
      // BNVolume create(const UMeshVolume::SP &v);
      
      void setInstances(const std::vector<BNGroup> &groups,
                        const std::vector<affine3f> &xfms);
      void setLights(BNGroup rootGroup, const std::vector<BNLight> &lights);

      inline void release(BNSampler t)  { bnRelease(t); }
      inline void release(BNMaterial m) { bnRelease(m); }

      void finalizeSlot();
      
      bool needRebuild = true;
      typename HayMakerT<BarneyBackend>::Slot *const impl;
      Global *const global;
      int     const slot;
    };
  };
  
} // ::hs
