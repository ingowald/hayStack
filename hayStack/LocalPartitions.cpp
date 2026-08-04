// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

/*! a "LocalPartitions" describes the kind of data -- organized in one or
    more data ranks -- that a given app / mpi rank has loaded. note
    that while 'regular' ranks do have at least one data rank, for
    certain processes (like a head node) it is allowd to not have any
    data at all */

#include "hayStack/LocalPartitions.h"

namespace hs {

  LocalPartitions::LocalPartitions(const std::vector<int> &localDataRanks,
                                   int numPartitionsGlobally)
    : numPartitionsGlobally(numPartitionsGlobally)
  {
    assert(!localDataRanks.empty());
    for (int i=0;i<localDataRanks.size();i++) {
      auto p = new OnePartition(localDataRanks[i],
                                numPartitionsGlobally);
      myPartitions.push_back(p);
    }
    assert(!myPartitions.empty());
  }

  BoundsData LocalPartitions::getBounds() const
  {
    BoundsData bounds;
    for (auto &dg : myPartitions)
      bounds.extend(dg->getBounds());
    return bounds;
  }

  /*! returns whether this rank does *not* have any data; in this
    case it's a passive (head?-)node */
  bool LocalPartitions::empty() const
  {
    return myPartitions.empty();
  }
    
  /*! returns the number of data groups *on this rank* */
  int LocalPartitions::numPartitionsOnThisRank() const
  {
    return (int)myPartitions.size();
  }

  int LocalPartitions::numPartitionsTotal() const
  {
    return numPartitionsGlobally;
  }

  /*! this is an optimization in particular for models (like lander)
    where one rank might get multiple "smaller" unstructured
    meshes -- if each of these become their own volumes, with
    their own acceleration strcutre, etc, then that may have some
    negative side effects on performance */
  void LocalPartitions::mergeUnstructuredMeshes()
  {
    for (auto &part : myPartitions)
      part->mergeUnstructuredMeshes();
  }

} // ::hs
