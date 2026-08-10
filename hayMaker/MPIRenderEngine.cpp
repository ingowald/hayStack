// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayMaker/MPIRenderEngine.h"

// #define LOGGING 1
#if LOGGING
# define LOG(a) a
#else
# define LOG(a)
#endif

/* parallel renderer abstraction */
namespace hm {
  
  const int endOfMessageConstant = 0x12345;
  
  typedef enum
    {
     SET_CAMERA = 0 ,
     SET_LIGHTS,
     RESIZE,
     RENDER_FRAME,
     SCREEN_SHOT,
     TERMINATE,
     SET_XF,
     RESET_ACCUMULATION,
#if HS_USE_MULTI_SCATTERING
     SET_VOLUME_SCATTER,
#endif
     // SET_ISO,
     MAX_VALID_COMMANDS
    } CommandTag;
  const char *commandNames[]
  = {
     "SET_CAMERA",
     "SET_LIGHTS",
     "RESIZE",
     "RENDER_FRAME",
     "SCREEN_SHOT",
     "TERMINATE",
     "SET_XF",
     "RESET_ACCUMULATION",
#if HS_USE_MULTI_SCATTERING
     "SET_VOLUME_SCATTER",
#endif
  };

  
  const char *cmdName[] = {
                           "set_camera",
                           "set_lights",
                           "resize",
                           "render_frame",
                           "screen-shot",
                           "terminate",
                           "set_xf",
                           "reset_accum",
#if HS_USE_MULTI_SCATTERING
                           "set_volume_scatter",
#endif
                           "<EOL>" };
                           
  
  MPIRenderEngine::MPIRenderEngine(Comm &comm,
                           RenderEngineInterface *passThrough)
    : comm(comm),
      passThrough(passThrough)
  {
    int handShake = 29031974;
    comm.barrier();
    sendToWorkers(handShake);
  }

  struct WorkerLoop 
  {
    WorkerLoop(Comm &comm,
               RenderEngineInterface *client);

    /*! the 'main loop' that receives and executes cmmands sent by the master */
    void runWorker();
    
  private:
    template<typename T>
    void sendToWorkers(const std::vector<T> &t);
    
    template<typename T>
    void sendToWorkers(const T &t);
    
    /*! @{ command handlers - each corresponds to exactly one command
        sent my the master */
    void cmd_terminate();
    void cmd_renderFrame();
    void cmd_resize();
    void cmd_resetAccumulation();
    void cmd_setCamera();
    void cmd_setTransferFunction();
#if HS_USE_MULTI_SCATTERING
    void cmd_setVolumeScatterSettings();
#endif
    void cmd_setISO();
    void cmd_setShadeMode();
    void cmd_setNodeSelection();
    void cmd_screenShot();
    void cmd_setLights();
    /* @} */

    template<typename T>
    void fromMaster(std::vector<T> &t);
    template<typename T>
    void fromMaster(T &t);
    
    Comm &comm;
    RenderEngineInterface *renderer;

  private:
    int eomIdentifierBase = 0x12345;
    void checkEndOfMessage();
    void sendEndOfMessage();
  };
  
  template<typename T>
  void WorkerLoop::fromMaster(T &t)
  {
    comm.bc_recv(&t,sizeof(T));
  }
  
  template<typename T>
  void WorkerLoop::fromMaster(std::vector<T> &t)
  {
    size_t s;
    fromMaster(s);
    t.resize(s);
    if (s) comm.bc_recv(t.data(),s*sizeof(T));
  }
  
  template<typename T>
  void MPIRenderEngine::sendToWorkers(const std::vector<T> &t)
  {
    size_t s = t.size();
    sendToWorkers(s);
    if (s) comm.bc_send(t.data(),s*sizeof(T));
  }
  
  template<typename T>
  void MPIRenderEngine::sendToWorkers(const T &t)
  {
    comm.bc_send(&t,sizeof(T));
  }
    
    

