// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayStack/OnePartition.h"
#include <map>

namespace hs {

  OnePartition::OnePartition(const PartitionsIndex &partitionsIndex)
    : partitionsIndex(partitionsIndex)
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
    
    // can only merge meshes that use the same transfer function, so
    // group by xfName, and merge each group separately
    std::map<std::string,std::vector<umesh::UMesh::SP>> meshesByXF;
    for (auto unst : this->unsts)
      meshesByXF[unst->xfName].push_back(unst->mesh);
    this->unsts.clear();
    for (auto &it : meshesByXF) {
      umesh::UMesh::SP merged = umesh::mergeMeshes(it.second);
      std:: cout << "done merging, got " << merged->toString()
                 << " (xf '" << it.first << "')" << std::endl;
      this->unsts.push_back
        (std::make_shared<UMeshVolume>(it.first,merged,box3f()));
    }
  }
      
  BoundsData OnePartition::getBounds() const
  {
    BoundsData bounds;
    for (auto mini : minis)
      if (mini)
        bounds.spatial.extend(mini->getBounds());
    for (auto &unst : unsts) {
      if (unst && unst->mesh) {
        bounds.spatial.extend(unst->getBounds());
        bounds.scalars.extend(unst->getValueRange());
      }
    }
    for (auto &triangleMesh : triangleMeshes)
      if (triangleMesh)
        bounds.extend(triangleMesh->getBounds());
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
    for (auto &volume : nanovdbVolumes) {
      bounds.spatial.extend(volume->getBounds());
      bounds.scalars.extend(volume->getValueRange());
    }
    return bounds;
  }

  std::set<std::string> OnePartition::gatherXFNames() const
  {
    std::set<std::string> xfNames;
    for (auto &vol : structuredVolumes)
      xfNames.insert(vol->xfName);
    for (auto &vol : nanovdbVolumes)
      xfNames.insert(vol->xfName);
    for (auto &vol : amr)
      xfNames.insert(vol->xfName);
    for (auto &vol : unsts)
      xfNames.insert(vol->xfName);
    return xfNames;
  }

} // ::hs
