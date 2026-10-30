// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "viewer/render/HayMaker.h"
#include "hayStack/loader/DataLoader.h"
#if HS_SERVER
# include "viewer/ClientServer.h"
#endif
#if HS_VIEWER
# include "viewer/Viewer.h"
#else
# define STB_IMAGE_WRITE_IMPLEMENTATION 1
# define STB_IMAGE_IMPLEMENTATION 1
#endif
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"

namespace hm {

  double t_last_render;
  
  typedef enum { DPMODE_NOT_SPECIFIED,
                 DPMODE_DATA_PARALLEL,
                 DPMODE_DATA_REPLICATED } DPMode;

  struct FromCL {
    /*! data groups per rank. '0' means 'auto - use few as we can, as
      many as we have to fit for given number of ranks */
    int dpr = 0;
    /*! num data groups. '0' will become '1' by default, but allows us
        for specifying 'hasn't been specified', which in turn allows
        the mpi mode to set it to either '1' or '1 per rank' depending
        on choesn dpmode */
    int ndg = 0;

    bool forceSingleGPU = false;
    
    DPMode dpMode = DPMODE_NOT_SPECIFIED;
    
    /*! which color map to use for color mapping (if applicable) */
    int cmID = 0;
    
    bool mergeUnstructuredMeshes = false;
    vec4f bgColor { NAN, NAN, NAN, NAN };
    float ambientRadiance = .6f;
    std::string xfFileName = "";
    std::string outFileName = "hayStack.png";
    vec2i fbSize = { 800,600 };
    bool createHeadNode = false;
    int  numExtraDisplayRanks = 0;
    int  numFramesAccum = 1;
    int  spp            = 1;
    bool verbose = true;
    //    CmdLineCamera camera;
    hs::Camera camera;
    bool denoise = true;
    std::vector<hs::Camera> cameraPath;
    // struct {
    //   vec3f vp0, vp1;
    //   vec3f vi0, vi1;
    //   int numSteps = 0;
    // } cameraPath;
    bool measure = 0;
    std::string envMapFileName;
    hs::VolumeScatterSettings scatterSettings;
  };
  FromCL fromCL;
  
  // std::string FromCL::outFileName = "hayStack.png";
  // bool FromCL::measure = 0;
  // bool FromCL::verbose = true;

  inline bool verbose() { return fromCL.verbose; }
  
  void usage(const std::string &error="")
  {
    std::cout << "./hs{Offline,Viewer,ViewerQT} ... <args>" << std::endl;
    std::cout << "w/ args:" << std::endl;
    std::cout << "-xf file.xf   ; specify transfer function" << std::endl;
    if (!error.empty())
      throw std::runtime_error("fatal error: " +error);
    exit(0);
  }

  inline mini::common::vec3f get3f(char **av, int &i)
  {
    float x = std::stof(av[++i]);
    float y = std::stof(av[++i]);
    float z = std::stof(av[++i]);
    return mini::common::vec3f(x,y,z);
  }

  size_t computeHashFromString(const char *s)
  {
    size_t hash = 0;
    size_t FNV_PRIME = 0x00000100000001b3ull;
    for (int i=0;s[i] != 0;i++)
      hash = hash * FNV_PRIME ^ s[i];
    return hash;
  }
  
  std::vector<int> parseCommaSeparatedListOfInts(std::string s)
  {
    std::vector<std::string> tokens;
    int last = 0;
    while (true) {
      int pos = s.find(",");
      if (pos == s.npos) {
        tokens.push_back(s);
        break;
      } else {
        tokens.push_back(s.substr(0,pos));
        s = s.substr(pos+1);
      }
    }
    
    std::vector<int> result;
    for (auto s : tokens)
      if (s != "")
        result.push_back(std::stoi(s));
    return result;
  }

#if HS_MPI
  void determineLocalProcessID(hs::Comm::SP world, int &localRank, int &localSize)
  {
    world->barrier();
    std::vector<char> hostName(10000);
    gethostname(hostName.data(),hostName.size());
    size_t hash = computeHashFromString(hostName.data());
    std::vector<size_t> allHostHashes(world->size);
    world->allGather(hash,allHostHashes);
    // MPI_Allgather(&hash,sizeof(hash),MPI_BYTE,
    //               allHostHashes.data(),sizeof(hash),MPI_BYTE,
    //               world->comm);
    localRank = 0;
    localSize = 0;
    for (int i=0;i<world->size;i++) {
      if (allHostHashes[i] != hash) continue;
      localSize++;
      if (i < world->rank) localRank++;
    }

    for (int i=0;i<world->size;i++) {
      world->barrier();
      if (i != world->rank) continue;
      std::cout << "#hs(" << world->rank << "): determined local rank/size as "
                << localRank << "/" << localSize << std::endl;
    }
  }
#endif

