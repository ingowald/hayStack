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

//#ifdef _WIN32
//#	include <glad/glad.h>
//#endif
#include <epoxy/gl.h>

#include <cuda_gl_interop.h>
#include "renderengine_tcp.h"

#ifdef _WIN32
#include <omp.h>
#endif

#include <iostream>
#include <string.h>
#include <string>
#include <stdlib.h>
//#include <vector>

#ifdef TCP_FLOAT
#define PIX_SIZE sizeof(float)
#else
#define PIX_SIZE sizeof(char)
#endif

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

unsigned char* g_pixels_buf = NULL;
void* g_pixels_buf_d = NULL;
void* g_pixels_buf_recv_d = NULL;
GLuint g_bufferId;   // ID of PBO
GLuint g_textureId;  // ID of texture

renderengine_data g_renderengine_data;
HsDataState g_hs_data_state;

double g_previousTime[3] = { 0, 0, 0 };
int g_frameCount[3] = { 0, 0, 0 };
char fname[1024];

float g_right_eye = 0.035f;

struct stl_tri;
stl_tri* polys = NULL;
size_t polys_size = 0;

//int current_samples = 0;

int active_gpu = 1;
float local_fps = 0;

/////////////////////////

void displayFPS(int type, int tot_samples = 0)
{
	double currentTime = omp_get_wtime();
	g_frameCount[type]++;

	local_fps = (double)g_frameCount[type] / (currentTime - g_previousTime[type]);

	if (currentTime - g_previousTime[type] >= 3.0)
	{		
		if (local_fps > 0.01)
		{
			char sTemp[1024];

			//int* samples = (int*)&g_renderengine_data.step_samples;

			sprintf(sTemp,
				"FPS: %.2f, Total Samples: %d, Res: %d x %d",
				local_fps,
				tot_samples,
				g_width,
				g_height);
			printf("%s\n", sTemp);
		}
		g_frameCount[type] = 0;
		g_previousTime[type] = omp_get_wtime();
	}
}

//////////////////////////
void check_exit()
{
}

#define cuda_assert(stmt) \
  { \
    if (stmt != cudaSuccess) { \
      char err[1024]; \
      sprintf(err, \
              "CUDA error: %s: %s in %s, line %d", \
              cudaGetErrorName(stmt), \
              cudaGetErrorString(stmt), \
              #stmt, \
              __LINE__); \
      std::string message(err); \
      fprintf(stderr, "%s\n", message.c_str()); \
      check_exit(); \
    } \
  } \
  (void)0

bool gpu_error_(cudaError_t result, const std::string& stmt)
{
	if (result == cudaSuccess)
		return false;

	char err[1024];
	sprintf(err,
		"CUDA error at %s: %s: %s",
		stmt.c_str(),
		cudaGetErrorName(result),
		cudaGetErrorString(result));
	std::string message(err);
	fprintf(stderr, "%s\n", message.c_str());
	return true;
}

#define gpu_error(stmt) gpu_error_(stmt, #stmt)

void gpu_error_message(const std::string& message)
{
	fprintf(stderr, "%s\n", message.c_str());
}

void cuda_set_device()
{
	cuda_assert(cudaSetDevice(0));
}

void setup_texture()
{
	GLuint pboIds[1];      // IDs of PBO
	GLuint textureIds[1];  // ID of texture

	glGenTextures(1, textureIds);
	g_textureId = textureIds[0];

	glBindTexture(GL_TEXTURE_2D, g_textureId);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	//glTexImage2D(GL_TEXTURE_2D,
	//	0,
	//	GL_RGBA8,
	//	g_width,
	//	g_height,
	//	0,
	//	GL_RGBA,
	//	GL_UNSIGNED_BYTE,
	//	NULL);

	glTexImage2D(GL_TEXTURE_2D,
		0,
		GL_RGBA,
		g_width,
		g_height,
		0,
		GL_RGBA,
#ifdef TCP_FLOAT
		GL_FLOAT,
#else
		GL_UNSIGNED_BYTE,
#endif
		NULL);

	glBindTexture(GL_TEXTURE_2D, 0);

	glGenBuffers(1, pboIds);
	g_bufferId = pboIds[0];

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, g_bufferId);

	glBufferData(GL_PIXEL_UNPACK_BUFFER,
		(size_t)g_width * g_height * 4 * PIX_SIZE,
		0,
		GL_DYNAMIC_COPY);

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	cuda_set_device();
	cuda_assert(cudaGLRegisterBufferObject(g_bufferId));
	//cuda_assert(cudaGLMapBufferObject((void**)&g_pixels_buf_d, g_bufferId));
	cuda_assert(cudaMalloc(&g_pixels_buf_recv_d, (size_t)g_width * g_height * 4 * PIX_SIZE));	
}

