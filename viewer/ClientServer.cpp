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
    fd = socket(AF_INET,SOCK_STREAM,0);
    if (fd == -1)
      throw std::runtime_error("socketcomm: could not create socket");
    
    struct hostent *he = gethostbyname(server.c_str());
    assert(he);

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    memcpy(&sa.sin_addr.s_addr,he->h_addr,sizeof(sa.sin_addr.s_addr));
    sa.sin_port = htons(port);
    int rc = connect(fd,(struct sockaddr*)&sa,sizeof(sa));
    if (rc != 0)
      perror("socketcomm: error in connect()");
    onClientSide = true;
  }
  
  SocketComm::SocketComm()
  {
    fd = socket(AF_INET,SOCK_STREAM,0);
    if (fd == -1)
      throw std::runtime_error("socketcomm: could not create socket");
    
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    port = 8628;
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = INADDR_ANY;
    sa.sin_port = htons(port);      

    bind(fd,(struct sockaddr*)&sa,sizeof(sa));
    port = ntohs(sa.sin_port);
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
    std::cout << "#hs: server running on port "
              << boundSocket->port << std::endl;

    comm = std::make_shared<SocketComm>(boundSocket);
    MPIRenderEngine::runWorker(comm,backend);
  }

  
  HSClient::HSClient(const std::string &server, int port)
    : MPIRenderEngine(std::make_shared<SocketComm>(server,port),nullptr)
  {
    std::cout << "#hsc: remote render client successfully connected to "
              << server << "::" << port << std::endl;
  }

  void HSClient::resize(const vec2i &fbSize,
                        uint32_t *fbColor,
                        float *fbDepth)
  {
    this->fbSize = fbSize;
    this->hostColor = fbColor;
    this->hostDepth = fbDepth;
    MPIRenderEngine::resize(fbSize,fbColor,fbDepth);
  }
  
  void HSClient::renderFrame()
  {
    MPIRenderEngine::renderFrame();
    size_t sizeOfPixel = 4;
    size_t numBytes = fbSize.x*fbSize.y*sizeOfPixel;
    SocketComm *sc = (SocketComm *)this->comm.get();
    {    
      uint8_t *ptr = (uint8_t*)hostColor;
      size_t nltr = numBytes;
      while (nltr) {
        size_t nr = recv(sc->fd,ptr,nltr,0);
        if (nr == 0) {
          throw std::runtime_error("received incomplete frame");
        }
        ptr += nr;
        nltr -= nr;
      }
    }
    {
      uint8_t *ptr = (uint8_t*)hostDepth;
      size_t nltr = numBytes;
      while (nltr) {
        size_t nr = recv(sc->fd,ptr,nltr,0);
        if (nr == 0) {
          throw std::runtime_error("received incomplete frame");
        }
        ptr += nr;
        nltr -= nr;
      }
    }
  }

  HSServerREI::HSServerREI(HSServer *server,
                           RenderEngineInterface *pt)
    : PassthroughREI(pt),
      server(server)
  {}
  
  void HSServerREI::renderFrame()
  {
    PassthroughREI::renderFrame();
    auto fbSize = server->fb.size;
    size_t sizeOfPixel = 4;
    size_t numBytes = fbSize.x*fbSize.y*sizeOfPixel;

    {
      size_t numLeftToSend = numBytes;
      uint8_t *ptr = (uint8_t *)server->fb.color.data();
      while (numLeftToSend > 0) {
        size_t numSent = send(server->comm->fd,ptr,numLeftToSend,0);
        if (numSent == 0)
          throw std::runtime_error("could not send anything!?");
        ptr += numSent;
        numLeftToSend -= numSent;
      }
    }
    {
      size_t numLeftToSend = numBytes;
      uint8_t *ptr = (uint8_t *)server->fb.depth.data();
      while (numLeftToSend > 0) {
        size_t numSent = send(server->comm->fd,ptr,numLeftToSend,0);
        if (numSent == 0)
          throw std::runtime_error("could not send anything!?");
        ptr += numSent;
        numLeftToSend -= numSent;
      }
    }
  }
  
  void HSServerREI::resize(const vec2i &fbSize,
                           uint32_t *fbColor,
                           float *fbDepth)
  {
    server->fb.color.resize(fbSize.x*fbSize.y);
    server->fb.depth.resize(fbSize.x*fbSize.y);
    server->fb.size = fbSize;
    PassthroughREI::resize(fbSize,
                           server->fb.color.data(),
                           server->fb.depth.data());
  }
  
}

