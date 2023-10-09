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

#include "viewer/HayMaker.h"

#if HS_VIEWER
# include "samples/common/owlViewer/InspectMode.h"
# include "samples/common/owlViewer/OWLViewer.h"
#else
# define STB_IMAGE_WRITE_IMPLEMENTATION 1
# include "stb/stb_image_write.h"
#endif

namespace hs {

  struct FromCL {
    /*! data groups per rank. '0' means 'auto - use few as we can, as
        many as we have to fit for given number of ranks */
    int dpr = 0;
    /*! num data groups */
    int ndg = 1;

    std::string outFileName = "hay.png";
    vec2i fbSize = { 800,600 };
    bool createHeadNode = false;
    int  numExtraDisplayRanks = 0;
    static bool verbose;
    struct {
      vec3f vp   = vec3f(0.f);
      vec3f vi   = vec3f(0.f);
      vec3f vu   = vec3f(0.f,1.f,0.f);
      float fovy = 60.f;
    } camera;
  };
  
  bool FromCL::verbose = true;

  inline bool verbose() { return FromCL::verbose; }
  
  void usage(const std::string &error="")
  {
    std::cout << "./haystack ... <args>" << std::endl;
    if (!error.empty())
      throw std::runtime_error("fatal error: " +error);
    exit(0);
  }

  
#if HS_VIEWER
  struct Viewer : public owl::viewer::OWLViewer
  {
    Viewer(Renderer *const renderer)
      : renderer(renderer)
    {}
    
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
      renderer->renderFrame();
    }
    
    Renderer *const renderer;
  };
#endif  
}

using namespace hs;

int main(int ac, char **av)
{
  barney::mpi::init(ac,av);
  barney::mpi::Comm world(MPI_COMM_WORLD);

  world.barrier();
  if (world.rank == 0)
    std::cout << "#hv: hsviewer starting up" << std::endl; fflush(0);
  world.barrier();

  FromCL fromCL;
  BarnConfig config;

  DynamicDataLoader loader;
  for (int i=1;i<ac;i++) {
    const std::string arg = av[i];
    if (arg[0] != '-') {
      loader.addContent(arg);
    } else if (arg == "--default-radius") {
      loader.defaultRadius = std::stoi(av[++i]);
    } else if (arg == "-o") {
      fromCL.outFileName = av[++i];
    } else if (arg == "-res") {
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
    } else {
      usage("unknown cmd-line argument '"+arg+"'");
    }    
  }

  const bool isHeadNode = fromCL.createHeadNode && (world.rank == 0);
  HayMaker hayMaker(/* the ring that binds them all : */world,
                    /* whether this is a active worker */!isHeadNode,
                    verbose());
  int numDataGroups = fromCL.ndg;
  int dataPerRank = fromCL.dpr;
  if (!isHeadNode)
    hayMaker.loadData(loader,numDataGroups,dataPerRank);

  world.barrier();
  const box3f worldBounds = hayMaker.getWorldBounds();
  if (world.rank == 0)
    std::cout << "#hs: world bounds is " << worldBounds << std::endl;

  if (fromCL.camera.vp == fromCL.camera.vi) {
    fromCL.camera.vp
      = worldBounds.center()
      + vec3f(-.3f, .7f, +1.f) * worldBounds.span();
    fromCL.camera.vi = worldBounds.center();
  }
  
  world.barrier();
  hayMaker.createBarney();

  world.barrier();
  
  Renderer *renderer = nullptr;
  if (world.size == 1)
    // no MPI, render direcftly
    renderer = &hayMaker;
  else if (world.rank == 0)
    // we're in MPI mode, _and_ the rank that runs the viewer
    renderer = new MPIRenderer(world,&hayMaker);
  else {
    // we're in MPI mode, but one of the passive workers (ie NOT running the viewer)
    MPIRenderer::runWorker(world,&hayMaker);
    world.barrier();
    barney::mpi::finalize();
    exit(0);
  }

#if HS_VIEWER
  Viewer viewer(renderer);
  
  viewer.enableFlyMode();
  viewer.enableInspectMode(/*owl::glutViewer::OWLViewer::Arcball,*/worldBounds);
  viewer.setWorldScale(owl::length(worldBounds.span()));
  viewer.setCameraOrientation(/*origin   */fromCL.camera.vp,
                              /*lookat   */fromCL.camera.vi,
                              /*up-vector*/fromCL.camera.vu,
                              /*fovy(deg)*/fromCL.camera.fovy);
  viewer.showAndRun();
#else

  auto &fbSize = fromCL.fbSize;
  std::vector<uint32_t> pixels(fbSize.x*fbSize.y);
  renderer->resize(fbSize,pixels.data());
  stbi_write_png(fromCL.outFileName.c_str(),fbSize.x,fbSize.y,4,
                 pixels.data(),fbSize.x*sizeof(uint32_t));
  
#endif
  
  world.barrier();
  barney::mpi::finalize();
  return 0;
}
