// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayStack/HayMaker.h"
#include "viewer/DataLoader.h"
#if HS_CUTEE
# include "cutee/OWLViewer.h"
# include "cutee/XFEditor.h"
# include "stb/stb_image_write.h"
#else
# define STB_IMAGE_WRITE_IMPLEMENTATION 1
# define STB_IMAGE_IMPLEMENTATION 1
# include "stb/stb_image.h"
# include "stb/stb_image_write.h"
#endif
#if HS_MPI
#include <unistd.h>
#endif

namespace hs {

  double t_last_render;
  
  typedef enum { DPMODE_NOT_SPECIFIED,
                 DPMODE_DATA_PARALLEL,
                 DPMODE_DATA_REPLICATED } DPMode;

  struct CmdLineCamera {
    vec3f vp   = vec3f(0.f);
    vec3f vi   = vec3f(0.f);
    vec3f vu   = vec3f(0.f,1.f,0.f);
    float fovy = 60.f;
  };
  
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
    CmdLineCamera camera;
    std::vector<CmdLineCamera> cameraPath;
    // struct {
    //   vec3f vp0, vp1;
    //   vec3f vi0, vi1;
    //   int numSteps = 0;
    // } cameraPath;
    bool measure = 0;
    std::string envMapFileName;
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

#if HS_CUTEE
  using namespace cutee; //cutee::OWLViewer
#endif
  
#if HS_CUTEE
  struct Viewer : public OWLViewer
  {
    Viewer(Renderer *const renderer,
           hs::mpi::Comm *world)
      : renderer(renderer),
        world(world)
    {}

  public slots:
    void colorMapChanged(cutee::XFEditor *xf);
    void rangeChanged(cutee::common::interval<float> r);
    void opacityScaleChanged(double scale);

  public:
    void screenShot()
    {
      std::string fileName = fromCL.outFileName;
      std::cout << "#ht: saving screen shot in " << fileName << std::endl;
      stbi_flip_vertically_on_write(true);
      stbi_write_png(fileName.c_str(),fbSize.x,fbSize.y,4,
                     fbPointer,fbSize.x*sizeof(uint32_t));
    }
    
    /*! this gets called when the user presses a key on the keyboard ... */
    void key(char key, const cutee::common::vec2i &where) override
    {
      switch(key) {
      case '!':
        screenShot();
        break;
      case '*': {
        hs::Camera camera;
        OWLViewer::getCameraOrientation
          ((cutee::common::vec3f&)camera.vp,
           (cutee::common::vec3f&)camera.vi,
           (cutee::common::vec3f&)camera.vu,
           camera.fovy);
        PRINT(normalize(camera.vi - camera.vp));
      } break;
      case 'P': {
          char *fl = getenv("BARNEY_FOCAL_LENGTH");
          std::cout << "export BARNEY_FOCAL_LENGTH=" << (fl != nullptr ? fl : "0") << std::endl;
          char *lr = getenv("BARNEY_LENS_RADIUS");
          std::cout << "export BARNEY_LENS_RADIUS=" << (lr != nullptr ? lr : "0") << std::endl;
          break;
        }
      case '9': case '0':
      case '(': case ')': {
        char* flc = getenv("BARNEY_FOCAL_LENGTH");
        float ratio = (key == '(' || key == '9') ? (0.9f) : (1.1f);
        if (key == '9' || key == '0')
          ratio = (ratio - 1.f) * 0.1f + 1.f; 
        float fl = (flc == nullptr) ? 1.f : std::stof(flc) * ratio;
#ifdef _WIN32
        _putenv_s("BARNEY_FOCAL_LENGTH", std::to_string(fl).c_str());
#else        
        setenv("BARNEY_FOCAL_LENGTH", std::to_string(fl).c_str(), 1);
#endif        
        std::cout << "Focal length is set to: " << fl << '\n';
        renderer->resetAccumulation();
        break;
      }
      case '{': case '}':
      case '[': case ']': {
        char* lrc = getenv("BARNEY_LENS_RADIUS");
        float ratio = (key == '[' || key == '{') ? (0.9f) : (1.1f);
        if (key == '[' || key == ']')
          ratio = (ratio - 1.f) * 0.1f + 1.f; 
        float lr = (lrc == nullptr) ? 1.f : std::stof(lrc) * ratio;
#ifdef _WIN32
        _putenv_s("BARNEY_LENS_RADIUS", std::to_string(lr).c_str());
#else        
        setenv("BARNEY_LENS_RADIUS", std::to_string(lr).c_str(), 1);
#endif        
        std::cout << "Lens radius is set to: " << lr << '\n';
        renderer->resetAccumulation();
        break;
      }

      case 'T':
        std::cout << "(T) : dumping transfer function" << std::endl;
#if HS_CUTEE
        if (xfEditor)
          xfEditor->saveTo("hayThere.xf");
#endif
        break;

      default: OWLViewer::key(key,where);
      };
    }

