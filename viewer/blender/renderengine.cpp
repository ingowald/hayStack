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

#include "renderengine_api.h"
#include "renderengine_data.h"

#include "renderengine_tcp.h"

#include <iostream>
#ifdef _WIN32
	#include <omp.h>
#endif
#include <string.h>
#include <string>
#include <stdlib.h>

//////////////////////////

#ifdef _WIN32
int setenv(const char* name, const char* value, int overwrite)
{
	int errcode = 0;
	if (!overwrite) {
		size_t envsize = 0;
		errcode = getenv_s(&envsize, NULL, 0, name);
		if (errcode || envsize)
			return errcode;
	}
	return _putenv_s(name, value);
}
#endif

unsigned int g_width = 2;
unsigned int g_height = 1;

unsigned char *g_pixels_buf = NULL; //[3] = {NULL, NULL, NULL};

renderengine_data g_renderengine_data;

double g_previousTime[3] = {0, 0, 0};
int g_frameCount[3] = {0, 0, 0};
char fname[1024];

float g_right_eye = 0.035f;

struct stl_tri;
stl_tri *polys = NULL;
size_t polys_size = 0;

int current_samples = 0;

int active_gpu = 1;

/////////////////////////

void displayFPS(int type, int tot_samples = 0)
{
#if _WIN32
	double currentTime = omp_get_wtime();
	g_frameCount[type]++;

	if (currentTime - g_previousTime[type] >= 3.0)
	{
		double fps = (double)g_frameCount[type] / (currentTime - g_previousTime[type]);
		if (fps > 0.01)
		{
			char sTemp[1024];

			int *samples = (int *)&g_renderengine_data.step_samples;

			sprintf(sTemp,
					"FPS: %.2f, Total Samples: %d, Samples: : %d, Res: %d x %d",
					fps,
					tot_samples,
					samples[0],
					g_width,
					g_height);
			printf("%s\n", sTemp);
		}
		g_frameCount[type] = 0;
		g_previousTime[type] = omp_get_wtime();
	}
#endif
}

//////////////////////////

void resize(int width, int height)
{
	if (width == g_width && height == g_height && g_pixels_buf)
		return;

	if (g_pixels_buf)
	{
		delete[] g_pixels_buf;
	}

	g_width = width;
	g_height = height;

	g_pixels_buf = new unsigned char[(size_t)width * height * sizeof(char) * 4];
	memset(g_pixels_buf, 0, (size_t)width * height * sizeof(char) * 4);

	int *size = (int *)&g_renderengine_data.width;
	size[0] = width;
	size[1] = height;
}

int recv_pixels_data()
{
	recv_data_data((char *)g_pixels_buf,
				   g_width * g_height * sizeof(char) * 4 /*, false*/);

	current_samples = ((int *)g_pixels_buf)[0];
	displayFPS(1, current_samples);

	return 0;
}

int send_cam_data()
{
	send_data_cam((char *)&g_renderengine_data, sizeof(renderengine_data));

	return 0;
}

void reset()
{
	renderengine_data rd;
	rd.reset = 1;

	send_data_cam((char *)&rd, sizeof(renderengine_data));
}

void set_haystack_data(void* values, int size)
{
	send_data_cam((char*)values, /*sizeof(float) * 4 * 129*/ size);
}

void set_timestep(int timestep)
{
	set_port_offset(timestep);
}

void client_init(const char *server,
				 int port_cam,
				 int port_data,
				 int w,
				 int h,
				 int step_samples,
				 const char *filename)
{
	//init_sockets_cam(server, port_cam, port_data);
	setenv("SOCKET_SERVER_NAME_CAM", server, 1);
	setenv("SOCKET_SERVER_NAME_DATA", server, 1);

	char stemp[128];
	sprintf(stemp, "%d", port_cam);
	setenv("SOCKET_SERVER_PORT_CAM", stemp, 1);
	sprintf(stemp, "%d", port_data);
	setenv("SOCKET_SERVER_PORT_DATA", stemp, 1);

	g_renderengine_data.step_samples = step_samples;
	strcpy(g_renderengine_data.filename, filename);

	resize(w, h);
}

void client_close_connection()
{
	client_close();
	server_close();
}

void set_camera(void *view_martix,
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
				float shift_y)
{
	memcpy(
		(char *)g_renderengine_data.cam.transform_inverse_view_matrix, view_martix, sizeof(float) * 12);

	g_renderengine_data.cam.lens = lens;
	g_renderengine_data.cam.clip_start = nearclip;
	g_renderengine_data.cam.clip_end = farclip;

	g_renderengine_data.cam.sensor_width = sensor_width;
	g_renderengine_data.cam.sensor_height = sensor_height;
	g_renderengine_data.cam.sensor_fit = sensor_fit;

	g_renderengine_data.cam.view_camera_zoom = view_camera_zoom;
	g_renderengine_data.cam.view_camera_offset[0] = view_camera_offset0;
	g_renderengine_data.cam.view_camera_offset[1] = view_camera_offset1;
	g_renderengine_data.cam.use_view_camera = use_view_camera;
	g_renderengine_data.cam.shift_x = shift_x;
	g_renderengine_data.cam.shift_y = shift_y;
}

int get_samples()
{
	int *samples = (int *)&g_renderengine_data.step_samples;
	return samples[0];
}

int get_current_samples()
{
	return current_samples;
}

void get_pixels(void *pixels)
{
	size_t pix_type_size = sizeof(char) * 4; // sizeof(char) * 4;
	memcpy(pixels, (char *)g_pixels_buf, g_width * g_height * pix_type_size);
}
