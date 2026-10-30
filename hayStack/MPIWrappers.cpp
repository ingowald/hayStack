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

#include "hayStack/MPIWrappers.h"
#if HS_MPI
# include <mpi.h>
#endif

#if !HS_MPI
# define HS_FAKE_MPI 1
#endif

#if HS_FAKE_MPI
#define HS_MPI_CALL(fctCall)  
#define MPI_Comm int
#define MPI_COMM_NULL 0
#define MPI_Request int
#else
#define HS_MPI_CALL(fctCall)                                            \
  { int rc = MPI_##fctCall; if (rc != MPI_SUCCESS) throw hs::mpi::Exception(__PRETTY_FUNCTION__,rc); }
#define HS_MPI_NOTHROW(fctCall)                                            \
  { int rc = MPI_##fctCall; if (rc != MPI_SUCCESS) { std::cerr << "haystack mpi error at " << __PRETTY_FUNCTION__ << std::endl; exit(1); }}
#endif



namespace hs {

  vec3f Comm::allReduceMin(vec3f v) const
  {
    return vec3f(allReduceMin(v.x),allReduceMin(v.y),allReduceMin(v.z));
  }
    
  vec3f Comm::allReduceMax(vec3f v) const
  {
    return vec3f(allReduceMax(v.x),allReduceMax(v.y),allReduceMax(v.z));
  }


  namespace mpi {
#if HS_FAKE_MPI
    inline std::string mpiErrorString(int rc) { return "HS_FAKE_MPI"; }
#else
    inline std::string mpiErrorString(int rc)
    {
      char s[MPI_MAX_ERROR_STRING];
      memset(s,0,MPI_MAX_ERROR_STRING);
      int len = MPI_MAX_ERROR_STRING;
      MPI_Error_string(rc,s,&len);
      return s;
    }
#endif  
    struct Exception : public std::runtime_error {
      Exception(const std::string &where, int rc)
        : std::runtime_error("#hs.mpi (@"+where+") : " + mpiErrorString(rc))
      {}
    };
    struct Comm : public hs::Comm {
      Comm();
      ~Comm() override;
      
      // /*! equivalent of MPI_Comm_split - splits this comm into
      //     possibly multiple comms, each one containing exactly those
      //     former ranks of the same color. E.g. split(old.rank > 0)
      //     would have rank 0 get a communicator that contains only
      //     itself, and all others get a communicator that contains all
      //     other former ranks */
      // Comm split(int color);
      
      void  assertValid() const;
      int   allReduceMax(int value) const override;
      int   allReduceMin(int value) const override;
      float allReduceMax(float value) const override;
      float allReduceMin(float value) const override;
      int   allReduceAdd(int value) const override;
      float allReduceAdd(float value) const override;
      void  allGather(const size_t &ulong,
                      std::vector<size_t> &allUlongs) const override;
      void barrier() const override;

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
      void bc_send(const void *ptr, size_t numBytes) const override;
      
      /*! receive side of a broadcast - must be called on all ranks >
        0, and match a bc_send on rank 0 */
      void bc_recv(void *ptr, size_t numBytes) const override;

#if HS_MPI
      MPI_Comm comm = MPI_COMM_NULL;
#endif
    };

    template<typename T>
    inline void Comm::recv(int fromRank, int tag,
                           T *buffer, int numItems, MPI_Request &req)
    {
      HS_MPI_CALL(Irecv(buffer,
                        numItems*sizeof(T),MPI_BYTE,
                        fromRank,0x123+tag,comm,&req));
    }
    template<typename T>
    inline void Comm::send(int toRank, int tag,
                           const T *buffer, int numItems, MPI_Request &req)
    {
      HS_MPI_CALL(Isend(buffer,
                        numItems*sizeof(T),MPI_BYTE,
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
    

    
    Comm::Comm()
    {
#if HS_FAKE_MPI
      rank = 0;
      size = 1;
#else
      HS_MPI_CALL(Comm_dup(MPI_COMM_WORLD,&comm));
      HS_MPI_CALL(Comm_rank(comm,&rank));
      HS_MPI_CALL(Comm_size(comm,&size));
#endif
    }

    /*! master's send side of broadcast - must be done on rank 0,
      and matched by bc_recv on all workers */
    void Comm::bc_send(const void *data, size_t numBytes) const
    {
      HS_MPI_CALL(Bcast((void *)data,numBytes,MPI_BYTE,0,comm));
    }
    
    /*! receive side of a broadcast - must be called on all ranks >
      0, and match a bc_send on rank 0 */
    void Comm::bc_recv(void *data, size_t numBytes) const
    {
      HS_MPI_CALL(Bcast(data,numBytes,MPI_BYTE,0,comm));
    }
    
    void Comm::assertValid() const
    {
#if HS_FAKE_MPI
      // no mpi...
#else
      if (comm == MPI_COMM_NULL)
        throw std::runtime_error(std::string(__PRETTY_FUNCTION__)
                                 +" : not a valid mpi communicator"); 
#endif
    }
    
    int Comm::allReduceAdd(int value) const
    {
      int result = value;
      HS_MPI_CALL(Allreduce(&value,&result,1,MPI_INT,MPI_SUM,comm));
      return result;
    }

    float Comm::allReduceAdd(float value) const
    {
      float result = value;
      HS_MPI_CALL(Allreduce(&value,&result,1,MPI_FLOAT,MPI_SUM,comm));
      return result;
    }

    int Comm::allReduceMin(int value) const
    {
      int result = value;
      HS_MPI_CALL(Allreduce(&value,&result,1,MPI_INT,MPI_MIN,comm));
      return result;
    }

    int Comm::allReduceMax(int value) const
    {
      int result = value;
      HS_MPI_CALL(Allreduce(&value,&result,1,MPI_INT,MPI_MAX,comm));
      return result;
    }
    
    float Comm::allReduceMax(float value) const
    {
      float result = value;
      HS_MPI_CALL(Allreduce(&value,&result,1,MPI_FLOAT,MPI_MAX,comm));
      return result;
    }
    
    float Comm::allReduceMin(float value) const
    {
      float result = value;
      HS_MPI_CALL(Allreduce(&value,&result,1,MPI_FLOAT,MPI_MIN,comm));
      return result;
    }

    void Comm::allGather(int *allValues, int myValue)
    {
      HS_MPI_CALL(Allgather(&myValue,1,MPI_INT,allValues,1,MPI_INT,comm));
    }
    
    void Comm::allGather(int *allValues, const int *myValues, int numMyValues)
    {
      HS_MPI_CALL(Allgather(myValues,numMyValues,MPI_INT,allValues,numMyValues,MPI_INT,comm));
    }
    
    /*! free/close this communicator */
    Comm::~Comm()
    {
      HS_MPI_NOTHROW(Comm_free(&comm));
    }
      
    void Comm::barrier() const
    {
      HS_MPI_CALL(Barrier(comm));
    }

    void Comm::allGather(const size_t &ulong,
                         std::vector<size_t> &allUlongs) const
    {
      MPI_Allgather(&ulong,sizeof(ulong),MPI_BYTE,
                    allUlongs.data(),sizeof(ulong),MPI_BYTE,
                    comm);
    }
  }

  void Comm::mpiInit(int &ac, char **&av)
  {
#if HS_MPI
    int required = MPI_THREAD_MULTIPLE;
    int provided = 0;
    HS_MPI_CALL(Init_thread(&ac,&av,required,&provided));
    if (provided != required)
      throw std::runtime_error("MPI runtime does not provide threading support");
#endif
  }
  
  void Comm::mpiFinalize()
  {
#if HS_MPI
    HS_MPI_CALL(Finalize());
#endif
  }
  
  Comm::SP Comm::createMPIWorld()
  {
    return std::make_shared<mpi::Comm>();
  }
  
}
