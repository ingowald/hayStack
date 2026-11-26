// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

/*! a "LocalPartitions" describes the kind of data -- organized in one or
    more data ranks -- that a given app / mpi rank has loaded. note
    that while 'regular' ranks do have at least one data rank, for
    certain processes (like a head node) it is allowd to not have any
    data at all */

#include "hayStack/LocalPartitions.h"

namespace hs {

  void LocalPartitions::createPartitions(const std::vector<int> &localDataRanks,
                                         int numPartitionsGlobally)
  {
    assert(myPartitions.empty());
    assert(!localDataRanks.empty());
    this->numPartitionsGlobally = numPartitionsGlobally;
    for (int i=0;i<localDataRanks.size();i++) {
      PartitionsIndex pi;
      pi.global.index = localDataRanks[i];
      pi.global.count = numPartitionsGlobally;
      pi.local.index = i;
      pi.local.count = localDataRanks.size();
      auto p = new OnePartition(pi);
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

  OnePartition *LocalPartitions::get(int localPartitionIndex) const
  {
    assert(localPartitionIndex >= 0);
    assert(localPartitionIndex < myPartitions.size());
    return myPartitions[localPartitionIndex];
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

  std::vector<std::pair<std::string,range1f>> LocalPartitions::gatherXFNames() const
  {
    std::map<std::string,range1f> xfNames;
    for (auto part : myPartitions) {
      if (!part) continue;
      for (auto &it : part->gatherXFNames())
        xfNames[it.first].extend(it.second);
    }
    // also add names of all transfer functions loaded as rank-0
    // content (even if no volume refers to them, in which case their
    // range stays empty)
    for (auto &it : rank0.transferFunctions)
      xfNames[it.first];
    return { xfNames.begin(), xfNames.end() };
  }

} // ::hs
