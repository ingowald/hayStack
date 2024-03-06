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

#ifndef __RENDERENGINE_TCP_H__
#define __RENDERENGINE_TCP_H__

#include <stdlib.h>

//#define TCP_OPTIMIZATION
//#define TCP_FLOAT

#ifdef __cplusplus
extern "C" {
#endif

	void write_data_kernelglobal(void* data, size_t size);
	bool read_data_kernelglobal(void* data, size_t size);
	void close_kernelglobal();

	bool is_error();

	void init_sockets_cam(const char* server = NULL, int port_cam = 0, int port_data = 0);
	void init_sockets_data(const char* server = NULL, int port = 0);

	bool client_check();
	bool server_check();

	void client_close();
	void server_close();

	void send_data_cam(char* data, size_t size, bool ack = true);
	void recv_data_cam(char* data, size_t size, bool ack = true);

	void send_data_data(char* data, size_t size, bool ack = true);
	void recv_data_data(char* data, size_t size, bool ack = true);

	void send_gpujpeg(char* dmem, char* pixels, int width, int height);
	void recv_gpujpeg(char* dmem, char* pixels, int width, int height);
	void recv_decode(char* dmem, char* pixels, int width, int height, int frame_size);

	void rgb_to_yuv_i420(
		unsigned char* destination, unsigned char* source, int tile_h, int tile_w);

	void yuv_i420_to_rgb(
		unsigned char* destination, unsigned char* source, int tile_h, int tile_w);

	void yuv_i420_to_rgb_half(
		unsigned short* destination, unsigned char* source, int tile_h, int tile_w);

	void rgb_to_half(
		unsigned short* destination, unsigned char* source, int tile_h, int tile_w);

	void set_port_offset(int offset);

#ifdef __cplusplus
}
#endif


#endif
