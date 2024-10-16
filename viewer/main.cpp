// ======================================================================== //
// Copyright 2022-2023 Ingo Wald                                            //
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

#include "hayStack/HayMaker.h"
#include "viewer/DataLoader.h"
#if HS_VIEWER
#ifdef _WIN32 
#  include "glad.h"
#endif
# include "samples/common/owlViewer/InspectMode.h"
# include "samples/common/owlViewer/OWLViewer.h"
#define STB_IMAGE_IMPLEMENTATION 1
# include "stb/stb_image.h"
# include "stb/stb_image_write.h"
# if HS_HAVE_IMGUI
#  include "imgui_tfe/imgui_impl_glfw_gl3.h"
#  include "imgui_tfe/TFEditor.h"
#  include <imgui.h>
# endif
#elif HS_CUTEE
# include "qtOWL/OWLViewer.h"
# include "qtOWL/XFEditor.h"
# include "stb/stb_image_write.h"
#else
# define STB_IMAGE_WRITE_IMPLEMENTATION 1
# define STB_IMAGE_IMPLEMENTATION 1
# include "stb/stb_image.h"
# include "stb/stb_image_write.h"
#endif
#include <cuda_runtime.h>

namespace hs {

  double t_last_render;
  
  struct FromCL {
    /*! data groups per rank. '0' means 'auto - use few as we can, as
        many as we have to fit for given number of ranks */
    int dpr = 0;
    /*! num data groups */
    int ndg = 1;

    bool mergeUnstructuredMeshes = false;
    
    std::string xfFileName = "";
    std::string outFileName = "hayStack.png";
    vec2i fbSize = { 800,600 };
    bool createHeadNode = false;
    int  numExtraDisplayRanks = 0;
    int  numFramesAccum = 1;
    int  spp            = 1;
    bool verbose = true;
    struct {
      vec3f vp   = vec3f(0.f);
      vec3f vi   = vec3f(0.f);
      vec3f vu   = vec3f(0.f,1.f,0.f);
      float fovy = 60.f;
    } camera;
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

#if HS_VIEWER
  using namespace owl::viewer; //owl::viewer::OWLViewer;
#endif
#if HS_CUTEE
  using namespace qtOWL; //qtOWL::OWLViewer
#endif
  
#if HS_VIEWER || HS_CUTEE
  struct Viewer : public OWLViewer
  {
    Viewer(Renderer *const renderer)
      : renderer(renderer)
    {
#if HS_VIEWER
# if HS_HAVE_IMGUI
      ImGui_ImplGlfwGL3_Init(handle, true);
# endif
#endif
    }

#if HS_CUTEE
  public slots:
    void colorMapChanged(qtOWL::XFEditor *xf);
    void rangeChanged(range1f r);
    void opacityScaleChanged(double scale);
#endif

#if HS_VIEWER
    void updateXFImGui();
#endif
  public:
    void screenShot()
    {
      std::string fileName = fromCL.outFileName;
      std::vector<int> hostFB(fbSize.x*fbSize.y);
      cudaMemcpy(hostFB.data(),fbPointer,
                 fbSize.x*fbSize.y*sizeof(int),
                 cudaMemcpyDefault);
      cudaDeviceSynchronize();
      std::cout << "#ht: saving screen shot in " << fileName << std::endl;
      stbi_flip_vertically_on_write(true);
      stbi_write_png(fileName.c_str(),fbSize.x,fbSize.y,4,
                     hostFB.data(),fbSize.x*sizeof(uint32_t));
    }
    