    /*! window notifies us that we got resized. We HAVE to override
      this to know our actual render dimensions, and get pointer
      to the device frame buffer that the viewer cated for us */
    void resize(const cutee::common::vec2i &newSize) override
    {
      OWLViewer::resize(newSize);
      renderer->resize((const mini::common::vec2i&)newSize,fbPointer);
    }

    /*! gets called whenever the viewer needs us to re-render out widget */
    void render() override
    {
      double _t0 = mini::common::getCurrentTime();

      if (xfDirty) {
        renderer->setTransferFunction(xf);
        xfDirty = false;
        accumDirty = true;
      }

      if (accumDirty) {
        renderer->resetAccumulation();
        accumDirty = false;
      }

      static int numFramesRendered = 0;
      const int measure_warmup_frames = 2;
      const int measure_max_frames = 100;
      const float measure_max_seconds = 60.f;
      
      static double measure_t0 = 0.;
      if (numFramesRendered == measure_warmup_frames)
        measure_t0 = mini::common::getCurrentTime();

      static double t0 = mini::common::getCurrentTime();
      renderer->renderFrame();
      ++numFramesRendered;
      double t1 = mini::common::getCurrentTime();

      if (fromCL.measure) {
        int numFramesMeasured = numFramesRendered - measure_warmup_frames;
        float numSecondsMeasured
          = (numFramesMeasured < 1)
          ? 0.f
          : float(t1 - measure_t0);

        if (numFramesMeasured >= measure_max_frames ||
            numSecondsMeasured >= measure_max_seconds) {
          std::cout << "measure: rendered " << numFramesMeasured << " frames in " << numSecondsMeasured << ", that is:" << std::endl;
          std::cout << "FPS " << double(numFramesMeasured/numSecondsMeasured) << std::endl;
          screenShot();
          renderer->terminate();
          
          world->barrier();
          hs::mpi::finalize();
          exit(0);
        }
      }

      static double sum_t = 0.f;
      static double sum_w = 0.f;
      sum_t = 0.8f*sum_t + (t1-t0);
      sum_w = 0.8f*sum_w + 1.f;
      float timePerFrame = float(sum_t / sum_w);
      float fps = 1.f/timePerFrame;
      std::string title = "HayThere ("+mini::common::prettyDouble(fps)+"fps)";
      setTitle(title.c_str());
      t0 = t1;
      double _t1 = mini::common::getCurrentTime();
      t_last_render = _t1-_t0;
    }
    
    void cameraChanged() override
    {  
      hs::Camera camera;
      OWLViewer::getCameraOrientation
        ((cutee::common::vec3f&)camera.vp,
         (cutee::common::vec3f&)camera.vi,
         (cutee::common::vec3f&)camera.vu,
         camera.fovy);
      renderer->setCamera(camera);
      accumDirty = true;
    }

    TransferFunction xf;
    bool xfDirty = true;
    bool accumDirty = true;
    Renderer *const renderer;
    hs::mpi::Comm *world;
    XFEditor *xfEditor = 0;
  };
#endif

#if HS_CUTEE
  void Viewer::colorMapChanged(cutee::XFEditor *xfEditor)
  {
    xf.colorMap = (const std::vector<mini::common::vec4f>&)xfEditor->getColorMap();
    xfDirty = true;
  }
  
  void Viewer::rangeChanged(cutee::common::interval<float> r)
  {
    xf.domain.lower = r.lower;
    xf.domain.upper = r.upper;
    xfDirty = true;
  }
  
  void Viewer::opacityScaleChanged(double scale)
  {
    xf.baseDensity = powf(1.03,scale - 100.f);
    xfDirty = true;
  }
#endif

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
  void determineLocalProcessID(mpi::Comm &world, int &localRank, int &localSize)
  {
# if HS_FAKE_MPI
    localRank = 0;
    localSize = 1;
# else
    world.barrier();
    std::vector<char> hostName(10000);
    gethostname(hostName.data(),hostName.size());
    size_t hash = computeHashFromString(hostName.data());
    std::vector<size_t> allHostNames(world.size);
    MPI_Allgather(&hash,sizeof(hash),MPI_BYTE,
                  allHostNames.data(),sizeof(hash),MPI_BYTE,
                  world.comm);
    localRank = 0;
    localSize = 0;
    for (int i=0;i<world.size;i++) {
      if (allHostNames[i] != hash) continue;
      localSize++;
      if (i < world.rank) localRank++;
    }

    for (int i=0;i<world.size;i++) {
      world.barrier();
      if (i != world.rank) continue;
      std::cout << "#hs(" << world.rank << "): determined local rank/size as "
                << localRank << "/" << localSize << std::endl;
    }
#endif
  }
#endif