  WorkerLoop::WorkerLoop(Comm &comm,
                         RenderEngineInterface *renderer)
    : comm(comm),
      renderer(renderer)
  {}

  void WorkerLoop::checkEndOfMessage()
  {
    int expected_eomIdentifier = eomIdentifierBase++;
    int eomIdentifier;
    fromMaster(eomIdentifier);
    if(eomIdentifier != expected_eomIdentifier) {
      throw std::runtime_error("invalid end of message!?");
    }
  }

  void MPIRenderEngine::sendEndOfMessage()
  {
    int eomIdentifier = eomIdentifierBase++;
    sendToWorkers(eomIdentifier);
  }
  
  // ==================================================================
  void MPIRenderEngine::screenShot()
  {
    // ------------------------------------------------------------------
    // send request....
    // ------------------------------------------------------------------
    int cmd = SCREEN_SHOT;
    sendToWorkers(cmd);
    sendEndOfMessage();

    // ------------------------------------------------------------------
    // and do our own....
    // ------------------------------------------------------------------
    if (passThrough) passThrough->screenShot();

  }

  void WorkerLoop::cmd_screenShot()
  {
    // ------------------------------------------------------------------
    // get args....
    // ------------------------------------------------------------------
    checkEndOfMessage();
    ;
    // ------------------------------------------------------------------
    // and execute
    // ------------------------------------------------------------------
    renderer->screenShot();
  }
    

  // ==================================================================
  
  void MPIRenderEngine::resetAccumulation()
  {
    // ------------------------------------------------------------------
    // send request....
    // ------------------------------------------------------------------
    int cmd = RESET_ACCUMULATION;
    sendToWorkers(cmd);
    sendEndOfMessage();
      
    // ------------------------------------------------------------------
    // and do our own....
    // ------------------------------------------------------------------
    if (passThrough) passThrough->resetAccumulation();
  }

  void WorkerLoop::cmd_resetAccumulation()
  {
    // ------------------------------------------------------------------
    // get args....
    // ------------------------------------------------------------------
    checkEndOfMessage();
    ;
    // ------------------------------------------------------------------
    // and execute
    // ------------------------------------------------------------------
    renderer->resetAccumulation();
  }
    

  // ==================================================================
  void MPIRenderEngine::terminate()
  {
    // ------------------------------------------------------------------
    // send request....
    // ------------------------------------------------------------------
    int cmd = TERMINATE;
    sendToWorkers(cmd);
    sendEndOfMessage();
      
    // ------------------------------------------------------------------
    // and do our own....
    // ------------------------------------------------------------------
    // MPI_Finalize();
    // hs::mpi::finalize();//comm.finalize();
    // exit(0);
  }
    
  void WorkerLoop::cmd_terminate()
  {
    // ------------------------------------------------------------------
    // get args....
    // ------------------------------------------------------------------
    checkEndOfMessage();
    ;
    // ------------------------------------------------------------------
    // and do our own....
    // ------------------------------------------------------------------
    // MPI_Finalize();
    renderer->terminate();
    // comm.finalize();
    // exit(0);
  }



  // ==================================================================

  void MPIRenderEngine::renderFrame()
  {
    // ------------------------------------------------------------------
    // send request....
    // ------------------------------------------------------------------
    int cmd = RENDER_FRAME;
    sendToWorkers(cmd);
    sendEndOfMessage();
      
    // ------------------------------------------------------------------
    // and do our own....
    // ------------------------------------------------------------------
    if (passThrough) passThrough->renderFrame();
  }

  void WorkerLoop::cmd_renderFrame()
  {
    // ------------------------------------------------------------------
    // get args....
    // ------------------------------------------------------------------
    checkEndOfMessage();
    ;
    // ------------------------------------------------------------------
    // and execute
    // ------------------------------------------------------------------
    renderer->renderFrame();

    // throw std::runtime_error("HARD EXIT FOR DEBUG");
  }

  // ==================================================================