    /*! this gets called when the user presses a key on the keyboard ... */
    void key(char key, const vec2i &where) override
    {
      switch(key) {
      case '!':
        screenShot();
        break;
      case 'T':
        std::cout << "(T) : dumping transfer function" << std::endl;
#if HS_CUTEE
        if (xfEditor)
          xfEditor->saveTo("hayThere.xf");
#elif HS_VIEWER
# if HS_HAVE_IMGUI
        if (xfEditor)
          xfEditor->saveToFile("hayThere.xf");
# endif
#else
        std::cout << "dumping transfer function only works in QT viewer" << std::endl;
#endif
        break;
      default: OWLViewer::key(key,where);
      };
    }

#if HS_VIEWER
    void mouseMotion(const vec2i &newMousePosition) override
    {
# if HS_HAVE_IMGUI
      ImGuiIO &io = ImGui::GetIO();
      if (!io.WantCaptureMouse) 
# endif
      {
          OWLViewer::mouseMotion(newMousePosition);
      }
    }
#endif
    
    /*! window notifies us that we got resized. We HAVE to override
      this to know our actual render dimensions, and get pointer
      to the device frame buffer that the viewer cated for us */
    void resize(const vec2i &newSize) override
    {
      OWLViewer::resize(newSize);
      renderer->resize(newSize,fbPointer);
    }

    /*! gets called whenever the viewer needs us to re-render out widget */
    void render() override
    {
      double _t0 = getCurrentTime();
#if HS_VIEWER
# if HS_HAVE_IMGUI
      ImGui_ImplGlfwGL3_NewFrame();
# endif
#endif

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
        measure_t0 = getCurrentTime();

      static double t0 = getCurrentTime();
      renderer->renderFrame(fromCL.spp);
      ++numFramesRendered;
      double t1 = getCurrentTime();

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
          exit(0);
        }
      }

      static double sum_t = 0.f;
      static double sum_w = 0.f;
      sum_t = 0.8f*sum_t + (t1-t0);
      sum_w = 0.8f*sum_w + 1.f;
      float timePerFrame = float(sum_t / sum_w);
      float fps = 1.f/timePerFrame;
      std::string title = "HayThere ("+prettyDouble(fps)+"fps)";
      setTitle(title.c_str());
      t0 = t1;
      double _t1 = getCurrentTime();
      t_last_render = _t1-_t0;
    }
    
#if HS_VIEWER
    void draw() override
    {
      OWLViewer::draw();

#if HS_HAVE_IMGUI
      if (xfEditor) {
        ImGui::Begin("TFE");
    
        xfEditor->drawImmediate();

        ImGui::End();

        ImGui::Render();
        ImGui_ImplGlfwGL3_Render();

        updateXFImGui();
      }
#endif
    }
#endif

    void cameraChanged()
    {  
      hs::Camera camera;
      OWLViewer::getCameraOrientation(camera.vp,camera.vi,camera.vu,camera.fovy);
      renderer->setCamera(camera);
      accumDirty = true;
    }

    TransferFunction xf;
    bool xfDirty = true;
    bool accumDirty = true;
    Renderer *const renderer;
#if HS_CUTEE
    XFEditor *xfEditor = 0;
#elif HS_VIEWER
# if HS_HAVE_IMGUI
    TFEditor *xfEditor = 0;
# endif
#endif
  };


#if HS_CUTEE
  void Viewer::colorMapChanged(qtOWL::XFEditor *xfEditor)
  {
    xf.colorMap = xfEditor->getColorMap();
    xfDirty = true;
  }
  
  void Viewer::rangeChanged(range1f r)
  {
    xf.domain = r;
    xfDirty = true;
  }
  
  void Viewer::opacityScaleChanged(double scale)
  {
    xf.baseDensity = powf(1.1f,scale - 100.f);
    xfDirty = true;
  }
  
#elif HS_VIEWER
# if HS_HAVE_IMGUI
  void Viewer::updateXFImGui()
  {
    if (xfEditor->cmapUpdated()) {
      xf.colorMap = xfEditor->getColorMap();
      xfDirty = true;
    }

    if (xfEditor->rangeUpdated()) {
      xf.domain = xfEditor->getRange();
      xfDirty = true;
    }

    if (xfEditor->opacityUpdated()) {
      float scale = xfEditor->getOpacityScale();
      xf.baseDensity = powf(1.1f,scale - 100.f);
      xfDirty = true;
    }

    xfEditor->downdate(); // TODO: is this necessary?
  }
