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
      if (onClientSide)
        send(fd,&value,sizeof(value),0);
      else
        recv(fd,&value,sizeof(value),0);
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
    PING;

    struct hostent *he = gethostbyname(server.c_str());
    assert(he);

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    // sa.sin_addr.s_addr = INADDR_ANY;
    memcpy(&sa.sin_addr.s_addr,he->h_addr,sizeof(sa.sin_addr.s_addr));
    sa.sin_port = htons(port);
    PING;
    connect(fd,(struct sockaddr*)&sa,sizeof(sa));
    PING;
    onClientSide = true;
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
    PING;
    fd = accept(boundSocket->fd,0,0);
    PING;
    onClientSide = false;
  }
  
  
  
  HSServer::HSServer(RenderEngineInterface *backend)
    : backend(backend)
  {
    SocketComm::SP boundSocket = std::make_shared<SocketComm>();
    std::cout << "server running on port " << boundSocket->port << std::endl;

    SocketComm::SP connection = std::make_shared<SocketComm>(boundSocket);
    MPIRenderEngine::runWorker(connection,backend);
  }

  HSClient::HSClient(const std::string &server, int port)
    : MPIRenderEngine(std::make_shared<SocketComm>(server,port),nullptr)
  {
    std::cout << "#hsc: hs client created..." << std::endl;
  }


  void HSClient::resize(const vec2i &fbSize, uint32_t *hostRgba)
  {
    PING;
    this->fbSize = fbSize;
    this->hostRgba = hostRgba;
  }
  
  void HSClient::renderFrame()
  {
  }
  
}

