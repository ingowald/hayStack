// ======================================================================== //
// Copyright 2023-2023 Ingo Wald                                            //
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

#include "hayStack/common.h"
#include <mpi.h>
#include <stdexcept>

#define HS_MPI_CALL(fctCall)                                                 \
    { int rc = MPI_##fctCall; if (rc != MPI_SUCCESS) throw hs::mpi::Exception(__PRETTY_FUNCTION__,rc); }
    
namespace hs {
  namespace mpi {

    inline std::string mpiErrorString(int rc)
    {
      char s[MPI_MAX_ERROR_STRING];
      memset(s,0,MPI_MAX_ERROR_STRING);
      int len = MPI_MAX_ERROR_STRING;
      MPI_Error_string(rc,s,&len);
      return s;
    }
    struct Exception : public std::runtime_error {
      Exception(const std::string &where, int rc)
        : std::runtime_error("#hs.mpi (@"+where+") : " + mpiErrorString(rc))
      {}
    };
    
    void init(int &ac, char **av);
    void finalize();
    
    struct Comm {
      Comm(MPI_Comm comm=MPI_COMM_NULL);

      /*! equivalent of MPI_Comm_split - splits this comm into
          possibly multiple comms, each one containing exactly those
          former ranks of the same color. E.g. split(old.rank > 0)
          would have rank 0 get a communicator that contains only
          itself, and all others get a communicator that contains all
          other former ranks */
      Comm split(int color);
      
      inline operator MPI_Comm() { return comm; }
      void  assertValid() const;
      int   allReduceMax(int value) const;
      int   allReduceMin(int value) const;
      float allReduceMax(float value) const;
      float allReduceMin(float value) const;
      vec3f allReduceMax(vec3f value) const;
      vec3f allReduceMin(vec3f value) const;
      int   allReduceAdd(int value) const;
      float allReduceAdd(float value) const;
      void barrier() const;

      /*! free/close this communicator */
      void free();

      void allGather(int *allValues, int myValue);
      void allGather(int *allValues, const int *myValues, int numMyValues);
      
      /*! master-side of a gather where clietn gathers a fixed number
          of itmes from each rank */
      template<typename T>
      void masterGather(// where we'll receive into - for ALL ranks
                        T *recvBuffer,
                        // what we're sending (this rank's data only)
                        const T *sendBuffer, int numItemsSentOnEachRank);
      /*! client-side of a gather where each client send a fixed number
          of items to the master */
      template<typename T>
      void masterGather(// what we're sending (this rank's data only)
                        const T *sendBuffer, int numItemsSentOnEachRank);

      template<typename T>
      void recv(int fromRank, int tag,
                T *buffer, int numItems, MPI_Request &req);

      template<typename T>
      void send(int fromRank, int tag,
                const T *buffer, int numItems, MPI_Request &req);

      void wait(MPI_Request &req)
      {
        HS_MPI_CALL(Wait(&req,MPI_STATUS_IGNORE));
      }

      /*! master's send side of broadcast - must be done on rank 0,
          and matched by bc_recv on all workers */
      void bc_send(const void *ptr, size_t numBytes);
      
      /*! receive side of a broadcast - must be called on all ranks >
          0, and match a bc_send on rank 0 */
      void bc_recv(void *ptr, size_t numBytes);

      int rank = -1, size = -1;
      MPI_Comm comm = MPI_COMM_NULL;
    };
    
    template<typename T>
    inline void Comm::recv(int fromRank, int tag,
                                  T *buffer, int numItems, MPI_Request &req)
    {
      // HS_MPI_CALL(Recv(buffer,numItems*sizeof(T),MPI_BYTE,
      //                  fromRank,tag,comm,MPI_STATUS_IGNORE),
      //             "Irecv");
      HS_MPI_CALL(Irecv(buffer,
                        numItems*sizeof(T),MPI_BYTE,
                        // 1,MPI_INT,
                        fromRank,0x123+tag,comm,&req));
    }
    
    template<typename T>
    inline void Comm::send(int toRank, int tag,
                           const T *buffer, int numItems, MPI_Request &req)
    {
      // HS_MPI_CALL(Send(buffer,numItems*sizeof(T),MPI_BYTE,
      //                  toRank,tag,comm),
      //             "Isend");
      HS_MPI_CALL(Isend(buffer,
                        numItems*sizeof(T),MPI_BYTE,
                        // 1,MPI_INT,
                        toRank,0x123+tag,comm,&req));
    }
    
    /*! master-side of a gather where clietn gathers a fixed number
      of itmes from each rank */
    template<typename T>
    inline void Comm::masterGather(// where we'll receive into - for ALL ranks
                                   T *recvBuffer,
                                   // what we're sending (this rank's data only)
                                   const T *sendBuffer, int numItemsSentOnEachRank)
    {
      HS_MPI_CALL(Gather(sendBuffer,numItemsSentOnEachRank*sizeof(T),MPI_BYTE,
                          recvBuffer,numItemsSentOnEachRank*sizeof(T),MPI_BYTE,
                          0,comm));
    }
    
    /*! client-side of a gather where each client send a fixed number
      of items to the master */
    template<typename T>
    inline void Comm::masterGather(// what we're sending (this rank's data only)
                                   const T *sendBuffer, int numItemsSentOnEachRank)
    {
      HS_MPI_CALL(Gather(sendBuffer,numItemsSentOnEachRank*sizeof(T),MPI_BYTE,
                          nullptr,numItemsSentOnEachRank*sizeof(T),MPI_BYTE,
                          0,comm));
    }
    

  }
}