  int getIntFromEnv(const char *varName, int fallback)
  {
    const char *var = getenv(varName);
    if (!var) return fallback;
    return std::stoi(var);
  }

  std::vector<int> selectGPUs(Comm::SP world, int localRank, int localSize)
  {
#ifdef __APPLE__
    return { -1 };
#endif
    std::cout << "#hs(" << world->rank << ":" << localRank << "/" << localSize << "): selecting GPUs ... " << std::endl;
    const char *hcd = getenv("HS_CUDA_DEVICES");
    if (hcd) {
      std::cout << "#hs(" << world->rank << "): found HS_CUDA_DEVICES, using this" << std::endl;
      return parseCommaSeparatedListOfInts(hcd);
    }
    const char *cvd = getenv("CUDA_VISIBLE_DEVICES");
    if (hcd) {
      std::cout << "#hs(" << world->rank << "): found CUDA_VISIBLE_DEVICES being set in env, using GPUs 0,1,... etc" << std::endl;
      auto cudaGPUs = parseCommaSeparatedListOfInts(hcd);
      std::vector<int> result;
      for (int i=0;i<cudaGPUs.size();i++)
        result.push_back(i);
      return result;
    }

    if (world->size == 1) {
      return { -1 };
    }
    
    if (localSize == 1) {
      std::cout << "#hs: single data rank per process, no dedicated gpus specified in env -> selecting gpuIDs={-1} to indicate 'any you can'" << std::endl;
      return { -1 };
    }
    
    return { localRank % localSize };
    // return { 0 };
  }
    
  inline float lerp_l(float f, float a, float b)
  { return (1.f-f)*a + f*b; }
  inline mini::common::vec3f lerp_l(float f,
                                    mini::common::vec3f a,
                                    mini::common::vec3f b)
  { return (1.f-f)*a + f*b; }
  
  // void addCameraPath(int numSteps,
  //                    CmdLineCamera c0,
  //                    CmdLineCamera c1)
  // {
  //   for (int i=0;i<numSteps;i++) {
  //     float f = i/(numSteps-1.f);
  //     CmdLineCamera c;
  //     c.vp = lerp_l(f,c0.vp,c1.vp);
  //     c.vi = lerp_l(f,c0.vi,c1.vi);
  //     c.vu = lerp_l(f,c0.vu,c1.vu);
  //     c.fovy = lerp_l(f,c0.fovy,c1.fovy);
  //     fromCL.cameraPath.push_back(c);
  //   }
  // }

  void addCamerasFromFile(const std::string &fileName)
  {
    std::ifstream in(fileName.c_str());
    if (!in)
      throw std::runtime_error("could not open cameras from '"+fileName+"'");
    std::string line;
    while (in.good()) {
      std::getline(in,line);
      hs::Camera c = fromCL.camera;
      int rc = sscanf(line.c_str(),"--camera %f %f %f %f %f %f %f %f %f -fovy %f",
                      &c.vp.x,
                      &c.vp.y,
                      &c.vp.z,
                      &c.vi.x,
                      &c.vi.y,
                      &c.vi.z,
                      &c.vu.x,
                      &c.vu.y,
                      &c.vu.z,
                      &c.perspective.fovy);
      if (rc != 10)
        break;
      fromCL.cameraPath.push_back(c);
    }
  }
  
}

using namespace hm;

