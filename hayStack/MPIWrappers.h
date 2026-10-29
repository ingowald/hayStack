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
#include <stdexcept>
#include <unistd.h>

namespace hs {

  /*! this implements a COMMUNICATOR abstraction like in MPI; in
      particular it assumes that all ranks on this communicator are
      'equal' (ie, there is no 'master' vs 'workers') */
  struct Comm {
    typedef std::shared_ptr<Comm> SP;

    static Comm::SP createMPIWorld();
    static void mpiInit(int &ac, char **&av);
    static void mpiFinalize();
    
    virtual ~Comm() = default;

    virtual int   allReduceMax(int value) const = 0;
    virtual int   allReduceMin(int value) const = 0;
    virtual int   allReduceAdd(int value) const = 0;
    virtual float allReduceAdd(float value) const = 0;
    virtual float allReduceMax(float value) const = 0;
    virtual float allReduceMin(float value) const = 0;
    virtual void barrier() const = 0;
    
    /*! may only be called on 'master rank' (ie, rank 0 in a mpi
        communicator, or on the 'master' size of a socket or
        intercommunicator. depending on what comm fabric is used this
        _has_ to be matched by corresponding bc_recv() calls on all
        other ranks of the respective collective communicator (ie, all
        ranks>0 in a regular mpi communicator, or the remote side
        socket, etc) */
    virtual void bc_send(const void *data, size_t size) const = 0;
    virtual void bc_recv(void *data, size_t size) const = 0;
    
    vec3f allReduceMax(vec3f value) const;
    vec3f allReduceMin(vec3f value) const;

    int rank = 0;
    int size = 1;
  };


  // struct RemoteWorkersInterface {
  //   virtual void bc(void *data, size_t size) = 0;
  //   virtual void send(int whoTo,   void *data, size_t size) = 0;
  //   virtual void recv(int whoFrom, void *data, size_t size) = 0;
  // };
  // struct MPICommClientServer : public ClientServer {
  // };
  // struct SocketClientServer : public ClientServer {
  // };
}