  void MPIRenderEngine::resize(const vec2i &newSize, uint32_t *appFB)
  {
    // ------------------------------------------------------------------
    // send request....
    // ------------------------------------------------------------------
    int cmd = RESIZE;
    sendToWorkers(cmd);
    sendToWorkers(newSize);
    sendEndOfMessage();
    
    // ------------------------------------------------------------------
    // and do our own....
    // ------------------------------------------------------------------
    if (passThrough) passThrough->resize(newSize,appFB);

    comm.barrier();
  }

  void WorkerLoop::cmd_resize()
  {
    // ------------------------------------------------------------------
    // get args....
    // ------------------------------------------------------------------
    vec2i newSize;
    fromMaster(newSize);
    checkEndOfMessage();
    // ------------------------------------------------------------------
    // and execute
    // ------------------------------------------------------------------
    
    renderer->resize(newSize,nullptr);

    comm.barrier();
  }

  // ==================================================================

  void MPIRenderEngine::setCamera(const Camera &camera)
  {
    // ------------------------------------------------------------------
    // send request....
    // ------------------------------------------------------------------
    int cmd = SET_CAMERA;
    sendToWorkers(cmd);
    sendToWorkers(camera);
    sendEndOfMessage();
    // ------------------------------------------------------------------
    // and do our own....
    // ------------------------------------------------------------------
    if (passThrough) passThrough->setCamera(camera);
  }

  void WorkerLoop::cmd_setCamera()
  {
    // ------------------------------------------------------------------
    // get args....
    // ------------------------------------------------------------------
    Camera camera;
    fromMaster(camera);
    checkEndOfMessage();

    // ------------------------------------------------------------------
    // and execute
    // ------------------------------------------------------------------
    renderer->setCamera(camera);
  }

  // ==================================================================

  void MPIRenderEngine::setTransferFunction(const TransferFunction &xf)
  {
    // ------------------------------------------------------------------
    // send request....
    // ------------------------------------------------------------------
    int cmd = SET_XF;
    sendToWorkers(cmd);
    sendToWorkers(xf.domain);
    sendToWorkers(xf.baseDensity);
    sendToWorkers(xf.colorMap);
    sendEndOfMessage();
    // ------------------------------------------------------------------
    // and do our own....
    // ------------------------------------------------------------------
    if (passThrough)
      passThrough->setTransferFunction(xf);
  }

  void WorkerLoop::cmd_setTransferFunction()
  {
    // ------------------------------------------------------------------
    // get args....
    // ------------------------------------------------------------------
    TransferFunction xf;
    fromMaster(xf.domain);
    fromMaster(xf.baseDensity);
    fromMaster(xf.colorMap);
    checkEndOfMessage();

    // ------------------------------------------------------------------
    // and execute
    // ------------------------------------------------------------------
    renderer->setTransferFunction(xf);
  }

#if HS_USE_MULTI_SCATTERING
  // ==================================================================

  void MPIRenderEngine::setVolumeScatterSettings(const VolumeScatterSettings &settings)
  {
    int cmd = SET_VOLUME_SCATTER;
    sendToWorkers(cmd);
    sendToWorkers(settings);
    sendEndOfMessage();
    if (passThrough)
      passThrough->setVolumeScatterSettings(settings);
  }

  void WorkerLoop::cmd_setVolumeScatterSettings()
  {
    VolumeScatterSettings settings;
    fromMaster(settings);
    checkEndOfMessage();
    renderer->setVolumeScatterSettings(settings);
  }
#endif
  
  // ==================================================================

  // void MPIRenderEngine::setISO(int numActive,
  //                           const std::vector<int> &active,
  //                           const std::vector<float> &values,
  //                           const std::vector<vec3f> &colors)
  // {
  //   // ------------------------------------------------------------------
  //   // send request....
  //   // ------------------------------------------------------------------
  //   int cmd = SET_ISO;
  //   sendToWorkers(cmd);
  //   sendToWorkers(active);
  //   sendToWorkers(values);
  //   sendToWorkers(colors);
  //   sendEndOfMessage();
  //   // ------------------------------------------------------------------
  //   // and do our own....
  //   // ------------------------------------------------------------------
  //   // std::cout << "skipping iso for now; not yet implemented in renderer ... " << std::endl;
  //   // if (passThrough) passThrough->setISO(numActive,active,values,colors);
  // }