void free_texture()
{
	cuda_set_device();
	//cuda_assert(cudaGLUnmapBufferObject(g_bufferId));
	cuda_assert(cudaGLUnregisterBufferObject(g_bufferId));

	cuda_assert(cudaFree(g_pixels_buf_recv_d));

	glDeleteFramebuffers(1, &g_bufferId);
	glDeleteTextures(1, &g_textureId);
}

void to_ortho()
{
	// set viewport to be the entire window
	glViewport(0, 0, (GLsizei)g_width, (GLsizei)g_height);

	// set orthographic viewing frustum
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glOrtho(0, 1, 0, 1, -1, 1);

	//glOrtho(0, 0.5, 0, 1, -1, 1);
	//glOrtho(0.5, 1, 0, 1, -1, 1);

	// switch to modelview matrix in order to set scene
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

#if 0
void draw_texture()
{
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	// bind the texture and PBO
	glBindTexture(GL_TEXTURE_2D, g_textureId);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, g_bufferId);

	// copy pixels from PBO to texture object
	// use offset instead of pointer.
	glTexSubImage2D(GL_TEXTURE_2D,
		0,
		0,
		0,
		g_width,
		g_height,
		GL_RGBA,
		GL_UNSIGNED_BYTE,
		0);

	//// draw
	//to_ortho();
	//

 // // draw a point with texture
	//glBindTexture(GL_TEXTURE_2D, g_textureId);
	//glBegin(GL_QUADS);

	//glTexCoord2d(0.0, 0.0);
	//glVertex2d(0.0, 0.0);
	//glTexCoord2d(1.0, 0.0);
	//glVertex2d(1, 0.0);
	//glTexCoord2d(1.0, 1.0);
	//glVertex2d(1, 1);
	//glTexCoord2d(0.0, 1.0);
	//glVertex2d(0.0, 1);

	//glEnd();	

	//// unbind texture
	//glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	//glBindTexture(GL_TEXTURE_2D, 0);

	int x = 0;
	int y = 0;
	int width = g_width;
	int height = g_height;

	//def _draw_texture(texture_id, x, y, width, height) :

	//	# Getting shader program
	//	shader_program = bgl.Buffer(bgl.GL_INT, 1)
	//	bgl.glGetIntegerv(bgl.GL_CURRENT_PROGRAM, shader_program)
	GLint shaderProgram[1];
	glGetIntegerv(GL_CURRENT_PROGRAM, shaderProgram);

	//	# Generate vertex array
	//	vertex_array = bgl.Buffer(bgl.GL_INT, 1)
	//	bgl.glGenVertexArrays(1, vertex_array)
	GLuint vertexArray[1];
	glGenVertexArrays(1, vertexArray);

	//	texturecoord_location = bgl.glGetAttribLocation(
	//		shader_program[0], "texCoord")
	//	position_location = bgl.glGetAttribLocation(shader_program[0], "pos")

	GLint textureCoordLocation = glGetAttribLocation(shaderProgram[0], "texCoord");
	GLint positionLocation = glGetAttribLocation(shaderProgram[0], "pos");

	//# Generate geometry buffers for drawing textured quad
	//	position = [x, y, x + width, y, x + width, y + height, x, y + height]
	//	position = bgl.Buffer(bgl.GL_FLOAT, len(position), position)
	//	texcoord = [0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0]
	//	texcoord = bgl.Buffer(bgl.GL_FLOAT, len(texcoord), texcoord)

	float positions[8] = { x, y, x + width, y, x + width, y + height, x, y + height };
	float texCoords[8] = { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };

	//	vertex_buffer = bgl.Buffer(bgl.GL_INT, 2)
	//	bgl.glGenBuffers(2, vertex_buffer)
	GLuint vertex_buffer[2];
	glGenBuffers(2, vertex_buffer);
	//	bgl.glBindBuffer(bgl.GL_ARRAY_BUFFER, vertex_buffer[0])
	//	bgl.glBufferData(bgl.GL_ARRAY_BUFFER, 32, position, bgl.GL_STATIC_DRAW)

	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer[0]);
	glBufferData(GL_ARRAY_BUFFER, 32, positions, GL_STATIC_DRAW);

	//	bgl.glBindBuffer(bgl.GL_ARRAY_BUFFER, vertex_buffer[1])
	//	bgl.glBufferData(bgl.GL_ARRAY_BUFFER, 32, texcoord, bgl.GL_STATIC_DRAW)

	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer[1]);
	glBufferData(GL_ARRAY_BUFFER, 32, texCoords, GL_STATIC_DRAW);

	//	bgl.glBindBuffer(bgl.GL_ARRAY_BUFFER, 0)
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	//	# DRAWING
	//	bgl.glActiveTexture(bgl.GL_TEXTURE0)
	glActiveTexture(GL_TEXTURE0);
	//	bgl.glBindTexture(bgl.GL_TEXTURE_2D, texture_id)	
	glBindTexture(GL_TEXTURE_2D, g_textureId);

	//	bgl.glBindVertexArray(vertex_array[0])
	glBindVertexArray(vertexArray[0]);

	//	bgl.glEnableVertexAttribArray(texturecoord_location)
	glEnableVertexAttribArray(textureCoordLocation);

	//	bgl.glEnableVertexAttribArray(position_location)
	glEnableVertexAttribArray(positionLocation);

	//	bgl.glBindBuffer(bgl.GL_ARRAY_BUFFER, vertex_buffer[0])
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer[0]);
	//	bgl.glVertexAttribPointer(
	//		position_location, 2, bgl.GL_FLOAT, bgl.GL_FALSE, 0, None)
	glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

	//	bgl.glBindBuffer(bgl.GL_ARRAY_BUFFER, vertex_buffer[1])
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer[1]);
	//	bgl.glVertexAttribPointer(
	//		texturecoord_location, 2, bgl.GL_FLOAT, bgl.GL_FALSE, 0, None)
	glVertexAttribPointer(textureCoordLocation, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

	//	bgl.glBindBuffer(bgl.GL_ARRAY_BUFFER, 0)
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	//	bgl.glDrawArrays(bgl.GL_TRIANGLE_FAN, 0, 4)
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	//	bgl.glBindVertexArray(0)
	glBindVertexArray(0);
	//	bgl.glBindTexture(bgl.GL_TEXTURE_2D, 0)		
	glBindTexture(GL_TEXTURE_2D, 0);

	glDisableVertexAttribArray(positionLocation);
	glDisableVertexAttribArray(textureCoordLocation);

	//	# DELETING
	//	bgl.glDeleteBuffers(2, vertex_buffer)
	//	bgl.glDeleteVertexArrays(1, vertex_array)

	glDeleteBuffers(2, vertex_buffer);
	glDeleteVertexArrays(1, vertexArray);
}
#else
void draw_texture()
{
	cuda_set_device();
	cuda_assert(cudaGLMapBufferObject((void**)&g_pixels_buf_d, g_bufferId));
	cuda_assert(cudaMemcpy(g_pixels_buf_d, g_pixels_buf_recv_d, (size_t)g_width * g_height * 4 * PIX_SIZE,
		cudaMemcpyDeviceToDevice));
	cuda_assert(cudaGLUnmapBufferObject(g_bufferId));

	//download texture from pbo
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, g_bufferId);
	glBindTexture(GL_TEXTURE_2D, g_textureId);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g_width, g_height, GL_RGBA, 