  int getIntFromEnv(const char *varName, int fallback)
  {
    const char *var = getenv(varName);
    if (!var) return fallback;
    return std::stoi(var);
  }

  std::vector<int> selectGPUs(mpi::Comm &world, int localRank, int localSize)
  {
#ifdef __APPLE__
    return { -1 };
#endif
    std::cout << "#hs(" << world.rank << "): selecting GPUs ... " << std::endl;
    const char *hcd = getenv("HS_CUDA_DEVICES");
    if (hcd) {
      std::cout << "#hs(" << world.rank << "): found HS_CUDA_DEVICES, using this" << std::endl;
      return parseCommaSeparatedListOfInts(hcd);
    }
    const char *cvd = getenv("CUDA_VISIBLE_DEVICES");
    if (hcd) {
      std::cout << "#hs(" << world.rank << "): found CUDA_VISIBLE_DEVICES being set in env, using GPUs 0,1,... etc" << std::endl;
      auto cudaGPUs = parseCommaSeparatedListOfInts(hcd);
      std::vector<int> result;
      for (int i=0;i<cudaGPUs.size();i++)
        result.push_back(i);
      return result;
    }

    return { 0 };
  }
    
  inline float lerp_l(float f, float a, float b)
  { return (1.f-f)*a + f*b; }
  inline mini::common::vec3f lerp_l(float f,
                                    mini::common::vec3f a,
                                    mini::common::vec3f b)
  { return (1.f-f)*a + f*b; }
  
  void addCameraPath(int numSteps,
                     CmdLineCamera c0,
                     CmdLineCamera c1)
  {
    for (int i=0;i<numSteps;i++) {
      float f = i/(numSteps-1.f);
      CmdLineCamera c;
      c.vp = lerp_l(f,c0.vp,c1.vp);
      c.vi = lerp_l(f,c0.vi,c1.vi);
      c.vu = lerp_l(f,c0.vu,c1.vu);
      c.fovy = lerp_l(f,c0.fovy,c1.fovy);
      fromCL.cameraPath.push_back(c);
    }
  }

  void addCamerasFromFile(const std::string &fileName)
  {
    std::ifstream in(fileName.c_str());
    std::string line;
    while (in.good()) {
      std::getline(in,line);
      CmdLineCamera c;
      int rc = sscanf(line.c_str(),"--camera %f %f %f %f %f %f %f %f %f --fovy %f",
                      &c.vp.x,
                      &c.vp.y,
                      &c.vp.z,
                      &c.vi.x,
                      &c.vi.y,
                      &c.vi.z,
                      &c.vu.x,
                      &c.vu.y,
                      &c.vu.z,
                      &c.fovy);
      if (rc != 10)
        break;
      fromCL.cameraPath.push_back(c);
    }
  }
  
}

using namespace hs;

