// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "ClientServer.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

namespace hm {

  struct SocketComm : public hs::Comm {
    typedef std::shared_ptr<SocketComm> SP;
    
    SocketComm(const std::string &server, int port);
    SocketComm();
    SocketComm(SocketComm::SP boundSocket);

    template<typename T> 
    void dummyReduce(T &value) const
    {
      if (onClientSide) {
        recv(fd,&value,sizeof(value),0);
      } else {
        send(fd,&value,sizeof(value),0);
      }
    }

    /* all WORKERS do the reduction - but for default MPI comm this
       includes rank 0, too */
    int   allReduceMax(int value) const override
    { dummyReduce(value); return value; }
    int   allReduceMin(int value) const override
    { dummyReduce(value); return value; }
    int   allReduceAdd(int value) const override
    { dummyReduce(value); return value; }
    float allReduceAdd(float value) const override
    { dummyReduce(value); return value; }
    float allReduceMax(float value) const override
    { dummyReduce(value); return value; }
    float allReduceMin(float value) const override
    { dummyReduce(value); return value; }
    void barrier() const override {}

    void  allGather(const size_t &ulong,
                    std::vector<size_t> &allUlongs) const override
    {
      assert(allUlongs.size() == 1);
      allUlongs[0] = ulong;
    }
    
    void bc_send(const void *data, size_t size) const override
    {
      assert(!onClientSide);
      send(fd,data,size,0);
    }
    
    void bc_recv(void *data, size_t size) const override
    {
      assert(onClientSide);
      recv(fd,data,size,0);
    }
    
    bool onClientSide;
    int port   = -1;
    int fd = -1;
  };

  SocketComm::SocketComm(const std::string &server, int port)
  {
    PING;
    fd = socket(AF_INET,SOCK_STREAM,0);

    PING;
    struct hostent *he = gethostbyname(server.c_str());
    assert(he);

    PING;
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    // sa.sin_addr.s_addr = INADDR_ANY;
    memcpy(&sa.sin_addr.s_addr,he->h_addr,sizeof(sa.sin_addr.s_addr));
    sa.sin_port = htons(port);
    connect(fd,(struct sockaddr*)&sa,sizeof(sa));
    onClientSide = true;
    PING;
  }
  
  SocketComm::SocketComm()
  {
    fd = socket(AF_INET,SOCK_STREAM,0);
    
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    port = 8628;
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;                 // IPv4
    sa.sin_addr.s_addr = INADDR_ANY;          // Bind to all available network interfaces
    sa.sin_port = htons(port);      

    bind(fd,(struct sockaddr*)&sa,sizeof(sa));
    port = ntohs(sa.sin_port);
    PRINT(port);
    onClientSide = false;

    listen(fd,1);
  }
  
  SocketComm::SocketComm(SocketComm::SP boundSocket)
  {
    fd = accept(boundSocket->fd,0,0);
    onClientSide = false;
  }
  
  
  
  HSServer::HSServer(RenderEngineInterface *_backend)
    : backend(new HSServerREI(this,_backend))
  {
    SocketComm::SP boundSocket = std::make_shared<SocketComm>();
    std::cout << "server running on port " << boundSocket->port << std::endl;

    comm = std::make_shared<SocketComm>(boundSocket);
    MPIRenderEngine::runWorker(comm,backend);
  }

  
  HSClient::HSClient(const std::string &server, int port)
    : MPIRenderEngine(std::make_shared<SocketComm>(server,port),nullptr)
  {
    std::cout << "#hsc: hs client created..." << std::endl;
    // socket_comm = 
  }

  void HSClient::resize(const vec2i &fbSize, uint32_t *hostRgba)
  {
    this->fbSize = fbSize;
    // fb.color.resize(fbSize.x*fbSize.y);
    this->hostRgba = hostRgba;
    MPIRenderEngine::resize(fbSize,hostRgba);
  }
  
  void HSClient::renderFrame()
  {
    PING;
    PRINT(fbSize);
    MPIRenderEngine::renderFrame();
    size_t sizeOfPixel = 4;
    size_t numBytes = fbSize.x*fbSize.y*sizeOfPixel;
    PING;
    SocketComm *sc = (SocketComm *)this->comm.get();
    std::cout << "client RECV " << numBytes << std::endl;
    uint8_t *ptr = (uint8_t*)hostRgba;
    size_t nltr = numBytes;
    while (nltr) {
      size_t nr = recv(sc->fd,ptr,nltr,0);
      if (nr == 0) {
        throw std::runtime_error("received incomplete frame");
      }
      ptr += nr;
      nltr -= nr;
    }
    // recv(comm->fd,fb.color.data(),numBytes,0);
    PING;
  }

  HSServerREI::HSServerREI(HSServer *server,
                           RenderEngineInterface *pt)
    : PassthroughREI(pt),
      server(server)
  {
    PING; PRINT(this);
  }
  
  void HSServerREI::renderFrame()
  {
    PING;
    PassthroughREI::renderFrame();
    PING;
    auto fbSize = server->fb.size;
    size_t sizeOfPixel = 4;
    size_t numBytes = fbSize.x*fbSize.y*sizeOfPixel;
    PRINT(fbSize);
    PRINT(server->fb.color.size());
    std::cout << "server SEND " << numBytes << std::endl;
    
    size_t numLeftToSend = numBytes;
    uint8_t *ptr = (uint8_t *)server->fb.color.data();
    while (numLeftToSend > 0) {
      size_t numSent = send(server->comm->fd,ptr,numLeftToSend,0);
      if (numSent == 0)
        throw std::runtime_error("could not send anything!?");
      ptr += numSent;
      numLeftToSend -= numSent;
    }
    // send(server->comm->fd,server->fb.color.data(),numBytes,0);
  }
  
  void HSServerREI::resize(const vec2i &fbSize, uint32_t *fbPointer)
  {
    PING;
    server->fb.color.resize(fbSize.x*fbSize.y);
    server->fb.size = fbSize;
    PassthroughREI::resize(fbSize,server->fb.color.data());//fbPointer);
  }
  
}

