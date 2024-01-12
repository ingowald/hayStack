// ======================================================================== //
// Copyright 2022-2023 Ingo Wald                                            //
// Copyright 2022-2023 IT4Innovations, VSB - Technical University of Ostrava//
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

#include "renderengine_data.h"
#include "renderengine_tcp.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION 1
#define STB_IMAGE_IMPLEMENTATION 1
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"
#include <cuda_runtime.h>

namespace hs {

  struct FromCL {
    /*! data groups per rank. '0' means 'auto - use few as we can, as
        many as we have to fit for given number of ranks */
    int dpr = 0;
    /*! num data groups */
    int ndg = 1;

    bool mergeUnstructuredMeshes = false;
    
    std::string xfFileName = "";
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
    static bool measure;
  };
  
  bool FromCL::measure = 0;
  bool FromCL::verbose = true;

  inline bool verbose() { return FromCL::verbose; }
  
  void usage(const std::string &error="")
  {
    std::cout << "./haystack ... <args>" << std::endl;
    if (!error.empty())
      throw std::runtime_error("fatal error: " +error);
    exit(0);
  }
  
}

using namespace hs;

renderengine_data g_renderengine_data_rcv;
renderengine_data g_renderengine_data;

double fps_previous_time = 0;
int fps_frame_count = 0;

void mul_vec(owl::common::vec3f& r, const float *mat, const owl::common::vec3f &vec)
{
	const float x = vec[0];
	const float y = vec[1];

	r[0] = x * mat[0 + 4 * 0] + y * mat[1 + 4 * 0] + mat[2 + 4 * 0] * vec[2];// +mat[3 + 4 * 0];
	r[1] = x * mat[0 + 4 * 1] + y * mat[1 + 4 * 1] + mat[2 + 4 * 1] * vec[2];// +mat[3 + 4 * 1];
	r[2] = x * mat[0 + 4 * 2] + y * mat[1 + 4 * 2] + mat[2 + 4 * 2] * vec[2];// +mat[3 + 4 * 2];
}

void mul_point(owl::common::vec3f& r, const float* mat, const owl::common::vec3f&vec)
{
	const float x = vec[0];
	const float y = vec[1];

	r[0] = x * mat[0 + 4 * 0] + y * mat[1 + 4 * 0] + mat[2 + 4 * 0] * vec[2] + mat[3 + 4 * 0];
	r[1] = x * mat[0 + 4 * 1] + y * mat[1 + 4 * 1] + mat[2 + 4 * 1] * vec[2] + mat[3 + 4 * 1];
	r[2] = x * mat[0 + 4 * 2] + y * mat[1 + 4 * 2] + mat[2 + 4 * 2] * vec[2] + mat[3 + 4 * 2];
}

void display_fps(int samples, double render_time, double render_time_accu, int spp_one_step)
{
	double current_time = getCurrentTime();
	fps_frame_count++;

	if (current_time - fps_previous_time >= 3.0) {

		printf("FPS: %.2f, samples: %d, render_time: %.2f, render_time_fps: %.2f, render_time_accu: %.2f, spp_one_step: %d\n", (double)fps_frame_count / (current_time - fps_previous_time), samples, render_time, 1.0 / render_time, render_time_accu, spp_one_step);
		fps_frame_count = 0;
		fps_previous_time = getCurrentTime();
	}
}	