# endif
#endif

#endif

  inline vec3f get3f(char **av, int &i)
  {
    float x = std::stof(av[++i]);
    float y = std::stof(av[++i]);
    float z = std::stof(av[++i]);
    return vec3f(x,y,z);
  }
}

using namespace hs;

int main(int ac, char **av)
{
  hs::mpi::init(ac, av);
#if HS_FAKE_MPI
  hs::mpi::Comm world;
#else
  hs::mpi::Comm world(MPI_COMM_WORLD);
#endif

  world.barrier();
  if (world.rank == 0)
    std::cout << "#hv: hsviewer starting up" << std::endl; fflush(0);
  world.barrier();

  // FromCL fromCL;
  // BarnConfig config;
  bool hanari = false;
  DynamicDataLoader loader(world);
  for (int i=1;i<ac;i++) {
    const std::string arg = av[i];
    if (arg[0] != '-') {
      loader.addContent(arg);
    } else if (arg == "-env" || arg == "--env-map") {
      fromCL.envMapFileName = av[++i];
    } else if (arg == "--num-frames") {
      fromCL.numFramesAccum = std::stoi(av[++i]);
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
    } else if (arg == "-fovy") {
      fromCL.camera.fovy = std::stof(av[++i]);
    } else if (arg == "-xf") {
      fromCL.xfFileName = av[++i];
    } else if (arg == "-res" || arg == "-os" || arg == "--output-size") {
      fromCL.fbSize.x = std::stoi(av[++i]);
      fromCL.fbSize.y = std::stoi(av[++i]);
    } else if (arg == "-ndg") {
      fromCL.ndg = std::stoi(av[++i]);
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
    } else {
      usage("unknown cmd-line argument '"+arg+"'");
    }    
  }

  const bool isHeadNode = fromCL.createHeadNode && (world.rank == 0);
  hs::mpi::Comm workers = world.split(!isHeadNode);

  
  int numDataGroupsGlobally = fromCL.ndg;
  int dataPerRank   = fromCL.dpr;
  LocalModel thisRankData;
  if (!isHeadNode) {
    loader.loadData(thisRankData,numDataGroupsGlobally,dataPerRank,verbose());
  }
  if (fromCL.mergeUnstructuredMeshes) {
    std::cout << "merging potentially separate unstructured meshes into single mesh" << std::endl;
    thisRankData.mergeUnstructuredMeshes();
    std::cout << "done mergine umeshes..." << std::endl;
  }

  int numDataGroupsLocally = thisRankData.size();
  HayMaker *hayMaker
    = hanari
    ? HayMaker::createAnariImplementation(world,
                                          /* the workers */workers,
                                          thisRankData,
                                          verbose())
    : HayMaker::createBarneyImplementation(world,
                                           /* the workers */workers,
                                           thisRankData,
                                           verbose());
// #if HANARI
//     hayMaker = new HayMakerT<AnariBackend>(world,
//                                            /* the workers */workers,
//                                            thisRankData,
//                                            verbose());
// #else
// #endif
//   } else 
//     hayMaker = new HayMakerT<BarneyBackend>(world,
//                                             /* the workers */workers,
//                                             thisRankData,
//                                             verbose());
  
  world.barrier();
  const BoundsData worldBounds = hayMaker->getWorldBounds();
  if (world.rank == 0)
    std::cout << OWL_TERMINAL_CYAN
              << "#hs: world bounds is " << worldBounds
              << OWL_TERMINAL_DEFAULT << std::endl;

  if (fromCL.camera.vp == fromCL.camera.vi) {
    fromCL.camera.vp
      = worldBounds.spatial.center()
      + vec3f(-.3f, .7f, +1.f) * worldBounds.spatial.span();
    fromCL.camera.vi = worldBounds.spatial.center();
  }
  
  world.barrier();
  if (world.rank == 0)
    std::cout << OWL_TERMINAL_CYAN
              << "#hs: creating barney context"
              << OWL_TERMINAL_DEFAULT << std::endl;
  // hayMaker->createBarney();
  world.barrier();
  if (world.rank == 0)
    std::cout << OWL_TERMINAL_CYAN
              << "#hs: building barney data groups"
              << OWL_TERMINAL_DEFAULT << std::endl;
  if (!isHeadNode)
    hayMaker->buildSlots();
    // for (int dgID=0;dgID<numDataGroupsLocally;dgID++)
    //   hayMaker->buildDataGroup(dgID);
  
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

#if HS_VIEWER
  Viewer viewer(renderer);
  
   viewer.enableFlyMode();
  viewer.enableInspectMode(/*owl::glutViewer::OWLViewer::Arcball,*/worldBounds.spatial);
  viewer.setWorldScale(owl::length(worldBounds.spatial.span()));
  viewer.setCameraOrientation(/*origin   */fromCL.camera.vp,
                              /*lookat   */fromCL.camera.vi,
                              /*up-vector*/fromCL.camera.vu,
                              /*fovy(deg)*/fromCL.camera.fovy);
# if HS_HAVE_IMGUI
  TFEditor    *xfEditor = new TFEditor;
  xfEditor->setRange(worldBounds.scalars);
  viewer.xfEditor       = xfEditor;

  if (!fromCL.xfFileName.empty())
    xfEditor->loadFromFile(fromCL.xfFileName.c_str());
# endif
  viewer.showAndRun();
#elif HS_CUTEE
  QApplication app(ac,av);
  Viewer viewer(renderer);

  viewer.show();
  viewer.enableFlyMode();
  viewer.enableInspectMode();///*owl::glutViewer::OWLViewer::Arcball,*/worldBounds.spatial);
  viewer.setWorldScale(owl::length(worldBounds.spatial.span()));
  viewer.setCameraOrientation(/*origin   */fromCL.camera.vp,
                              /*lookat   */fromCL.camera.vi,
                              /*up-vector*/fromCL.camera.vu,
                              /*fovy(deg)*/fromCL.camera.fovy);

  XFEditor    *xfEditor = new XFEditor(worldBounds.scalars);
  viewer.xfEditor      = xfEditor;
  QFormLayout *layout   = new QFormLayout;
  layout->addWidget(xfEditor);

  QObject::connect(xfEditor,&qtOWL::XFEditor::colorMapChanged,
                   &viewer, &Viewer::colorMapChanged);
  QObject::connect(xfEditor,&qtOWL::XFEditor::rangeChanged,
                   &viewer, &Viewer::rangeChanged);
  QObject::connect(xfEditor,&qtOWL::XFEditor::opacityScaleChanged,
                   &viewer, &Viewer::opacityScaleChanged);
  // QObject::connect(&viewer.lightInteractor,&LightInteractor::lightPosChanged,
  //                  &viewer, &Viewer::lightPosChanged);
  

  if (!fromCL.xfFileName.empty())
    xfEditor->loadFrom(fromCL.xfFileName);
  
  // Set QWidget as the central layout of the main window
  QMainWindow secondWindow;
  secondWindow.setCentralWidget(xfEditor);

  secondWindow.show();
  
  app.exec();
#else

  auto &fbSize = fromCL.fbSize;
  std::vector<uint32_t> pixels(fbSize.x*fbSize.y);
  renderer->resize(fbSize,pixels.data());

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

  for (int i=0;i<fromCL.numFramesAccum;i++) 
    renderer->renderFrame(fromCL.spp);

  stbi_flip_vertically_on_write(true);
  stbi_write_png(fromCL.outFileName.c_str(),fbSize.x,fbSize.y,4,
                 pixels.data(),fbSize.x*sizeof(uint32_t));

  renderer->terminate();
#endif

  world.barrier();

  hs::mpi::finalize();

  return 0;
}
