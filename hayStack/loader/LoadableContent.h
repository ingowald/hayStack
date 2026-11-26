// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/OnePartition.h"
#include "hayStack/ReplicatedData.h"
#include "hayStack/Rank0Data.h"
#include "hayStack/loader/ResourceSpecifier.h"

namespace hs {
  namespace loader {

    /*! the loader that loadable content gets added to; derived
        loaders' create() functions take a pointer to one of these */
    struct DataLoader;

    /*! abstraction for some piece of renderable content, such as a
      triangle mesh, a mini scene (or part thereof), a umesh, a
      sub-set of spheres or triangles from a ray spheres/triangles
      file, etc. Key feature of a `LoadableContent` is that it can do
      'deferred' loading in the sense that it can execute the actual
      loading only after different pieces of content have already been
      assinged to different ranks/data groups. */
    struct LoadableContent {
      virtual ~LoadableContent() = default;
    
      virtual std::string toString() = 0;
    };

    /*! base class for distributed content, ie, content that gets
        assigned to (and loaded on) exactly one data group */
    struct DistributedContent : public LoadableContent {
      /*! return an estimate of how much (GPU) memory this specific
        piece of content is probably going to cost. this can/will be
        used to assign different content to differnet gpus based on
        weight */
      virtual size_t projectedSize() = 0;

      /*! make this content execute the actual load, and add the
        actually loaded content to the specific data group */
      virtual void   executeLoad(OnePartition &dataGroup) = 0;
    };

    /*! base class for replicated content, ie, content that gets
        loaded (once) on every rank, into that rank's ReplicatedData */
    struct ReplicatedContent : public LoadableContent {
      /*! make this content execute the actual load, and add the
        actually loaded content to this rank's replicated
        data. gets called exactly once on every rank */
      virtual void   executeLoad(ReplicatedData &replicated) = 0;
    };

    /*! base class for content that gets loaded only on rank 0 (eg,
        because only rank 0 actually needs it), into rank 0's
        Rank0Data */
    struct Rank0Content : public LoadableContent {
      /*! make this content execute the actual load, and add the
        actually loaded content to rank 0's Rank0Data. gets called
        exactly once, and only on rank 0 */
      virtual void   executeLoad(Rank0Data &rank0) = 0;
    };

  }
}