int main(int ac, char **av)
{
  hs::mpi::init(ac,av);
  hs::mpi::Comm world(MPI_COMM_WORLD);

  world.barrier();
  if (world.rank == 0)
    std::cout << "#hv: hsviewer starting up" << std::endl; fflush(0);
  world.barrier();

  FromCL fromCL;
  // BarnConfig config;

  DynamicDataLoader loader;
  for (int i=1;i<ac;i++) {
    const std::string arg = av[i];
    if (arg[0] != '-') {
      loader.addContent(arg);
    } else if (arg == "-mum" || arg == "--merge-unstructured-meshes") {
      fromCL.mergeUnstructuredMeshes = true;
    } else if (arg == "--default-radius") {
      loader.defaultRadius = std::stof(av[++i]);
    } else if (arg == "--measure") {
      fromCL.measure = true;
    } else if (arg == "-o") {
      fromCL.outFileName = av[++i];
    } else if (arg == "--camera") {
      fromCL.camera.vp.x = std::stof(av[++i]);
      fromCL.camera.vp.y = std::stof(av[++i]);
      fromCL.camera.vp.z = std::stof(av[++i]);
      fromCL.camera.vi.x = std::stof(av[++i]);
      fromCL.camera.vi.y = std::stof(av[++i]);
      fromCL.camera.vi.z = std::stof(av[++i]);
      fromCL.camera.vu.x = std::stof(av[++i]);
      fromCL.camera.vu.y = std::stof(av[++i]);
      fromCL.camera.vu.z = std::stof(av[++i]);
    } else if (arg == "-fovy") {
      fromCL.camera.fovy = std::stof(av[++i]);
    } else if (arg == "-xf") {
      fromCL.xfFileName = av[++i];
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
  hs::mpi::Comm workers = world.split(!isHeadNode);

  int numDataGroupsGlobally = fromCL.ndg;
  int dataPerRank   = fromCL.dpr;
  ThisRankData thisRankData;
  if (!isHeadNode) {
    loader.loadData(workers,thisRankData,numDataGroupsGlobally,dataPerRank,verbose());
  }
  if (fromCL.mergeUnstructuredMeshes)
    thisRankData.mergeUnstructuredMeshes();
  
  int numDataGroupsLocally = thisRankData.size();
  


  HayMaker hayMaker(/* the ring that binds them all : */world,
                    /* the workers */workers,
                    thisRankData,
                    verbose());
  

  world.barrier();
  const BoundsData worldBounds = hayMaker.getWorldBounds();
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
  hayMaker.createBarney();
  world.barrier();
  if (world.rank == 0)
    std::cout << OWL_TERMINAL_CYAN
              << "#hs: building barney data groups"
              << OWL_TERMINAL_DEFAULT << std::endl;
  if (!isHeadNode)
    for (int dgID=0;dgID<numDataGroupsLocally;dgID++)
      hayMaker.buildDataGroup(dgID);
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
    hs::mpi::finalize();
    exit(0);
  }

   //auto &fbSize = fromCL.fbSize;
//   std::vector<uint32_t> pixels(fbSize.x*fbSize.y);
//   renderer->resize(fbSize,pixels.data());

//   hs::Camera camera;
//   camera.vp = fromCL.camera.vp;
//   camera.vu = fromCL.camera.vu;
//   camera.vi = fromCL.camera.vi;
//   camera.fovy = fromCL.camera.fovy;
//   renderer->setCamera(camera);


////////////////////////////////////////////////////
	// Config config;
	// config.args(ac, av);

	// config.camera.vp.x = 0;
	// config.camera.vp.y = 0;
	// config.camera.vp.z = 0;

	// config.camera.vi.x = 0;
	// config.camera.vi.y = 0;
	// config.camera.vi.z = -1;

	// config.camera.vu.x = 0;
	// config.camera.vu.y = 1;
	// config.camera.vu.z = 0;

	// //MaxiOWL renderer(config);
	// MaxiOWL* renderer = NULL; // new MaxiOWL(config);

	double render_time = 0;
	double render_time_accu = 0;
	int spp_one_step = 0;

	void* fbPointer = NULL;

	//float fix_fov = 1.0f;
	std::vector<char> pixels_buf_empty;

	//std::vector<vec4f> volume_color_ramp(128);
	std::vector<vec4f> volume_color_ramp_rcv(128);

//  27   struct TransferFunction {                                                               
//  28     std::vector<vec4f> colorMap = { vec4f(1.f), vec4f(1.f) };                             
//  29     range1f domain = { 0.f, 0.f };                                                        
//  30     float   baseDensity = 1.f;                                                            
//  31   };
  TransferFunction xf;
  xf.colorMap.resize(128);
  xf.domain = range1f(0.f, 1.0f); 

	while (true) {		
		recv_data_cam((char*)&g_renderengine_data_rcv, sizeof(renderengine_data));

		if (g_renderengine_data_rcv.reset) {
			// if (renderer != NULL) {
			// 	delete renderer;
			// 	renderer = NULL;
			// }

			client_close();
			server_close();

			continue;
		}

		recv_data_cam((char*)volume_color_ramp_rcv.data(), sizeof(vec4f) * volume_color_ramp_rcv.size());

		if (pixels_buf_empty.size() != sizeof(uint32_t) * g_renderengine_data_rcv.width * g_renderengine_data_rcv.height) {
			pixels_buf_empty.resize(sizeof(uint32_t) * g_renderengine_data_rcv.width * g_renderengine_data_rcv.height);
		}

		try {

			// cam_change
			if (/*renderer == NULL || */ memcmp(&g_renderengine_data, &g_renderengine_data_rcv, sizeof(renderengine_data))) {
				memcpy(&g_renderengine_data, &g_renderengine_data_rcv, sizeof(renderengine_data));

				// if (renderer == NULL) {
				// 	config.inFileNames.clear();
				// 	config.inFileNames.push_back(std::string(g_renderengine_data_rcv.filename));

				// 	renderer = new MaxiOWL(config);
				// 	fromCL.fbSize.x = 0;
				// 	fromCL.fbSize.y = 0;

				// 	//continue;
				// }

				//Camera cam = renderer->getCamera();
				renderer->resetAccumulation();
				render_time = 0;
				render_time_accu = 0;
				//renderer->config.camera.dirty = true;

				if (g_renderengine_data_rcv.reset || fromCL.fbSize.x != g_renderengine_data_rcv.width || fromCL.fbSize.y != g_renderengine_data_rcv.height) {
					fromCL.fbSize.x = g_renderengine_data_rcv.width;
					fromCL.fbSize.y = g_renderengine_data_rcv.height;

					if (fbPointer)
						cudaFree(fbPointer);

					cudaMallocManaged(&fbPointer, fromCL.fbSize.x * fromCL.fbSize.y * sizeof(uint32_t));
					renderer->resize(fromCL.fbSize, (uint32_t*)fbPointer);
				}

				//renderer->setNumPPP(g_renderengine_data_rcv.step_samples);

				hs::Camera camera;
				camera.fovy = g_renderengine_data_rcv.cam.lens;				

				mul_point(camera.vp, g_renderengine_data_rcv.cam.transform_inverse_view_matrix, fromCL.camera.vp);
				mul_point(camera.vi, g_renderengine_data_rcv.cam.transform_inverse_view_matrix, fromCL.camera.vi);
				mul_vec(camera.vu, g_renderengine_data_rcv.cam.transform_inverse_view_matrix, fromCL.camera.vu);

				renderer->setCamera(camera);
			}

			// if (renderer == NULL) {
			// 	continue;
			// }

			if (memcmp(xf.colorMap.data(), volume_color_ramp_rcv.data(), sizeof(vec4f) * volume_color_ramp_rcv.size())) {
				memcpy(xf.colorMap.data(), volume_color_ramp_rcv.data(), sizeof(vec4f) * volume_color_ramp_rcv.size());

				//renderer->setColorMap(volume_color_ramp);
        renderer->setTransferFunction(xf);
				renderer->resetAccumulation();
			}

			// spp_one_step = renderer->getTotalSamples();
			// double start_render_time = getCurrentTime();				
			renderer->renderFrame();				
			// render_time = getCurrentTime() - start_render_time;
			// render_time_accu += render_time;
			// spp_one_step = renderer->getTotalSamples() - spp_one_step;

			cudaDeviceSynchronize();
			//char* pixels_buf = (char*)fbPointer; //renderer->getBuffer();
			//((int*)pixels_buf)[0] = renderer->getTotalSamples();

			//vec2i resolutuon = renderer->getResolution();
			send_data_data((char*)fbPointer, pixels_buf_empty.size());
		}
		catch (const std::exception &ex)
		{
			std::cerr << ex.what();
			send_data_data(pixels_buf_empty.data(), pixels_buf_empty.size());
		}

		// if (renderer != NULL)
		// 	display_fps(renderer->getTotalSamples(), render_time, render_time_accu, spp_one_step);
	}
	
////////////////////////////////////////////////////  

// #if 1
//   double t = getCurrentTime();
//   double t2 = getCurrentTime();
//   while(getCurrentTime() - t < 20.0) {
//       double t0 = getCurrentTime();
//       renderer->renderFrame();
//       double t1 = getCurrentTime();
//       static double sum_t = 0.f;
//       static double sum_w = 0.f;
//       sum_t = 0.8f*sum_t + (t1-t0);
//       sum_w = 0.8f*sum_w + 1.f;
//       float timePerFrame = sum_t / sum_w;
//       float fps = 1.f/timePerFrame;
//       std::string title = "HayThere ("+prettyDouble(fps)+"fps)";

//       if(getCurrentTime() - t2 > 2.0) {
//         std::cout << title << std::endl;
//         t2 = getCurrentTime();
//       }
//   }
// #else    
//   renderer->renderFrame();
// #endif 

//   stbi_write_png(fromCL.outFileName.c_str(),fbSize.x,fbSize.y,4,
//                  pixels.data(),fbSize.x*sizeof(uint32_t));

  renderer->terminate();
// #endif

  world.barrier();
  hs::mpi::finalize();
  return 0;
}