#ifdef TCP_FLOAT
		GL_FLOAT,
#else
		GL_UNSIGNED_BYTE,
#endif
		NULL);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_textureId);
	//glBindBuffer(GL_PIXEL_UNPACK_BUFFER, g_bufferId);
	////glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	//return;

	////glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	//// bind the texture and PBO
	//glBindTexture(GL_TEXTURE_2D, g_textureId);
	//glBindBuffer(GL_PIXEL_UNPACK_BUFFER, g_bufferId);

	//// copy pixels from PBO to texture object
	//// use offset instead of pointer.
	//glTexSubImage2D(GL_TEXTURE_2D,
	//	0,
	//	0,
	//	0,
	//	g_width,
	//	g_height,
	//	GL_RGBA,
	//	GL_UNSIGNED_BYTE,
	//	0);

	//glBindBuffer(GL_ARRAY_BUFFER, 0);
	//glBindTexture(GL_TEXTURE_2D, 0);
}
#endif
//////////////////////////

void resize(int width, int height)
{
	if (width == g_width && height == g_height && g_pixels_buf)
		return;

	cuda_set_device();

	if (g_pixels_buf)
	{		
		free_texture();
		cuda_assert(cudaFreeHost(g_pixels_buf));
	}

	g_width = width;
	g_height = height;

	cuda_assert(cudaHostAlloc((void**)&g_pixels_buf, (size_t)width * height * sizeof(char) * 4, cudaHostAllocMapped));

	int* size = (int*)&g_renderengine_data.width;
	size[0] = width;
	size[1] = height;

	setup_texture();
}

