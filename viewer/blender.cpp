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

#include <iostream>
#include <fstream>

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
		int  numFramesAccum = 1024;
		int  spp = 1;
		bool verbose = true;
		struct {
			vec3f vp = vec3f(0.f);
			vec3f vi = vec3f(0.f);
			vec3f vu = vec3f(0.f, 1.f, 0.f);
			float fovy = 60.f;
		} camera;
		bool measure = 0;

		std::string server = "localhost";
		int port_cam = 7000;
		int port_data = 7001;
	};
	FromCL fromCL;

	// std::string FromCL::outFileName = "hayStack.png";
	// bool FromCL::measure = 0;
	// bool FromCL::verbose = true;

	inline bool verbose() { return fromCL.verbose; }

	void usage(const std::string& error = "")
	{
		std::cout << "./hs{Offline,Viewer,ViewerQT} ... <args>" << std::endl;
		std::cout << "w/ args:" << std::endl;
		std::cout << "-xf file.xf   ; specify transfer function" << std::endl;
		if (!error.empty())
			throw std::runtime_error("fatal error: " + error);
		exit(0);
	}

}

using namespace hs;

renderengine_data g_renderengine_data_rcv;
renderengine_data g_renderengine_data;

double fps_previous_time = 0;
int fps_frame_count = 0;

void mul_vec(owl::common::vec3f& r, const float* mat, const owl::common::vec3f& vec)
{
	const float x = vec[0];
	const float y = vec[1];

	r[0] = x * mat[0 + 4 * 0] + y * mat[1 + 4 * 0] + mat[2 + 4 * 0] * vec[2];// +mat[3 + 4 * 0];
	r[1] = x * mat[0 + 4 * 1] + y * mat[1 + 4 * 1] + mat[2 + 4 * 1] * vec[2];// +mat[3 + 4 * 1];
	r[2] = x * mat[0 + 4 * 2] + y * mat[1 + 4 * 2] + mat[2 + 4 * 2] * vec[2];// +mat[3 + 4 * 2];
}

void mul_point(owl::common::vec3f& r, const float* mat, const owl::common::vec3f& vec)
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

int main(int ac, char** av)
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

#if 1
	int device_count;
	cudaGetDeviceCount(&device_count);
	std::string dev_properties = " (";

	for (int i = 0; i < device_count; ++i) {
		cudaDeviceProp deviceProp;
		cudaGetDeviceProperties(&deviceProp, i);
		dev_properties += std::to_string(deviceProp.pciBusID) + std::string(":") + std::to_string(deviceProp.pciDeviceID) + std::string(",");
	}
	dev_properties += ")";

	std::cout << "#hv: rank: " << world.rank << ", GPU devices: " << device_count << dev_properties << std::endl; fflush(0);
