// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/Volume.h"
#include <umesh/UMesh.h>

namespace hs {

  /*! wraps around a umesh::UMesh (ie, one rank's part of an
      unstructured-mesh volume), plus an optional domain that this
      part is clipped to */
  struct UMeshVolume : public Volume {
    typedef std::shared_ptr<UMeshVolume> SP;

    UMeshVolume(umesh::UMesh::SP mesh,
                /*! domain being empty means 'no clip box' */
                const box3f &domain = box3f(),
                const std::string &xfName = "defaultVolume")
      : Volume(xfName),
        mesh(mesh),
        domain(domain)
    {}

    box3f getBounds() const override
    {
      if (!domain.empty()) return domain;
      umesh::box3f bb = mesh->getBounds();
      return (const box3f &)bb;
    }
    range1f getValueRange() const override
    {
      umesh::range1f sr = mesh->getValueRange();
      return (const range1f &)sr;
    }

    umesh::UMesh::SP mesh;
    /*! domain being empty means 'no clip box' */
    box3f            domain;
  };

}
