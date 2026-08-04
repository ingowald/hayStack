// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

/*! a "LocalPartitions" describes the kind of data -- organized in one or
    more data ranks -- that a given app / mpi rank has loaded. note
    that while 'regular' ranks do have at least one data rank, for
    certain processes (like a head node) it is allowd to not have any
    data at all */

#pragma once

#include "hayStack/OnePartition.h"

namespace hs {

  /*! the (one or more) partition(s) FOR ONE RANK/PROCESS. Note that
      due to support for multi-gpu (in addition to multi-rank) each
      rank _can_ contain more than one model partitions */
  struct LocalPartitions {
    LocalPartitions(const std::vector<int> &localDataRanks,
                    int numPartitionsGlobally);
    BoundsData getBounds() const;

    /*! returns whether this rank does *not* have any data; in this
      case it's a passive (head?-)node */
    bool empty() const;
    
    /*! returns the number of data groups *on this rank* */
    int numPartitionsOnThisRank() const;
    int numPartitionsTotal() const;

    /*! this is an optimization in particular for models (like lander)
      where one rank might get multiple "smaller" unstructured
      meshes -- if each of these become their own volumes, with
      their own acceleration strcutre, etc, then that may have some
      negative side effects on performance */
    void mergeUnstructuredMeshes();

    /*! these are (only) the current rank's partitions; there might be
        more ranks with more data */
    std::vector<OnePartition *> myPartitions;
    int const numPartitionsGlobally;
    // int colorMapIndex = 0;
  };

} // ::hs