#endif

	world.barrier();

	// FromCL fromCL;
	// BarnConfig config;

	DynamicDataLoader loader(world);
	for (int i = 1; i < ac; i++) {
		const std::string arg = av[i];
		if (arg[0] != '-') {
			loader.addContent(arg);
		}
		else if (arg == "--num-frames") {
			fromCL.numFramesAccum = std::stoi(av[++i]);
		}
		else if (arg == "-spp" || arg == "-ppp" || arg == "--paths-per-pixel") {
			fromCL.spp = std::stoi(av[++i]);
		}
		else if (arg == "-mum" || arg == "--merge-unstructured-meshes" || arg == "--merge-umeshes") {
			fromCL.mergeUnstructuredMeshes = true;
		}
		else if (arg == "--no-mum") {
			fromCL.mergeUnstructuredMeshes = false;
		}
		else if (arg == "--default-radius") {
			loader.defaultRadius = std::stof(av[++i]);
		}
		else if (arg == "--measure") {
			fromCL.measure = true;
		}
		else if (arg == "-o") {
			fromCL.outFileName = av[++i];
		}
		else if (arg == "--camera") {
			fromCL.camera.vp.x = std::stof(av[++i]);
			fromCL.camera.vp.y = std::stof(av[++i]);
			fromCL.camera.vp.z = std::stof(av[++i]);
			fromCL.camera.vi.x = std::stof(av[++i]);
			fromCL.camera.vi.y = std::stof(av[++i]);
			fromCL.camera.vi.z = std::stof(av[++i]);
			fromCL.camera.vu.x = std::stof(av[++i]);
			fromCL.camera.vu.y = std::stof(av[++i]);
			fromCL.camera.vu.z = std::stof(av[++i]);
		}
		else if (arg == "-fovy") {
			fromCL.camera.fovy = std::stof(av[++i]);
		}
		else if (arg == "-xf") {
			fromCL.xfFileName = av[++i];
		}
		else if (arg == "-res") {
			fromCL.fbSize.x = std::stoi(av[++i]);
			fromCL.fbSize.y = std::stoi(av[++i]);
		}
		else if (arg == "-ndg") {
			fromCL.ndg = std::stoi(av[++i]);
		}
		else if (arg == "-dpr") {
			fromCL.dpr = std::stoi(av[++i]);
		}
		else if (arg == "-nhn" || arg == "--no-head-node") {
			fromCL.createHeadNode = false;
		}
		else if (arg == "-hn" || arg == "-chn" ||
			arg == "--head-node" || arg == "--create-head-node") {
			fromCL.createHeadNode = true;
		}
		else if (arg == "-server") {
			fromCL.server = av[++i];
		}
		else if (arg == "-port-cam") {
			fromCL.port_cam = std::stoi(av[++i]);
		}
		else if (arg == "-port-data") {
			fromCL.port_data = std::stoi(av[++i]);
		}
		else if (arg == "-h" || arg == "--help") {
			usage();
		}
		else {
			usage("unknown cmd-line argument '" + arg + "'");
		}
	}

	const bool isHeadNode = fromCL.createHeadNode && (world.rank == 0);
	hs::mpi::Comm workers = world.split(!isHeadNode);


	int numDataGroupsGlobally = fromCL.ndg;
	int dataPerRank = fromCL.dpr;
	ThisRankData thisRankData;
	if (!isHeadNode) {
		loader.loadData(thisRankData, numDataGroupsGlobally, dataPerRank, verbose());
	}
	if (fromCL.mergeUnstructuredMeshes) {
		std::cout << "merging potentially separate unstructured meshes into single mesh" << std::endl;
		thisRankData.mergeUnstructuredMeshes();
		std::cout << "done mergine umeshes..." << std::endl;
	}

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
		for (int dgID = 0; dgID < numDataGroupsLocally; dgID++)
			hayMaker.buildDataGroup(dgID);
	world.barrier();

	Renderer* renderer = nullptr;
	if (world.size == 1)
		// no MPI, render direcftly
		renderer = &hayMaker;
	else if (world.rank == 0)
		// we're in MPI mode, _and_ the rank that runs the viewer
		renderer = new MPIRenderer(world, &hayMaker);
	else {
		// we're in MPI mode, but one of the passive workers (ie NOT running the viewer)
		MPIRenderer::runWorker(world, &hayMaker);
		world.barrier();
		hs::mpi::finalize();

		exit(0);
	}

	////////////////////////////////////////////////////
	fromCL.camera.vp.x = 0;
	fromCL.camera.vp.y = 0;
	fromCL.camera.vp.z = 0;

	fromCL.camera.vi.x = 0;
	fromCL.camera.vi.y = 0;
	fromCL.camera.vi.z = -1;

	fromCL.camera.vu.x = 0;
	fromCL.camera.vu.y = 1;
	fromCL.camera.vu.z = 0;

	double render_time = 0;
	double render_time_accu = 0;
	int spp_one_step = 0;

	void* fbPointer = NULL;

	//float fix_fov = 1.0f;
	std::vector<char> pixels_buf_empty;

	//std::vector<vec4f> haystack_data(128 + 1);
	//std::vector<vec4f> haystack_data_rcv(128 + 1);

	//const int colorMapCount = 128;
	HsDataRender hsDataRender, hsDataRenderRcv;

	TransferFunction xf;
	xf.colorMap.resize(sizeof(hsDataRender.colorMap) / sizeof(vec4f));
	xf.domain = range1f(0.f, 1.0f);

	int total_samples = 0;
	hs::Camera camera;

	double t2 = getCurrentTime();

	/////////

	HsDataState hsDataState;
	memset(&hsDataState, 0, sizeof(hsDataState));
	hsDataState.world_bounds_spatial_lower[0] = worldBounds.spatial.lower[0];
	hsDataState.world_bounds_spatial_lower[1] = worldBounds.spatial.lower[1];
	hsDataState.world_bounds_spatial_lower[2] = worldBounds.spatial.lower[2];
	hsDataState.world_bounds_spatial_upper[0] = worldBounds.spatial.upper[0];
	hsDataState.world_bounds_spatial_upper[1] = worldBounds.spatial.upper[1];
	hsDataState.world_bounds_spatial_upper[2] = worldBounds.spatial.upper[2];

	hsDataState.scalars_range[0] = worldBounds.scalars.lo;
	hsDataState.scalars_range[1] = worldBounds.scalars.hi;

	init_sockets_cam(fromCL.server.c_str(), fromCL.port_cam, fromCL.port_data);
	//send_data_cam((char*)&hsDataState, sizeof(hsDataState));
	/////////

	while (true) {
		recv_data_cam((char*)&g_renderengine_data_rcv, sizeof(renderengine_data));
		if (is_error()) {
			throw std::runtime_error("TCP Error!");
		}

		if (g_renderengine_data_rcv.reset) {
			// if (renderer != NULL) {
			// 	delete renderer;
			// 	renderer = NULL;
			// }

			client_close();
			server_close();

			continue;
		}

		recv_data_cam((char*)&hsDataRenderRcv, sizeof(HsDataRender));
		if (is_error()) {
			throw std::runtime_error("TCP Error!");
		}

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
				total_samples = 0;
				render_time = 0;
				render_time_accu = 0;
				//renderer->config.camera.dirty = true;

				if (g_renderengine_data_rcv.reset || fromCL.fbSize.x != g_renderengine_data_rcv.width || fromCL.fbSize.y != g_renderengine_data_rcv.height) {
					fromCL.fbSize.x = g_renderengine_data_rcv.width;
					fromCL.fbSize.y = g_renderengine_data_rcv.height;

					cudaSetDevice(0);

					if (fbPointer)
						cudaFree(fbPointer);

#ifdef WITH_CLIENT_GPUJPEG
					cudaMalloc(&fbPointer, fromCL.fbSize.x * fromCL.fbSize.y * sizeof(uint32_t));
#else
					cudaMallocManaged(&fbPointer, fromCL.fbSize.x * fromCL.fbSize.y * sizeof(uint32_t));
#endif          
					renderer->resize(fromCL.fbSize, (uint32_t*)fbPointer);
				}

				//renderer->setNumPPP(g_renderengine_data_rcv.step_samples);				
				camera.fovy = g_renderengine_data_rcv.cam.lens;

				mul_point(camera.vp, g_renderengine_data_rcv.cam.transform_inverse_view_matrix, fromCL.camera.vp);
				mul_point(camera.vi, g_renderengine_data_rcv.cam.transform_inverse_view_matrix, fromCL.camera.vi);
				mul_vec(camera.vu, g_renderengine_data_rcv.cam.transform_inverse_view_matrix, fromCL.camera.vu);

				renderer->setCamera(camera);
			}

			// if (renderer == NULL) {
			// 	continue;
			// }

			if (memcmp(&hsDataRender, &hsDataRenderRcv, sizeof(HsDataRender))) {
				memcpy(&hsDataRender, &hsDataRenderRcv, sizeof(HsDataRender));

				memcpy(xf.colorMap.data(), hsDataRender.colorMap, sizeof(vec4f) * xf.colorMap.size());
				xf.domain = range1f(hsDataRender.domain[0], hsDataRender.domain[1]);
				xf.baseDensity = hsDataRender.baseDensity;

				//renderer->setColorMap(haystack_data);
				renderer->setTransferFunction(xf);
				renderer->resetAccumulation();
				total_samples = 0;

				/////////////////////////////////////////////////////////////////////////
				//int save_to_file = haystack_data[128].w;
				//if (save_to_file == 1) {
				//	/////XF
				//	if (fromCL.xfFileName.length() > 0) {
				//		const char* fname = fromCL.xfFileName.c_str();
				//		std::ofstream ofs(fname, std::ios::binary);

				//		if (!ofs.good())
				//			std::cerr << "Cannot save tf at " << fname << "\n";

				//		static const size_t xfFileFormatMagic = 0x1235abc000;
				//		ofs.write((char*)&xfFileFormatMagic, sizeof(xfFileFormatMagic));

				//		float opacityScale = log(xf.baseDensity) / log(1.1f) + 100.f;
				//		ofs.write((char*)&opacityScale, sizeof(opacityScale));

				//		const auto range = xf.domain;
				//		ofs.write((char*)&range.lower, sizeof(range.lower));
				//		ofs.write((char*)&range.upper, sizeof(range.upper));

				//		owl::interval<float> relDomain = { 0.f, 100.f };
				//		ofs.write((char*)&relDomain, sizeof(relDomain));

				//		const auto cmap = xf.colorMap;
				//		const int numColorMapValues = cmap.size();

				//		ofs.write((char*)&numColorMapValues, sizeof(numColorMapValues));
				//		ofs.write((char*)&cmap[0], numColorMapValues * sizeof(vec4f));

				//		ofs.close();

				//		std::cout << "TFE saved to " << fname << "\n";

				//		////Camera
				//		std::string fname_cam = fromCL.xfFileName + ".cam";
				//		std::ofstream ofs_cam(fname_cam.c_str());
				//		if (!ofs_cam.good())
				//			std::cerr << "Cannot save tf at " << fname_cam << "\n";

				//		ofs_cam << "--camera" << " ";

				//		ofs_cam << camera.vp.x << " ";
				//		ofs_cam << camera.vp.y << " ";
				//		ofs_cam << camera.vp.z << " ";
				//		ofs_cam << camera.vi.x << " ";
				//		ofs_cam << camera.vi.y << " ";
				//		ofs_cam << camera.vi.z << " ";
				//		ofs_cam << camera.vu.x << " ";
				//		ofs_cam << camera.vu.y << " ";
				//		ofs_cam << camera.vu.z << " ";

				//		ofs_cam << "-fovy" << " ";
				//		ofs_cam << camera.fovy << " ";
				//		ofs_cam.close();

				//		std::cout << "Camera saved to " << fname_cam << "\n";
				//	}
				//}
				/////////////////////////////////////////////////////////////////////////
			}

			// spp_one_step = renderer->getTotalSamples();
			// double start_render_time = getCurrentTime();				
	  /////////////////////////////////////////////////
			//renderer->renderFrame();
			double t0 = getCurrentTime();
			renderer->renderFrame(fromCL.spp);
			double t1 = getCurrentTime();
			static double sum_t = 0.f;
			static double sum_w = 0.f;
			sum_t = 0.8f * sum_t + (t1 - t0);
			sum_w = 0.8f * sum_w + 1.f;
			float timePerFrame = sum_t / sum_w;
			float fps = 1.f / timePerFrame;

			if (getCurrentTime() - t2 > 2.0) {
				std::string title = "HayThere (" + prettyDouble(fps) + "fps), " + std::to_string(t0) + ", " + std::to_string(t1);
				std::cout << title << std::endl;
				t2 = getCurrentTime();
			}
			/////////////////////////////////////////////////
			total_samples++;

			cudaDeviceSynchronize();

			cudaSetDevice(0);

#ifdef WITH_CLIENT_GPUJPEG     
			send_gpujpeg((char*)fbPointer, pixels_buf_empty.data(), fromCL.fbSize.x, fromCL.fbSize.y);
#else
			char* pixels_buf = (char*)fbPointer; //renderer->getBuffer();
			//((int*)pixels_buf)[0] = total_samples; //renderer->getTotalSamples();

			send_data_data((char*)fbPointer, pixels_buf_empty.size());
#endif
			if (is_error()) {
				throw std::runtime_error("TCP Error!");
			}

			hsDataState.fps = fps;
			hsDataState.samples = total_samples;
			send_data_data((char*)&hsDataState, sizeof(hsDataState));

			if (is_error()) {
				throw std::runtime_error("TCP Error!");
			}
		}
		catch (const std::exception& ex)
		{
			std::cerr << ex.what();
			exit(-1);
		}
	}

	////////////////////////////////////////////////////  

	renderer->terminate();

	world.barrier();
	hs::mpi::finalize();
	return 0;
}
