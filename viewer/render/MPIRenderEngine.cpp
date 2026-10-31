// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "viewer/render/MPIRenderEngine.h"

// #define LOGGING 1
#if LOGGING
# define LOG(a) a
#else
# define LOG(a)
#endif

/* parallel renderer abstraction */
namespace hm {
  using namespace hs;
  
  const int endOfMessageConstant = 0x12345;
  
  typedef enum {
    SET_CAMERA = 0,
    SET_LIGHTS,
    RESIZE,
    RENDER_FRAME,
    TERMINATE,
    GET_WORLD_BOUNDS,
    SET_XF,
    SET_EMISSION_XF,
    RESET_ACCUMULATION,
    SET_VOLUME_SCATTER,
    MAX_VALID_COMMANDS
  } CommandTag;
  
  MPIRenderEngine::MPIRenderEngine(std::shared_ptr<Comm> comm,
                                   RenderEngineInterface *passThrough)
    : comm(comm),
      passThrough(passThrough)
  {
    int handShake = 29031974;
    comm->barrier();
    sendToWorkers(handShake);
  }

  struct WorkerLoop 
  {
    WorkerLoop(Comm::SP comm,
               RenderEngineInterface *client);

    /*! the 'main loop' that receives and executes cmmands sent by the master */
    void runWorker();
    
  private:
    // template<typename T>
    // void sendToWorkers(const std::vector<T> &t);
    
    // template<typename T>
    // void sendToWorkers(const T &t);
    
    /*! @{ command handlers - each corresponds to exactly one command
        sent my the master */
    void cmd_terminate();
    void cmd_getWorldBounds();
    void cmd_renderFrame();
    void cmd_resize();
    void cmd_resetAccumulation();
    void cmd_setCamera();
    void cmd_setTransferFunction();
    void cmd_setEmissionTransferFunction();
    void cmd_setVolumeScatterSettings();
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
    
    Comm::SP comm;
    RenderEngineInterface *renderer;

  private:
    int eomIdentifierBase = 0x12345;
    void checkEndOfMessage();
    void sendEndOfMessage();
  };
  
  template<typename T>
  void WorkerLoop::fromMaster(T &t)
  {
    comm->bc_recv(&t,sizeof(T));
  }
  
  template<typename T>
  void WorkerLoop::fromMaster(std::vector<T> &t)
  {
    size_t s;
    fromMaster(s);
    t.resize(s);
    if (s) comm->bc_recv(t.data(),s*sizeof(T));
  }
  
  template<typename T>
  void MPIRenderEngine::sendToWorkers(const std::vector<T> &t)
  {
    size_t s = t.size();
    sendToWorkers(s);
    if (s) comm->bc_send(t.data(),s*sizeof(T));
  }
  
  template<typename T>
  void MPIRenderEngine::sendToWorkers(const T &t)
  {
    comm->bc_send(&t,sizeof(T));
  }
    
    

