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

#ifndef __HAYSTACK_API_H__
#define __HAYSTACK_API_H__

#if defined(__APPLE__)
#define HAYSTACK_EXPORT_DLL
#define HAYSTACK_EXPORT_STD
#elif defined(_WIN32)
#define HAYSTACK_EXPORT_DLL __declspec(dllexport)
#define HAYSTACK_EXPORT_STD __stdcall
#else
#define HAYSTACK_EXPORT_DLL
#define HAYSTACK_EXPORT_STD
#endif

#ifdef __cplusplus
extern "C"
{
#endif

	HAYSTACK_EXPORT_DLL void HAYSTACK_EXPORT_STD resize(int width, int height);

	HAYSTACK_EXPORT_DLL int HAYSTACK_EXPORT_STD recv_pixels_data();
	HAYSTACK_EXPORT_DLL int HAYSTACK_EXPORT_STD send_cam_data();
	HAYSTACK_EXPORT_DLL void HAYSTACK_EXPORT_STD set_timestep(int timestep);

	HAYSTACK_EXPORT_DLL void HAYSTACK_EXPORT_STD client_init(const char *server, int port_cam, int port_data, int w, int h/*, int step_samples, const char* filename*/);
	HAYSTACK_EXPORT_DLL void HAYSTACK_EXPORT_STD client_close_connection();

	HAYSTACK_EXPORT_DLL void HAYSTACK_EXPORT_STD set_camera(void *view_martix,
														  float lens,
														  float nearclip,
														  float farclip,
														  float sensor_width,
														  float sensor_height,
														  int sensor_fit,
														  float view_camera_zoom,
														  float view_camera_offset0,
														  float view_camera_offset1,
														  int use_view_camera,
														  float shift_x,
														  float shift_y);

	//HAYSTACK_EXPORT_DLL void HAYSTACK_EXPORT_STD get_pixels(void *pixels);
	HAYSTACK_EXPORT_DLL void HAYSTACK_EXPORT_STD draw_texture();

	//HAYSTACK_EXPORT_DLL int HAYSTACK_EXPORT_STD get_samples();
	HAYSTACK_EXPORT_DLL int HAYSTACK_EXPORT_STD get_current_samples();
	HAYSTACK_EXPORT_DLL float HAYSTACK_EXPORT_STD get_remote_fps();
	HAYSTACK_EXPORT_DLL float HAYSTACK_EXPORT_STD get_local_fps();

	HAYSTACK_EXPORT_DLL void HAYSTACK_EXPORT_STD reset();

	HAYSTACK_EXPORT_DLL void HAYSTACK_EXPORT_STD send_haystack_data_render(void* colorMap, int colorMapSize, void* domain, void* baseDensity);
	//HAYSTACK_EXPORT_DLL void HAYSTACK_EXPORT_STD rcv_haystack_data_init(const char* server,
	//	int port_cam,
	//	int port_data, 
	//	void* world_bounds_spatial_lower, 
	//	void* world_bounds_spatial_upper, 
	//	void* scalars_range);

	//HAYSTACK_EXPORT_DLL void HAYSTACK_EXPORT_STD  haystack_init(const char* server,
	//	int port_cam,
	//	int port_data);

	HAYSTACK_EXPORT_DLL void HAYSTACK_EXPORT_STD  get_haystack_range(
		void* world_bounds_spatial_lower,
		void* world_bounds_spatial_upper,
		void* scalars_range);

	HAYSTACK_EXPORT_DLL int HAYSTACK_EXPORT_STD get_texture_id();

	HAYSTACK_EXPORT_DLL int HAYSTACK_EXPORT_STD com_error();
	

#ifdef __cplusplus
}
#endif
#endif
