// ======================================================================== //
// Copyright 2022-2022 Ingo Wald                                            //
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

#ifndef __RENDERENGINE_DATA_H__
#define __RENDERENGINE_DATA_H__

typedef struct renderengine_cam {
	float transform_inverse_view_matrix[12];

	float lens;
	float clip_start;
	float clip_end;

	float sensor_width;
	float sensor_height;
	int sensor_fit;

	float shift_x;
	float shift_y;

	float interocular_distance;
	float convergence_distance;

	float view_camera_zoom;
	float view_camera_offset[2];
	int use_view_camera;
}renderengine_cam;

typedef struct renderengine_data {
	//char filename[1024];
	int width, height;
	//int step_samples;
	int reset;

	struct renderengine_cam cam;

}renderengine_data;

typedef struct HsDataRender {
	float colorMap[4 * 128];
	float domain[2];
	float baseDensity;
}HsDataRender;

typedef struct HsDataState {
	float world_bounds_spatial_lower[3];
	float world_bounds_spatial_upper[3];
	float scalars_range[2];
	int samples;
	float fps;
} HsDataState;

#endif