int main(int ac, char **av)
{
  // /*! init ALL gpus - let's do that right away, so gpus are
  //     initailized before mpi even gets to run */
  // hs::initAllGPUs();

  hs::mpi::init(ac, av);
#if HS_FAKE_MPI
  hs::mpi::Comm world;
#else
  hs::mpi::Comm world(MPI_COMM_WORLD);
#endif

  world.barrier();
  if (world.rank == 0) {
    std::cout << "#hv: hsviewer starting up" << std::endl; fflush(0);
  }
  world.barrier();

  bool hanari = true;
  DynamicDataLoader loader(world);
  for (int i=1;i<ac;i++) {
    const std::string arg = av[i];
    if (arg[0] != '-') {
      loader.addContent(arg);
    } else if (arg == "--no-bg") {
      fromCL.bgColor = ::mini::common::vec4f(0.f);
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
    } else if (arg == "--camera-pdu") {
      fromCL.camera.vp = get3f(av,i);
      fromCL.camera.vi = fromCL.camera.vp + get3f(av,i);
      fromCL.camera.vu = get3f(av,i);
    } else if (arg == "--camera") {
      fromCL.camera.vp = get3f(av,i);
      fromCL.camera.vi = get3f(av,i);
      fromCL.camera.vu = get3f(av,i);
    } else if (arg == "--cameras-from-file") {
      hs::addCamerasFromFile(av[++i]);
    } else if (arg == "--camera-path") {
      CmdLineCamera c0, c1;
      int numSteps = std::stoi(av[++i]);
      // --camera
      ++i;
      c0.vp = get3f(av,i);
      c0.vp = get3f(av,i);
      c0.vu = get3f(av,i);
      ++i;// -fovy
      c0.fovy = std::stof(av[++i]);
      ++i;// --camera
      c1.vp = get3f(av,i);
      c1.vp = get3f(av,i);
      c1.vu = get3f(av,i);
      ++i;// -fovy
      c1.fovy = std::stof(av[++i]);
      hs::addCameraPath(numSteps,c0,c1);
    } else if (arg == "-fovy") {
      fromCL.camera.fovy = std::stof(av[++i]);
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
    } else if (arg == "-anari" || arg == "--hanari") {
      hanari = true;
    } else if (arg == "-native" || arg == "--native") {
      hanari = false;
    } else {
      usage("unknown cmd-line argument '"+arg+"'");
    }    
  }

  int localRank = 0, localSize = 1;
#if HS_MPI
  determineLocalProcessID(world,localRank,localSize);
#endif
  std::vector<int> gpuIDs;
  for (int i=0;i<world.size;i++) {
    world.barrier();
    if (i != world.rank) continue;
    gpuIDs = selectGPUs(world,localRank,localSize);
  }
  assert(!gpuIDs.empty());
  world.barrier();
  
  const bool isHeadNode = fromCL.createHeadNode && (world.rank == 0);
  hs::mpi::Comm workers = world.split(!isHeadNode);

  if (world.size > 1 && fromCL.dpMode == DPMODE_NOT_SPECIFIED)
    throw std::runtime_error("you're running haystack in MPI mode, and with more than one rank, but didn't specify num data groups (-ndg <n>), or whether you want to run data parallel (-dp|--data-parallel) or data replicated (-dr|--data-replicated). Just to make sure we're not actually running the wrong mode I'll hereby bail out ...");
  
  if (fromCL.ndg == 0) {
    if (fromCL.dpMode == DPMODE_DATA_PARALLEL && world.size > 1)
      // we _are_ run in mpi mode with more than one rank, and in data
      // _parallel_mode. if not otherwise specified, use one data
      // group per rank
      fromCL.ndg = world.size;
    else
      fromCL.ndg = 1;
  }
  
  int numDataGroupsGlobally = fromCL.ndg;
  int dataPerRank   = fromCL.dpr;
  LocalModel thisRankData;
  thisRankData.colorMapIndex = fromCL.cmID;
  if (!isHeadNode) {
    loader.loadData(thisRankData,numDataGroupsGlobally,dataPerRank,verbose());
  }
  if (fromCL.mergeUnstructuredMeshes) {
    std::cout << "merging potentially separate unstructured meshes into single mesh" << std::endl;
    thisRankData.mergeUnstructuredMeshes();
    std::cout << "done mergine umeshes..." << std::endl;
  }

  int numDataGroupsLocally = thisRankData.size();
  world.barrier();
  HayMaker *hayMaker
    = HayMaker::createAnariImplementation(world,
                                          /* the workers */workers,
                                          fromCL.spp,
                                           fromCL.ambientRadiance,
                                          fromCL.bgColor,
                                          thisRankData,
                                          gpuIDs,verbose());
  
  world.barrier();
  const BoundsData worldBounds = hayMaker->getWorldBounds();
  bool modelHasVolumeData = !worldBounds.scalars.empty();
  
  if (world.rank == 0)
    std::cout << MINI_TERMINAL_CYAN
              << "#hs: world bounds is " << worldBounds
              << MINI_TERMINAL_DEFAULT << std::endl;

  if (fromCL.camera.vp == fromCL.camera.vi) {
    fromCL.camera.vp
      = worldBounds.spatial.center()
      + mini::common::vec3f(-.3f, .7f, +1.f) * worldBounds.spatial.span();
    fromCL.camera.vi = worldBounds.spatial.center();
  }

  world.barrier();
  if (world.rank == 0)
    std::cout << MINI_TERMINAL_CYAN
              << "#hs: creating context"
              << MINI_TERMINAL_DEFAULT << std::endl;
  world.barrier();
  if (world.rank == 0)
    std::cout << MINI_TERMINAL_CYAN
              << "#hs: building data groups"
              << MINI_TERMINAL_DEFAULT << std::endl;
  if (!isHeadNode)
    hayMaker->buildSlots();
  
  world.barrier();

  Renderer *renderer = nullptr;
  if (world.size == 1)
    // no MPI, render direcftly
    renderer = hayMaker;
  else if (world.rank == 0)
    // we're in MPI mode, _and_ the rank that runs the viewer
    renderer = new MPIRenderer(world,hayMaker);
  else {
    // we're in MPI mode, but one of the passive workers (ie NOT running the viewer)
    MPIRenderer::runWorker(world,hayMaker);
    world.barrier();
    hs::mpi::finalize();

    exit(0);
  }

#if HS_CUTEE
  QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QApplication app(ac,av);
  Viewer viewer(renderer,&world);

  viewer.show();
  viewer.enableFlyMode();
  viewer.enableInspectMode();
  viewer.setWorldScale(length(worldBounds.spatial.span()));
  viewer.setCameraOrientation
    (/*origin   */(const cutee::common::vec3f &)fromCL.camera.vp,
     /*lookat   */(const cutee::common::vec3f &)fromCL.camera.vi,
     /*up-vector*/(const cutee::common::vec3f &)fromCL.camera.vu,
     /*fovy(deg)*/fromCL.camera.fovy);

  QMainWindow secondWindow;
  if (modelHasVolumeData) {
    XFEditor    *xfEditor = new XFEditor
      ((cutee::common::interval<float>&)worldBounds.scalars);
    viewer.xfEditor       = xfEditor;
    QFormLayout *layout   = new QFormLayout;
    layout->addWidget(xfEditor);
    
    QObject::connect(xfEditor,&cutee::XFEditor::colorMapChanged,
                   &viewer, &Viewer::colorMapChanged);
    QObject::connect(xfEditor,&cutee::XFEditor::rangeChanged,
                     &viewer, &Viewer::rangeChanged);
    QObject::connect(xfEditor,&cutee::XFEditor::opacityScaleChanged,
                     &viewer, &Viewer::opacityScaleChanged);
    
    
    if (!fromCL.xfFileName.empty())
      viewer.xfEditor->loadFrom(fromCL.xfFileName);
    
    // Set QWidget as the central layout of the main window
    secondWindow.setCentralWidget(viewer.xfEditor);
    
    secondWindow.show();
  }
  
  app.exec();
#else

  auto &fbSize = fromCL.fbSize;
  std::vector<uint32_t> pixels(fbSize.x*fbSize.y);
  renderer->resize((const mini::common::vec2i&)fbSize,pixels.data());

  hs::Camera camera;
  camera.vp = fromCL.camera.vp;
  camera.vu = fromCL.camera.vu;
  camera.vi = fromCL.camera.vi;
  camera.fovy = fromCL.camera.fovy;
  renderer->setCamera(camera);
  
  if (fromCL.xfFileName.length() > 0) {
    hs::TransferFunction xf;
    xf.load(fromCL.xfFileName);
    renderer->setTransferFunction(xf);
    renderer->resetAccumulation();
  }

  if (!fromCL.cameraPath.empty()) {
    std::cout << "rendering camera path sequence" << std::endl;
    for (int frameID=0;frameID<fromCL.cameraPath.size();frameID++) {
      hs::Camera camera;
      auto c = fromCL.cameraPath[frameID];
      camera.vi = c.vi;
      camera.vp = c.vp;
      camera.vu = c.vu;
      camera.fovy = c.fovy;
      // float f = frameID / (fromCL.cameraPath.numSteps-1.f);
      // camera.vu = fromCL.camera.vu;
      // camera.vp
      //   = (1.f-f)*fromCL.cameraPath.vp0 + f*fromCL.cameraPath.vp1;
      // camera.vi
      //   = (1.f-f)*fromCL.cameraPath.vi0 + f*fromCL.cameraPath.vi1;
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
    world.barrier();
    hs::mpi::finalize();
    exit(0);
  }
  
#if 1
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
        world.barrier();
        hs::mpi::finalize();
        exit(0);
      }
    }
  }
#endif
  for (int i=0;i<fromCL.numFramesAccum;i++) 
    renderer->renderFrame();

  stbi_flip_vertically_on_write(true);
  stbi_write_png(fromCL.outFileName.c_str(),fbSize.x,fbSize.y,4,
                 pixels.data(),fbSize.x*sizeof(uint32_t));

  renderer->terminate();
#endif

  world.barrier();
  hs::mpi::finalize();

  return 0;
}
