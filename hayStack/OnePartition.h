// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

/*! a hay-*stack* is a description of data-parallel data */

#pragma once

#include "hayStack/HayStack.h"
#include "hayStack/Spheres.h"
#include "hayStack/Cylinders.h"
#include "hayStack/TriangleMesh.h"
#include "hayStack/Capsules.h"
#include "hayStack/StructuredVolume.h"
#include "hayStack/TAMRVolume.h"
#if HS_USE_MULTI_SCATTERING
# include "hayStack/NanoVDBVolume.h"
#endif
#include <miniScene/Scene.h>
#include <umesh/UMesh.h>

namespace hs {

  /*! the model data that each rank/device should see, no matter which
    logical partition it has - in particular light sources, but also
    render settings as far as the scene is setting those */
  struct GlobalModelData {
    std::vector<mini::DirLight>     directional;
    
    /*! global data that _should_ remain the same across all ranks and
      partitions */
    struct  {
      int   pixelSamples;
      float ambientRadiance;
      vec4f bgColor;
    } renderSettings;
  };

  /*! one "partition" of a data-distributed scene. For data replicated
      rendering this is simply "the" scene (ie, there is but one
      parition of the entire scene); for distributed rendering this is
      one of the partitions that the scene has been partitioned into.

      Note:

      - for the loader this is the smallest entity into which it is
        on-demand-partitioning the scene into.

      - for traditional data parallel rendering each rank would have
        exactly one partition, and each rank would have a different
        partition. To _also_ support multi-gpu data parallel rendering
        and all the mixed data parallel/data replicated modes that
        barney can do this is _not_ necessarily the case in haystack:
        for multi-gpu rendering, we need different partitions for
        different gpus, so a single rank/process has more than one
        partition. Similarly, the same (logical) partition may be
        loaded onto more than a single gpu, or even onto more than a
        single rank.

      - depending on use we have to distinguish between _phsysical_
        and _logical_ partitions of a scene: Eg, imagine a scene that
        we partition into three parts, call them A, B, and C. In this
        codebase, we refer to this as _logical_ partitions of that
        model, because A+B+C contains all the data of the original
        model. However, the same logical partition could be loaded to
        more than one rank/process (or even to multiple GPUs in the
        same rank/process; we refer to these as the _physical_
        partitions.
  */
  struct OnePartition {
    /*! this is an optimization in particular for models (like lander)
        where one rank might get multiple "smaller" unstructured
        meshes -- if each of these become their own volumes, with
        their own acceleration strcutre, etc, then that may have some
        negative side effects on performance */
    void mergeUnstructuredMeshes();

    OnePartition(int partitionsRank,
                 int partitionsCount);
    BoundsData getBounds() const;
    
    mini::Material::SP                defaultMaterial;
    std::vector<mini::Scene::SP>      minis;
    /*! mesh AND domain. domain being empty means 'no clip box' */
    std::vector<std::pair<umesh::UMesh::SP,box3f>> unsts;
    std::vector<TriangleMesh::SP>     triangleMeshes;
    std::vector<SphereSet::SP>        sphereSets;
    std::vector<Cylinders::SP>        cylinderSets;
    std::vector<Capsules::SP>         capsuleSets;
    std::vector<StructuredVolume::SP> structuredVolumes;
#if HS_USE_MULTI_SCATTERING
    std::vector<NanoVDBVolume::SP>    nanovdbVolumes;
#endif
    std::vector<TAMRVolume::SP>       amr;
    
    const int partitionsRank;
    const int partitionsCount;
  };

} // ::hs