int recv_pixels_data()
{  
	cuda_set_device();

#ifdef WITH_CLIENT_GPUJPEG
	recv_gpujpeg(
		(char*)g_pixels_buf_recv_d, (char*)g_pixels_buf, g_width, g_height);
#else
	recv_data_data((char*)g_pixels_buf,
		g_width * g_height * sizeof(char) * 4 /*, false*/);

	cuda_assert(cudaMemcpy(g_pixels_buf_d,
		g_pixels_buf,
		g_width * g_height * sizeof(char) * 4,
		cudaMemcpyHostToDevice));  // cudaMemcpyDefault gpuMemcpyHostToDevice

	//current_samples = ((int*)g_pixels_buf)[0];
#endif

	recv_data_data((char*)&g_hs_data_state, sizeof(HsDataState));	

	displayFPS(1, get_current_samples());

	return 0;
}

int send_cam_data()
{
	send_data_cam((char*)&g_renderengine_data, sizeof(renderengine_data));

	return 0;
}

void reset()
{
	renderengine_data rd;
	rd.reset = 1;

	send_data_cam((char*)&rd, sizeof(renderengine_data));
}

void send_haystack_data_render(void* colorMap, int colorMapSize, void* domain, void* baseDensity)
{
	//struct HsDataRender {
	//	vec4f colorMap[128];
	//	float domain[2];
	//	float baseDensity;
	//};
	HsDataRender hsDataRender;

	memcpy(hsDataRender.colorMap, (char*)colorMap, colorMapSize * sizeof(float) * 4);
	memcpy(hsDataRender.domain, (char*)domain, sizeof(float) * 2);
	hsDataRender.baseDensity = ((float*)baseDensity)[0];

	send_data_cam((char*)&hsDataRender, sizeof(HsDataRender));
}

//void haystack_init(const char* server,
//	int port_cam,
//	int port_data)
//{
//	init_sockets_cam(server, port_cam, port_data);
//}

void get_haystack_range(
	void* world_bounds_spatial_lower,
	void* world_bounds_spatial_upper,
	void* scalars_range)
{
	memcpy((char*)world_bounds_spatial_lower, g_hs_data_state.world_bounds_spatial_lower, sizeof(float) * 3);
	memcpy((char*)world_bounds_spatial_upper, g_hs_data_state.world_bounds_spatial_upper, sizeof(float) * 3);
	memcpy((char*)scalars_range, g_hs_data_state.scalars_range, sizeof(float) * 2);
}

void set_timestep(int timestep)
{
	set_port_offset(timestep);
}

void client_init(const char* server,
	int port_cam,
	int port_data,
	int w,
	int h)
{
	//init_sockets_cam(server, port_cam, port_data);
	//setenv("SOCKET_SERVER_NAME_CAM", server, 1);
	//setenv("SOCKET_SERVER_NAME_DATA", server, 1);

	//char stemp[128];
	//sprintf(stemp, "%d", port_cam);
	//setenv("SOCKET_SERVER_PORT_CAM", stemp, 1);
	//sprintf(stemp, "%d", port_data);
	//setenv("SOCKET_SERVER_PORT_DATA", stemp, 1);

	//g_renderengine_data.step_samples = step_samples;
	//strcpy(g_renderengine_data.filename, filename);

	init_sockets_cam(server, port_cam, port_data);
	//gladLoadGL();

	memset(&g_hs_data_state, 0, sizeof(HsDataState));

	resize(w, h);
}

void client_close_connection()
{
	client_close();
	server_close();
}

void set_camera(void* view_martix,
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
		(char*)g_renderengine_data.cam.transform_inverse_view_matrix, view_martix, sizeof(float) * 12);

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

//int get_samples()
//{
//	int* samples = (int*)&g_renderengine_data.step_samples;
//	return samples[0];
//}

int get_current_samples()
{
	return g_hs_data_state.samples;
}

float get_remote_fps()
{
	return g_hs_data_state.fps;
}

float get_local_fps()
{
	return local_fps;
}

void get_pixels(void* pixels)
{
	size_t pix_type_size = sizeof(char) * 4; // sizeof(char) * 4;
	memcpy(pixels, (char*)g_pixels_buf, g_width * g_height * pix_type_size);
}

int get_texture_id()
{
	return g_textureId;
}
