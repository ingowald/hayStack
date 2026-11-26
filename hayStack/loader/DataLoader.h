// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/MPIWrappers.h"
#include "hayStack/OnePartition.h"
#include "hayStack/LocalPartitions.h"
#include "hayStack/loader/ResourceSpecifier.h"
#include "hayStack/loader/LoadableContent.h"
#include <fstream>

namespace hs {
  namespace loader {
    
    /*! helper function that creates an mini::ANARIMaterial from
        parameters specified in the given dataURL */
    mini::ANARIMaterial::SP parseAnariMaterial(const ResourceSpecifier &dataURL);
    
      
    /*! helper function that checks if a given string has another as prefix */
    bool startsWith(const std::string &haystack,
                    const std::string &needle);
    /*! helper function that checks if a given string has another as prefix */
    bool endsWith(const std::string &haystack,
                  const std::string &needle);
  
    /*! helper function that returns the size (in bytes) of a given file
      (or throws an exception if this file cannot be opened */
    size_t getFileSize(const std::string &fileName);
  
    /*! abstraction for an entity that can load one or more pieces of
      renderable data (i.e., "content") from a file */
    struct DataLoader {
      DataLoader(hs::Comm::SP workers);

      /*! returns rank of process loading the data */
      int myRank() const { return workers->rank; }
      void addContent(const std::string &contentDescriptor);
    
      /*! interface for any type of distributed content to add one or
        more different pieces of content; each of these gets
        assigned to exactly one data group */
      void addContent(DistributedContent *);
      /*! interface for any type of replicated content to add one or
        more different pieces of content; each of these gets loaded
        on every rank */
      void addContent(ReplicatedContent *);
      /*! interface for any type of rank-0-only content to add one or
        more different pieces of content; each of these gets loaded
        only on rank 0 */
      void addContent(Rank0Content *);
    
      virtual void assignGroups(int numDataRanks) = 0;
    
      /*! interface for the app to load one particular rank's data
        group(s) */
      virtual void loadPartition(OnePartition *dg) = 0;

      /*! actually loads one rank's data, based on which content got
        assigned to which rank. must get called on every worker
        collaboratively - but only on active workers */
      void loadData(/*! the local partitions to load into */
                    LocalPartitions &localPartitions,
                    int numDataRanks,
                    int dataPerRank);
    
      /*! all distributed (abstract) pieces of content in the scene;
          each of these gets assigned to exactly one data group */
      std::vector<std::tuple<double /*projected size*/,
                             int    /* linear index, to ensure stable sorting */,
                             DistributedContent *>> distributedContent;
      /*! all replicated content - this does not get assigned to any
          data group, but gets loaded on every rank */
      std::vector<ReplicatedContent *> replicatedContent;
      /*! all rank-0-only content - this does not get assigned to any
          data group, and gets loaded only on rank 0 */
      std::vector<Rank0Content *> rank0Content;

      /*! default radius to use for spheres that do not have a radius specified */
      static float defaultRadius;
      hs::Comm::SP workers;
    };

    /*! a data loader that assigns objects dynamically to data groups
      based on their projected weight */
    struct DynamicDataLoader : public DataLoader {
      DynamicDataLoader(hs::Comm::SP workers)
        : DataLoader(workers)
      {}
      void assignGroups(int numDataRanks) override;

      virtual void loadPartition(OnePartition *dg) override;
    private:
      /*! loadable content per data group, after assigning it */
      std::vector<std::vector<DistributedContent *>> contentOfGroup;
    
    };

    namespace withHeader {
      template<typename T>
      std::vector<T> loadVectorOf(std::ifstream &in, int part=0, int numParts=1)
      {
        std::vector<T> vec;
        size_t count;
        in.read((char *)&count,sizeof(count));
        size_t begin = part * count / numParts;
        size_t end = (part+1) * count / numParts;
        T t;
        for (size_t i=0;i<begin;i++)
          in.read((char *)&t,sizeof(t));
        for (size_t i=begin;i<end;i++) {
          in.read((char *)&t,sizeof(t));
          vec.push_back(t);
        }
        for (size_t i=end;i<count;i++)
          in.read((char *)&t,sizeof(t));
        return vec;
      }
    
      template<typename T>
      void writeVector(std::ofstream &out, const std::vector<T> &vec)
      {
        size_t count = vec.size();
        out.write((char *)&count,sizeof(count));
        out.write((const char *)vec.data(),count*sizeof(T));
      }
    }

    namespace noHeader {
      template<typename T>
      std::vector<T> loadVectorOf(std::ifstream &in, int part=0, int numParts=1)
      {
        if (!in.good()) throw std::runtime_error("invalid input stream");
        in.seekg(0,std::ios::end);
        size_t size = in.tellg();
        in.seekg(0,std::ios::beg);
        size_t count = size/sizeof(T);
        std::vector<T> vec;
        size_t begin = part * count / numParts;
        size_t end = (part+1) * count / numParts;
        T t;
        for (size_t i=0;i<begin;i++)
          in.read((char *)&t,sizeof(t));
        for (size_t i=begin;i<end;i++) {
          in.read((char *)&t,sizeof(t));
          vec.push_back(t);
        }
        for (size_t i=end;i<count;i++)
          in.read((char *)&t,sizeof(t));
        return vec;
      }

      template<typename T>
      std::vector<T> loadVectorOf(const std::string &fn, int part=0, int numParts=1)
      {
        std::ifstream in(fn,std::ios::binary);
        return noHeader::loadVectorOf<T>(in,part,numParts);
      }
    }

  }
}