  WorkerLoop::WorkerLoop(Comm::SP comm,
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
  BoundsData MPIRenderEngine::getWorldBounds()
  {
    // ------------------------------------------------------------------
    // send request....
    // ------------------------------------------------------------------
    int cmd = GET_WORLD_BOUNDS;
    sendToWorkers(cmd);
    sendEndOfMessage();

    BoundsData bb;
    // ------------------------------------------------------------------
    // and do our own....
    // ------------------------------------------------------------------
    if (passThrough)
      bb = passThrough->getWorldBounds();

    bb.spatial.lower = comm->allReduceMin(bb.spatial.lower);
    bb.spatial.upper = comm->allReduceMax(bb.spatial.upper);
    bb.scalars.lower = comm->allReduceMin(bb.scalars.lower);
    bb.scalars.upper = comm->allReduceMax(bb.scalars.upper);
    bb.mapped.lower  = comm->allReduceMin(bb.mapped.lower);
    bb.mapped.upper  = comm->allReduceMax(bb.mapped.upper);

    if (bb.spatial.empty()) {
      bb.spatial = {vec3f(-1.f),vec3f(+1.f)};
    }
    
    return bb;
  }
  
  void WorkerLoop::cmd_getWorldBounds()
  {
    // ------------------------------------------------------------------
    // get args....
    // ------------------------------------------------------------------
    checkEndOfMessage();
    ;
    // ------------------------------------------------------------------
    // and execute
    // ------------------------------------------------------------------
    BoundsData bb = renderer->getWorldBounds();

    bb.spatial.lower = comm->allReduceMin(bb.spatial.lower);
    bb.spatial.upper = comm->allReduceMax(bb.spatial.upper);
    bb.scalars.lower = comm->allReduceMin(bb.scalars.lower);
    bb.scalars.upper = comm->allReduceMax(bb.scalars.upper);
    bb.mapped.lower  = comm->allReduceMin(bb.mapped.lower);
    bb.mapped.upper  = comm->allReduceMax(bb.mapped.upper);
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
    // hs::mpi::finalize();//comm->finalize();
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
    // comm->finalize();
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
    if (passThrough) 
      passThrough->renderFrame();
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

  void MPIRenderEngine::resize(const vec2i &newSize,
                               uint32_t *fbPointer,
                               float *fbDepth)
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
    if (passThrough) passThrough->resize(newSize,fbPointer,fbDepth);

    comm->barrier();
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
    
    renderer->resize(newSize,nullptr,nullptr);

    comm->barrier();
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

  void MPIRenderEngine::setEmissionTransferFunction(const TransferFunction &xf)
  {
    const int cmd = SET_EMISSION_XF;
    sendToWorkers(cmd);
    sendToWorkers(xf.domain);
    sendToWorkers(xf.baseDensity);
    sendToWorkers(xf.colorMap);
    sendEndOfMessage();
    if (passThrough)
      passThrough->setEmissionTransferFunction(xf);
  }

  void WorkerLoop::cmd_setEmissionTransferFunction()
  {
    TransferFunction xf;
    fromMaster(xf.domain);
    fromMaster(xf.baseDensity);
    fromMaster(xf.colorMap);
    checkEndOfMessage();
    renderer->setEmissionTransferFunction(xf);
  }

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
  
  // ==================================================================
  void MPIRenderEngine::setLights(float ambient,
                                  const std::vector<hs::PointLight> &pointLights,
                                  const std::vector<hs::DirLight> &dirLights)
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
    
    std::vector<hs::PointLight> pointLights;
    fromMaster(pointLights);
    
    std::vector<hs::DirLight> dirLights;
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
    comm->barrier();
    fromMaster(handShake);
    if (handShake != 29031974)
      throw std::runtime_error("could not handshake with master");
    
    while (1) {
      int cmd = -1;
      fromMaster(cmd);
      switch(cmd) {
      case SET_CAMERA:
        cmd_setCamera();
        break;
      case GET_WORLD_BOUNDS:
        cmd_getWorldBounds();
        break;
      case TERMINATE:
        cmd_terminate();
        return;
      case RESIZE:
        cmd_resize();
        break;
      case RENDER_FRAME:
        cmd_renderFrame();
        break;
      case RESET_ACCUMULATION:
        cmd_resetAccumulation();
        break;
      case SET_XF:
        cmd_setTransferFunction();
        break;
      case SET_EMISSION_XF:
        cmd_setEmissionTransferFunction();
        break;
      case SET_VOLUME_SCATTER:
        cmd_setVolumeScatterSettings();
        break;
      case SET_LIGHTS:
        cmd_setLights();
        break;
      default:
        throw std::runtime_error("unknown command ...");
      }
    }
  }
  

  void MPIRenderEngine::runWorker(std::shared_ptr<Comm> comm,
                                  RenderEngineInterface *client)
  {
    WorkerLoop helper(comm,client);
    helper.runWorker();
  }
  
}