int main(int ac, char **av)
{
  // /*! init ALL gpus - let's do that right away, so gpus are
  //     initailized before mpi even gets to run */
  // hs::initAllGPUs();

  // hs::mpi::init(ac, av);
  hs::Comm::mpiInit(ac, av);
  
  /*! create MPI world, or wrapper equivalent */
  hs::Comm::SP world = hs::Comm::createMPIWorld();
  
  world->barrier();
  if (world->rank == 0) {
    std::cout << "#hv: hsviewer starting up" << std::endl; fflush(0);
  }
  world->barrier();

  hs::loader::DynamicDataLoader loader(world);
  for (int i=1;i<ac;i++) {
    const std::string arg = av[i];
    if (arg[0] != '-') {
      loader.addContent(arg);
    } else if (arg == "--no-bg") {
      fromCL.bgColor = ::mini::common::vec4f(0.f);
    } else if (arg == "--denoise" || arg == "-denoise") {
      fromCL.denoise = true;
    } else if (arg == "--no-denoise" || arg == "-no-denoise"
               || arg == "--no-denoising"  || arg == "-no-denoising") {
      fromCL.denoise = false;
    } else if (arg == "--bg-color") {
      fromCL.bgColor.x = std::stof(av[++i]);
      fromCL.bgColor.y = std::stof(av[++i]);
      fromCL.bgColor.z = std::stof(av[++i]);
      fromCL.bgColor.w = std::stof(av[++i]);
    } else if (arg == "-dp" || arg == "--data-parallel") {
      fromCL.dpMode = DPMODE_DATA_PARALLEL;
    } else if (arg == "-sg" || arg == "--single-gpu") {
      fromCL.forceSingleGPU = true;
    } else if (arg == "-dp1" || arg == "-dpsg" || arg == "--data-parallel-single-gpu") {
      fromCL.dpMode = DPMODE_DATA_PARALLEL;
      fromCL.forceSingleGPU = true;
    } else if (arg == "-dr" || arg == "--data-replicated") {
      fromCL.dpMode = DPMODE_DATA_REPLICATED;
    } else if (arg == "-cm" || arg == "--color-map") {
      fromCL.cmID = std::stoi(av[++i]);
    } else if (arg == "-env" || arg == "--env-map") {
      fromCL.envMapFileName = av[++i];
      loader.sharedLights.envMap = fromCL.envMapFileName;
    } else if (arg == "--num-frames") {
      fromCL.numFramesAccum = std::stoi(av[++i]);
    } else if (arg == "--ambient") {
      fromCL.ambientRadiance = std::stof(av[++i]);
    } else if (arg == "-spp" || arg == "-ppp" || arg == "--paths-per-pixel") {
      fromCL.spp = std::stoi(av[++i]);
#if HS_USE_MULTI_SCATTERING
    } else if (arg == "--no-multi-scatter") {
      fromCL.scatterSettings.enabled = false;
    } else if (arg == "--max-volume-bounces") {
      fromCL.scatterSettings.maxVolumeBounces = std::stoi(av[++i]);
    } else if (arg == "--anisotropy") {
      fromCL.scatterSettings.medium.anisotropy = std::stof(av[++i]);
    } else if (arg == "--emission-strength") {
      fromCL.scatterSettings.medium.emissionStrength = std::stof(av[++i]);
#endif
    } else if (arg == "-mum" || arg == "--merge-unstructured-meshes" || arg == "--merge-umeshes") {
      fromCL.mergeUnstructuredMeshes = true;
    } else if (arg == "--no-mum") {
      fromCL.mergeUnstructuredMeshes = false;
    } else if (arg == "--default-radius") {
      loader.defaultRadius = std::stof(av[++i]);
    } else if (arg == "--measure") {
      fromCL.measure = true;
    } else if (arg == "-o") {
      fromCL.outFileName = av[++i];
    } else if (arg == "--dir-light") {
      mini::DirLight light;
      light.direction = get3f(av,i);
      light.radiance  = get3f(av,i);
      loader.sharedLights.directional.push_back(light);
    } else if (arg == "--quad-light") {
      mini::QuadLight light;
      light.corner   = get3f(av,i);
      light.edge0    = get3f(av,i);
      light.edge1    = get3f(av,i);
      light.emission = get3f(av,i);
      loader.sharedLights.quad.push_back(light);
    } else if (arg == "--quad-light-centered") {
      mini::QuadLight light;
      vec3f center   = get3f(av,i);
      light.edge0    = get3f(av,i);
      light.edge1    = get3f(av,i);
      light.corner   = center - 0.5f*(light.edge0+light.edge1);
      light.emission = get3f(av,i);
      loader.sharedLights.quad.push_back(light);
    } else if (arg == "--lights") {
      std::string lightsFileName = av[++i];
      std::cout << "#hs: importing lights from " << lightsFileName << std::endl;
      mini::Scene::SP lightsScene = mini::Scene::load(lightsFileName);
      for (auto light : lightsScene->quadLights)
        loader.sharedLights.quad.push_back(light);
      for (auto light : lightsScene->dirLights)
        loader.sharedLights.directional.push_back(light);
        
    } else if (arg == "--camera-pdu") {
      fromCL.camera.vp = get3f(av,i);
      fromCL.camera.vi = fromCL.camera.vp + get3f(av,i);
      fromCL.camera.vu = get3f(av,i);
    } else if (arg == "--camera-near-clip") {
      fromCL.camera.nearClip = std::stof(av[++i]);
    } else if (arg == "--camera") {
      fromCL.camera.vp = get3f(av,i);
      fromCL.camera.vi = get3f(av,i);
      fromCL.camera.vu = get3f(av,i);
    } else if (arg == "--cameras-from-file") {
      hm::addCamerasFromFile(av[++i]);
    } else if (arg == "-fovy") {
      fromCL.camera.perspective.fovy = std::stof(av[++i]);
      fromCL.camera.type = hs::Camera::PERSPECTIVE;
    } else if (arg == "-ortho" || arg == "--ortho") {
      fromCL.camera.ortho.height = std::stof(av[++i]);
      fromCL.camera.type = hs::Camera::ORTHO;
    } else if (arg == "-xf") {
      fromCL.xfFileName = av[++i];
    } else if (arg == "-res" || arg == "-os" || arg == "--output-size") {
      fromCL.fbSize.x = std::stoi(av[++i]);
      fromCL.fbSize.y = std::stoi(av[++i]);
    } else if (arg == "-ndg") {
      fromCL.ndg = std::stoi(av[++i]);
      fromCL.dpMode
        = (fromCL.ndg == 1)
        ? DPMODE_DATA_REPLICATED
        : DPMODE_DATA_PARALLEL;
    } else if (arg == "-dpr") {
      fromCL.dpr = std::stoi(av[++i]);
    } else if (arg == "-nhn" || arg == "--no-head-node") {
      fromCL.createHeadNode = false;
    } else if (arg == "-hn" || arg == "-chn" ||
               arg == "--head-node" || arg == "--create-head-node") {
      fromCL.createHeadNode = true;
    } else if (arg == "-h" || arg == "--help") {
      usage();
    } else {
      usage("unknown cmd-line argument '"+arg+"'");
    }    
  }

  hs::VolumeTransferFunctionSet cliVolumeXfs;
  bool haveCliVolumeXfs = false;
  if (!fromCL.xfFileName.empty()) {
    cliVolumeXfs = hs::VolumeTransferFunctionSet::load(fromCL.xfFileName);
    haveCliVolumeXfs = true;
    fromCL.scatterSettings = cliVolumeXfs.scatterSettings;
  }

  int localRank = 0, localSize = 1;
#if HS_MPI
  determineLocalProcessID(world,localRank,localSize);
#endif
  std::vector<int> gpuIDs;
  for (int i=0;i<world->size;i++) {
    world->barrier();
    if (i != world->rank) continue;
    gpuIDs = selectGPUs(world,localRank,localSize);
  }
  assert(!gpuIDs.empty());
  world->barrier();
  
  if (world->size > 1 && fromCL.dpMode == DPMODE_NOT_SPECIFIED)
    throw std::runtime_error("you're running haystack in MPI mode, and with more than one rank, but didn't specify num data groups (-ndg <n>), or whether you want to run data parallel (-dp|--data-parallel) or data replicated (-dr|--data-replicated). Just to make sure we're not actually running the wrong mode I'll hereby bail out ...");
  
  if (fromCL.ndg == 0) {
    if (fromCL.dpMode == DPMODE_DATA_PARALLEL && world->size > 1)
      // we _are_ run in mpi mode with more than one rank, and in data
      // _parallel_mode. if not otherwise specified, use one data
      // group per rank))
      fromCL.ndg = world->size;
    else
      fromCL.ndg = 1;
  }
  
  int numDataGroupsGlobally = fromCL.ndg;
  int dataPerRank   = fromCL.dpr;
  //  LocalModel thisRankData;
  LocalPartitions *localPartitions
    = loader.loadData(numDataGroupsGlobally,dataPerRank);
  if (fromCL.mergeUnstructuredMeshes) {
    std::cout << "merging potentially separate unstructured "
              << "meshes into single mesh" << std::endl;
    localPartitions->mergeUnstructuredMeshes();
    std::cout << "done mergine umeshes..." << std::endl;
  }
  
  int numPartitionsLocally = localPartitions->numPartitionsOnThisRank();
  if (numPartitionsLocally == 0)
    throw std::runtime_error("no partitions on this rank!?");
  world->barrier();

  // std::vector<int> gpuIDs;
  std::vector<DeviceConfig> deviceConfigs;
  for (int i=0;i<gpuIDs.size();i++) {
    DeviceConfig dc;
    dc.gpuID = gpuIDs[i];
    dc.localPartitionIndex = i % numPartitionsLocally;
    deviceConfigs.push_back(dc);
  }

  GlobalRenderSettings globalRenderSettings;
  globalRenderSettings.samplesPerPixel = fromCL.spp;
  globalRenderSettings.ambientRadiance = fromCL.ambientRadiance;
  globalRenderSettings.bgColor = fromCL.bgColor;
  globalRenderSettings.defaultColorMapIndex = fromCL.cmID;
  globalRenderSettings.denoise = fromCL.denoise;
  
  HayMaker *hayMaker
    = new HayMaker(// mpi/peers:
                   world,
                   globalRenderSettings,
                   // the parition(s) we have loaded locally on this rank
                   localPartitions,
                   
                   deviceConfigs);

  hayMaker->setVolumeScatterSettings(fromCL.scatterSettings);
  hayMaker->setEmissionTransferFunction(
    haveCliVolumeXfs ? cliVolumeXfs.emission : hs::TransferFunction{});

  if (!fromCL.xfFileName.empty()) {
    hayMaker->setTransferFunction(cliVolumeXfs.opacity);
  }
                   

  world->barrier();
  if (world->rank == 0)
    std::cout << MINI_TERMINAL_CYAN
              << "#hs: creating context"
              << MINI_TERMINAL_DEFAULT << std::endl;
  world->barrier();
  if (world->rank == 0)
    std::cout << MINI_TERMINAL_CYAN
              << "#hs: building data groups"
              << MINI_TERMINAL_DEFAULT << std::endl;
  if (hayMaker)
    hayMaker->renderInitialAnariWorld();
  
  world->barrier();

  RenderEngineInterface *renderer = nullptr;
  if (world->size == 1)
    // no MPI, render direcftly
    renderer = hayMaker;
  else if (world->rank == 0)
    // we're in MPI mode, _and_ the rank that runs the viewer
    renderer = new MPIRenderEngine(world,hayMaker);
  else {
    // we're in MPI mode, but one of the passive workers (ie NOT running the viewer)
    MPIRenderEngine::runWorker(world,hayMaker);
    world->barrier();
    Comm::mpiFinalize();
    
    exit(0);
  }



  // const BoundsData worldBounds = hayMaker->worldBounds;//getWorldBounds();
  const BoundsData worldBounds = renderer->getWorldBounds();
  bool modelHasVolumeData = !worldBounds.scalars.empty();
  
  if (world->rank == 0)
    std::cout << MINI_TERMINAL_CYAN
              << "#hs: world bounds is " << worldBounds
              << MINI_TERMINAL_DEFAULT << std::endl;

  if (fromCL.camera.vp == fromCL.camera.vi) {
    fromCL.camera.vp
      = worldBounds.spatial.center()
      + mini::common::vec3f(-.3f, .7f, +1.f) * worldBounds.spatial.span();
    fromCL.camera.vi = worldBounds.spatial.center();
  }

  
#if HS_SERVER
  // in 'server' mode, we just open a socket, wait for viewer to
  // connect, and then 'forward' all viewer-specific commands as if we
  // were a local viewer
  HSServer hsServer__create_waitForClient_and_run(renderer);
#elif HS_VIEWER
  Viewer viewer(worldBounds,
                fromCL.scatterSettings,
                renderer,//&world,
                fromCL.camera);

  // viewer.show();
  viewer.enableFlyMode();
  viewer.enableInspectMode();
  viewer.setWorldScale(length(worldBounds.spatial.span()));
  viewer.setCameraOrientation
    (/*origin   */(const owl::common::vec3f &)fromCL.camera.vp,
     /*lookat   */(const owl::common::vec3f &)fromCL.camera.vi,
     /*up-vector*/(const owl::common::vec3f &)fromCL.camera.vu,
/*fovy(deg)*//*ignore now ,we handle that differently*/fromCL.camera.perspective.fovy);

  if (!fromCL.xfFileName.empty())
    viewer.loadXF(fromCL.xfFileName);

  viewer.showAndRun();
#else

  auto &fbSize = fromCL.fbSize;
  std::vector<uint32_t> pixels(fbSize.x*fbSize.y);
  renderer->resize((const mini::common::vec2i&)fbSize,pixels.data());

  hs::Camera camera = fromCL.camera;;
  // camera.vp = fromCL.camera.vp;
  // camera.vu = fromCL.camera.vu;
  // camera.vi = fromCL.camera.vi;
  // camera.fovy = fromCL.camera.fovy;
  // camera.nearClip = fromCL.camera.nearClip;
  renderer->setCamera(camera);
  
  if (!fromCL.xfFileName.empty()) {
    auto xf = hs::VolumeTransferFunctionSet::load(fromCL.xfFileName);
    renderer->setTransferFunctionSet(xf);
    renderer->resetAccumulation();
  }

  if (!fromCL.cameraPath.empty()) {
    std::cout << "rendering camera path sequence" << std::endl;
    for (int frameID=0;frameID<fromCL.cameraPath.size();frameID++) {
      // hs::Camera camera;
      auto camera = fromCL.cameraPath[frameID];
      // camera.vi = c.vi;
      // camera.vp = c.vp;
      // camera.vu = c.vu;
      // camera.fovy = c.fovy;
      renderer->setCamera(camera);
      renderer->renderFrame();
      stbi_flip_vertically_on_write(true);
      char suffix[100];
      sprintf(suffix,"_frame%05i.png",frameID);
      std::string fileName = fromCL.outFileName+std::string(suffix);
      std::cout << " ... saving frame " << fileName << std::endl;
      stbi_write_png(fileName.c_str(),fbSize.x,fbSize.y,4,
                     pixels.data(),fbSize.x*sizeof(uint32_t));
    }
    renderer->terminate();
    world->barrier();
    Comm::mpiFinalize();
    exit(0);
  }
  
  if (fromCL.measure) {
    int numFramesRendered = 0;
    const int measure_warmup_frames = 2;
    const int measure_max_frames = 100;
    const float measure_max_seconds = 60.f;
    double measure_t0 = 0.;

    while (true) {
      if (numFramesRendered == measure_warmup_frames)
        measure_t0 = getCurrentTime();
      
      double t0 = getCurrentTime();
      renderer->renderFrame();
      ++numFramesRendered;
      double t1 = getCurrentTime();
      
      int numFramesMeasured = numFramesRendered - measure_warmup_frames;
      float numSecondsMeasured
        = (numFramesMeasured < 1)
        ? 0.f
        : float(t1 - measure_t0);
      if (numFramesMeasured >= measure_max_frames ||
          numSecondsMeasured >= measure_max_seconds) {
        std::cout << "measure: rendered " << numFramesMeasured << " frames in " << numSecondsMeasured << ", that is:" << std::endl;
        std::cout << "FPS " << double(numFramesMeasured/numSecondsMeasured) << std::endl;
        stbi_flip_vertically_on_write(true);
        std::cout << "saving in " << fromCL.outFileName.c_str() << std::endl;
        stbi_write_png(fromCL.outFileName.c_str(),fbSize.x,fbSize.y,4,
                       pixels.data(),fbSize.x*sizeof(uint32_t));
        
        renderer->terminate();
        world->barrier();
        Comm::mpiFinalize();
        exit(0);
      }
    }
  }
  for (int i=0;i<fromCL.numFramesAccum;i++) 
    renderer->renderFrame();

  stbi_flip_vertically_on_write(true);
  std::cout << "saving in " << fromCL.outFileName.c_str() << std::endl;
  stbi_write_png(fromCL.outFileName.c_str(),fbSize.x,fbSize.y,4,
                 pixels.data(),fbSize.x*sizeof(uint32_t));

  renderer->terminate();
#endif

  world->barrier();
  Comm::mpiFinalize();

  return 0;
}
