// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayStack/OnePartition.h"

namespace hs {

  OnePartition::OnePartition(int partitionsRank,
                             int partitionsCount)
    : partitionsRank(partitionsRank),
      partitionsCount(partitionsCount)
  {
    defaultMaterial = mini::DisneyMaterial::create();
  }

  /*! this is an optimization in particular for models (like lander)
    where one rank might get multiple "smaller" unstructured
    meshes -- if each of these become their own volumes, with
    their own acceleration strcutre, etc, then that may have some
    negative side effects on performance */
  void OnePartition::mergeUnstructuredMeshes()
  {
    if (unsts.empty())
      // dont have any unstructured meshes - done.
      return;
    
    std::vector<umesh::UMesh::SP> unsts;
    for (auto _unst : this->unsts)
      unsts.push_back(_unst.first);
    umesh::UMesh::SP merged = umesh::mergeMeshes(unsts);
    std:: cout << "done merging, got " << merged->toString() << std::endl;
    this->unsts.clear();
    this->unsts.push_back({merged,box3f()});
  }
      
  BoundsData OnePartition::getBounds() const
  {
    BoundsData bounds;
    for (auto mini : minis)
      if (mini)
        bounds.spatial.extend(mini->getBounds());
    for (auto &_unst : unsts) {
      auto unst = _unst.first;
      if (unst) {
        umesh::box3f bb = unst->getBounds();
        if (!_unst.second.empty())
          bb = (const umesh::box3f &)_unst.second;
        bounds.spatial.extend((const box3f&)bb);
        umesh::range1f sr = unst->getValueRange();
        bounds.scalars.extend((const range1f&)sr);
      }
    }
    for (auto &sphereSet : sphereSets)
      if (sphereSet)
        bounds.spatial.extend(sphereSet->getBounds());
    for (auto &tm : triangleMeshes)
      if (tm)
        bounds.extend(tm->getBounds());
    for (auto &capsuleSet : capsuleSets)
      if (capsuleSet)
        bounds.spatial.extend(capsuleSet->getBounds());
    for (auto &cylinderSet : cylinderSets)
      if (cylinderSet)
        bounds.spatial.extend(cylinderSet->getBounds());
    for (auto &volume : amr) {
      bounds.spatial.extend(volume->getBounds());
      bounds.scalars.extend(volume->getValueRange());
    }
    for (auto &volume : structuredVolumes) {
      bounds.spatial.extend(volume->getBounds());
      bounds.scalars.extend(volume->getValueRange());
    }
#if HS_USE_MULTI_SCATTERING
    for (auto &volume : nanovdbVolumes) {
      bounds.spatial.extend(volume->getBounds());
      bounds.scalars.extend(volume->getValueRange());
    }
#endif
    return bounds;
  }

} // ::hs