  // void WorkerLoop::cmd_setISO()
  // {
  //   // ------------------------------------------------------------------
  //   // get args....
  //   // ------------------------------------------------------------------
  //   int count;
  //   int numActive;
  //   std::vector<int> active;
  //   std::vector<float> values;
  //   std::vector<vec3f> colors;
  //   fromMaster(active);
  //   fromMaster(values);
  //   fromMaster(colors);
  //   checkEndOfMessage();

  //   // ------------------------------------------------------------------
  //   // and execute
  //   // ------------------------------------------------------------------
  //   // std::cout << "skipping iso for now; not yet implemented in renderer ... " << std::endl;
  //   // renderer->setISO(numActive,active,values,colors);
  // }

  // ==================================================================
  
  void MPIRenderEngine::setLights(float ambient,
                              const std::vector<PointLight> &pointLights,
                              const std::vector<DirLight> &dirLights)
  {
    // ------------------------------------------------------------------
    // send request....
    // ------------------------------------------------------------------
    int cmd = SET_LIGHTS;
    sendToWorkers(cmd);
    sendToWorkers(ambient);
    sendToWorkers(pointLights);
    sendToWorkers(dirLights);
    sendEndOfMessage();
    
    // ------------------------------------------------------------------
    // and do our own....
    // ------------------------------------------------------------------
    if (passThrough) passThrough->setLights(ambient,pointLights,dirLights);
  }

  void WorkerLoop::cmd_setLights()
  {
    // ------------------------------------------------------------------
    // get args....
    // ------------------------------------------------------------------
    float ambient;
    fromMaster(ambient);
    
    std::vector<PointLight> pointLights;
    fromMaster(pointLights);
    
    std::vector<DirLight> dirLights;
    fromMaster(dirLights);
    
    checkEndOfMessage();
    
    // ------------------------------------------------------------------
    // and execute
    // ------------------------------------------------------------------
    renderer->setLights(ambient,pointLights,dirLights);
  }

  // ==================================================================
  void WorkerLoop::runWorker()
  {
    int handShake = -1;
    comm.barrier();
    fromMaster(handShake);
    if (handShake != 29031974)
      throw std::runtime_error("could not handshake with master");
    
    while (1) {
      int cmd = -1;
      fromMaster(cmd);
      LOG(printf("#mi(%i) worker got cmd tag %i (%s)\n",
                 comm.workersRank,cmd,
                 ((cmd>=0 && cmd<MAX_VALID_COMMANDS)
                  ?commandNames[cmd]
                  :"<not a valid tag>")));
      // PRINT(cmd); PRINT(cmdName[cmd]);
      switch(cmd) {
      case SET_CAMERA:
        cmd_setCamera();
        break;
      case TERMINATE:
        cmd_terminate();
        return;
        // break;
      case RESIZE:
        cmd_resize();
        break;
      case RENDER_FRAME:
        cmd_renderFrame();
        break;
      case RESET_ACCUMULATION:
        cmd_resetAccumulation();
        break;
      case SCREEN_SHOT:
        cmd_screenShot();
        break;
      case SET_XF:
        cmd_setTransferFunction();
        break;
#if HS_USE_MULTI_SCATTERING
      case SET_VOLUME_SCATTER:
        cmd_setVolumeScatterSettings();
        break;
#endif
      // case SET_ISO:
      //   cmd_setISO();
      //   break;
      case SET_LIGHTS:
        cmd_setLights();
        break;
      default:
        throw std::runtime_error("unknown command ...");
      }
    }
  }
  

  void MPIRenderEngine::runWorker(Comm &comms,
                              RenderEngineInterface *client)
  {
    WorkerLoop helper(comms,client);
    helper.runWorker();
  }
  
}
