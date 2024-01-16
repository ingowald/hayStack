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

#include "renderengine_tcp.h"

#include <cassert>
#    include <cstdlib>
#    include <stdio.h>
#    include <string.h>
#    include <sys/types.h>

#    ifdef _WIN32

#      include <iostream>
#      include <winsock2.h>
#      include <ws2tcpip.h>

#      pragma comment(lib, "Ws2_32.lib")
#      pragma comment(lib, "Mswsock.lib")
#      pragma comment(lib, "AdvApi32.lib")

#    else
#      include <arpa/inet.h>
#      include <netdb.h>
#      include <netinet/in.h>
#      include <netinet/tcp.h>
#      include <sys/socket.h>
#      include <unistd.h>
#    endif


//#include <omp.h>

#ifdef WITH_CLIENT_GPUJPEG
#  include <libgpujpeg/gpujpeg_common.h>
#  include <libgpujpeg/gpujpeg_decoder.h>
#  include <libgpujpeg/gpujpeg_encoder.h>
#endif

// RGB
#  define TCP_WIN_SIZE_SEND (32L * 1024L * 1024L)
#  define TCP_WIN_SIZE_RECV (32L * 1024L * 1024L)

#  define TCP_BLK_SIZE (1L * 1024L * 1024L * 1024L
#  define TCP_MAX_SIZE (128L * 1024L * 1024L)


#ifdef _WIN32
#  define KERNEL_SOCKET_SEND(s, buf, len) send(s, buf, (int)len, 0)
#  define KERNEL_SOCKET_RECV(s, buf, len) recv(s, buf, (int)len, 0)
#  define KERNEL_SOCKET_SEND_IGNORE_RC(s, buf, len) send(s, buf, (int)len, 0)
#  define KERNEL_SOCKET_RECV_IGNORE_RC(s, buf, len) recv(s, buf, (int)len, 0)
#else
#  define KERNEL_SOCKET_SEND(s, buf, len) write(s, buf, len); 
#  define KERNEL_SOCKET_RECV(s, buf, len) read(s, buf, len); 
#  define KERNEL_SOCKET_SEND_IGNORE_RC(s, buf, len) { auto rc = write(s, buf, len); assert(rc == len); }
#  define KERNEL_SOCKET_RECV_IGNORE_RC(s, buf, len) { auto rc = read(s, buf, len); assert(rc == len); }
#endif

#define MAX_CONNECTIONS 100
int g_port_offset = -1;

int g_server_id_cam[MAX_CONNECTIONS];
int g_client_id_cam[MAX_CONNECTIONS];

int g_server_id_data[MAX_CONNECTIONS];
int g_client_id_data[MAX_CONNECTIONS];

int g_timeval_sec = 60;
int g_connection_error = 0;

sockaddr_in g_client_sockaddr_cam[MAX_CONNECTIONS];
sockaddr_in g_server_sockaddr_cam[MAX_CONNECTIONS];

sockaddr_in g_client_sockaddr_data[MAX_CONNECTIONS];
sockaddr_in g_server_sockaddr_data[MAX_CONNECTIONS];

int setsock_tcp_windowsize(int inSock, int inTCPWin, int inSend)
{
#  ifdef SO_SNDBUF
	int rc;
	int newTCPWin;

	// assert( inSock >= 0 );

	if (inTCPWin > 0) {

#    ifdef TCP_WINSHIFT

		/* UNICOS requires setting the winshift explicitly */
		if (inTCPWin > 65535) {
			int winShift = 0;
			int scaledWin = inTCPWin >> 16;
			while (scaledWin > 0) {
				scaledWin >>= 1;
				winShift++;
			}

			/* set TCP window shift */
			rc = setsockopt(inSock, IPPROTO_TCP, TCP_WINSHIFT, (char*)&winShift, sizeof(winShift));
			if (rc < 0) {
				return rc;
			}

			/* Note: you cannot verify TCP window shift, since it returns
			 * a structure and not the same integer we use to set it. (ugh) */
		}
#    endif /* TCP_WINSHIFT  */

#    ifdef TCP_RFC1323
		/* On AIX, RFC 1323 extensions can be set system-wide,
		 * using the 'no' network options command. But we can also set them
		 * per-socket, so let's try just in case. */
		if (inTCPWin > 65535) {
			/* enable RFC 1323 */
			int on = 1;
			rc = setsockopt(inSock, IPPROTO_TCP, TCP_RFC1323, (char*)&on, sizeof(on));
			if (rc < 0) {
				return rc;
			}
		}
#    endif /* TCP_RFC1323 */

		if (!inSend) {
			/* receive buffer -- set
			 * note: results are verified after connect() or listen(),
			 * since some OS's don't show the corrected value until then. */
			newTCPWin = inTCPWin;
			rc = setsockopt(inSock, SOL_SOCKET, SO_RCVBUF, (char*)&newTCPWin, sizeof(newTCPWin));
		}
		else {
			/* send buffer -- set
			 * note: results are verified after connect() or listen(),
			 * since some OS's don't show the corrected value until then. */
			newTCPWin = inTCPWin;
			rc = setsockopt(inSock, SOL_SOCKET, SO_SNDBUF, (char*)&newTCPWin, sizeof(newTCPWin));
		}
		if (rc < 0) {
			return rc;
		}
	}
#  endif /* SO_SNDBUF */

	return 0;
} /* end setsock_tcp_windowsize */

bool client_check()
{
	return (g_client_id_cam[g_port_offset] != -1 && g_client_id_data[g_port_offset] != -1);
	// check_socket(g_client_id_cam) || check_socket(g_client_id_data);
}

bool server_check()
{
	return (g_server_id_cam[g_port_offset] != -1 && g_server_id_data[g_port_offset] != -1);
	// check_socket(g_server_id_cam) || check_socket(g_server_id_data);
}

bool is_error()
{
	return g_connection_error != 0;
}

bool init_wsa();
bool init_wsa()
{
#  ifdef WIN32
	WSADATA wsaData;
	// Request Winsock version 2.2
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		WSACleanup();
		return false;
	}

#  endif
	return true;
}

void init_port();
void init_port()
{
	if (g_port_offset == -1) {
		for (int tid = 0; tid < MAX_CONNECTIONS; tid++) {
			g_server_id_cam[tid] = -1;
			g_client_id_cam[tid] = -1;
			g_server_id_data[tid] = -1;
			g_client_id_data[tid] = -1;
		}

		g_connection_error = 0;
		g_port_offset = 0;
	}
}

void close_wsa();
void close_wsa()
{
#  ifdef WIN32
	WSACleanup();
#  endif
}

bool server_create(int port,
	int& server_id,
	int& client_id,
	sockaddr_in& server_sock,
	sockaddr_in& client_sock,
	bool only_accept = false);

bool server_create(int port,
	int& server_id,
	int& client_id,
	sockaddr_in& server_sock,
	sockaddr_in& client_sock,
	bool only_accept)
{
	init_port();

	if (!only_accept) {	
		if (!init_wsa()) {
			return false;
		}

		int type = SOCK_STREAM;
		int protocol = IPPROTO_TCP;

//#  ifdef WITH_SOCKET_UDP
//		type = SOCK_DGRAM;
//		protocol = IPPROTO_UDP;
//#  endif

		server_id = socket(AF_INET, type, protocol);

		if (server_id == -1) {
			printf("server_id == -1\n");
			fflush(0);
			return false;
		}

#  if !defined(__MIC__) && !defined(WIN32)
		int enable = 1;
		setsockopt(server_id, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int));
#  endif

		// timeval tv;
		// tv.tv_sec = g_timeval_sec;
		// tv.tv_usec = 0;
		// if (setsockopt(server_id, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv)) < 0) {
		//  printf("setsockopt == -1\n");
		//  fflush(0);
		//  return false;
		//}

		// sockaddr_in sock_name;
		memset(&server_sock, 0, sizeof(server_sock));
		memset(&client_sock, 0, sizeof(client_sock));
		server_sock.sin_family = AF_INET;
		server_sock.sin_port = htons(port);
		server_sock.sin_addr.s_addr = INADDR_ANY;

		int err_bind = bind(server_id, (sockaddr*)&server_sock, sizeof(server_sock));
		if (err_bind == -1) {
			printf("err_bind == -1\n");
			fflush(0);
			return false;
		}

//#  ifdef WITH_SOCKET_UDP
//		client_id = server_id;
//#  else

		int err_listen = listen(server_id, 1);
		if (err_listen == -1) {
			printf("err_listen == -1\n");
			fflush(0);
			return false;
		}
//#    if defined(WITH_SOCKET_ONLY_DATA)
//		return true;
//#    endif
	}

	sockaddr_in client_info;
	socklen_t addr_len = sizeof(client_info);

	printf("listen on %d\n", port);

	client_id = accept(server_id, (sockaddr*)&client_info, &addr_len);
	if (client_id == -1) {
		printf("client_id == -1\n");
		fflush(0);
		return false;
	}
//#  endif

	// printf("accept\n");
	printf("accept on %d <-> %d\n", port, client_info.sin_port);

	fflush(0);

	g_connection_error = 0;

	return true;
}

bool client_create(const char* server_name, int port, int& client_id, sockaddr_in& client_sock)
{
	// printf("connect to %s:%d\n", server_name, port);
	init_port();

	if (!init_wsa()) {
		return false;
	}

	hostent* host = gethostbyname(server_name);
	if (host == NULL) {
		printf("host == NULL\n");
		fflush(0);
		return false;
	}

	int type = SOCK_STREAM;
	int protocol = IPPROTO_TCP;

//#  ifdef WITH_SOCKET_UDP
//	type = SOCK_DGRAM;
//	protocol = IPPROTO_UDP;
//#  endif

	client_id = socket(AF_INET, type, protocol);
	if (client_id == -1) {
		printf("client_id == -1\n");
		fflush(0);
		return false;
	}

	// timeval tv;
	// tv.tv_sec = g_timeval_sec;
	// tv.tv_usec = 0;
	// if (setsockopt(6client_id, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv)) < 0) {
	//  printf("setsockopt == -1\n");
	//  fflush(0);
	//  return false;
	//}
	// netsh int tcp set global autotuninglevel=normal

#  ifdef TCP_OPTIMIZATION
#    ifdef _WIN32
#      define SIO_TCP_SET_ACK_FREQUENCY _WSAIOW(IOC_VENDOR, 23)
	int freq = 1;
	unsigned long bytes = 0;
	int result = WSAIoctl(
		client_id, SIO_TCP_SET_ACK_FREQUENCY, &freq, sizeof(freq), NULL, 0, &bytes, NULL, NULL);
	int i = 1;
	setsockopt(client_id, IPPROTO_TCP, TCP_NODELAY, (char*)&i, sizeof(i));
#    else
	int i = 1;
	setsockopt(client_id, IPPROTO_TCP, TCP_NODELAY, (char*)&i, sizeof(i));
	i = 1;
	//setsockopt(client_id, IPPROTO_TCP, TCP_QUICKACK, (char*)&i, sizeof(i));
#    endif

#    if 1 
	setsock_tcp_windowsize(client_id, TCP_WIN_SIZE_SEND, 1);
	setsock_tcp_windowsize(client_id, TCP_WIN_SIZE_RECV, 0);
#    endif
#  endif

	// sockaddr_in client_sock;
	memset(&client_sock, 0, sizeof(client_sock));
	client_sock.sin_family = AF_INET;
	client_sock.sin_port = htons(port);
	memcpy(&(client_sock.sin_addr), host->h_addr, host->h_length);

//#  ifndef WITH_SOCKET_UDP

	while (true) {
#    ifdef _WIN32
		Sleep(2);
#    else
		usleep(2000000);
#    endif

		int err_connect = connect(client_id, (sockaddr*)&client_sock, sizeof(client_sock));
		if (client_id == -1) {
			printf("disconnect\n");
			return false;
		}

		if (err_connect == -1) {
			// printf("wait on server %s:%d\n", server_name, port);

			//#      ifdef _WIN32
			//      Sleep(2);
			//#      else
			//      usleep(2000000);
			//#      endif
			continue;
		}
		break;
	}
//#  endif

	// printf("connect\n");
	printf("connect to %s:%d\n", server_name, port);
	fflush(0);

	g_connection_error = 0;

	return true;
}

void close_tcp(int id);
void close_tcp(int id)
{
#  ifdef WIN32
	closesocket(id);
#  else
	close(id);
#  endif
}

void client_close()
{
//#  if 0  // ndef _WIN32
//#    pragma omp parallel for num_threads(SOCKET_CONNECTIONS)
//#  endif
	for (int tid = 0; tid < MAX_CONNECTIONS; tid++) {
		// int tid = omp_get_thread_num();
		close_tcp(g_client_id_cam[tid]);
		close_tcp(g_client_id_data[tid]);

		//g_server_id_cam[tid] = -1;
		g_client_id_cam[tid] = -1;

		//g_server_id_data[tid] = -1;
		g_client_id_data[tid] = -1;
	}

	g_connection_error = 0;
	g_port_offset = -1;
}

void server_close()
{
//#  if 0  // ndef _WIN32
//#    pragma omp parallel for num_threads(SOCKET_CONNECTIONS)
//#  endif
	for (int tid = 0; tid < MAX_CONNECTIONS; tid++) {
		// int tid = omp_get_thread_num();
		close_tcp(g_server_id_cam[tid]);
		close_tcp(g_server_id_data[tid]);

		g_server_id_cam[tid] = -1;
		g_server_id_data[tid] = -1;
	}

	g_connection_error = 0;
	g_port_offset = -1;

	//close_wsa();
}

void init_sockets_cam(const char* server, int port_cam, int port_data)
{
	init_port();

	if (g_client_id_cam[g_port_offset] == -1) {
		init_wsa();
#  if defined(BLENDER_CLIENT)

		const char* env_p_port_cam = std::getenv("SOCKET_SERVER_PORT_CAM");
		if (port_cam == 0) {
			port_cam = (env_p_port_cam) ? atoi(env_p_port_cam) : 7000;
		}

//#    if 0  // ndef _WIN32
//#      pragma omp parallel for num_threads(SOCKET_CONNECTIONS)
//#    endif
		//for (int tid = 0; tid < SOCKET_CONNECTIONS; tid++) {
			// int tid = omp_get_thread_num();
			server_create(port_cam + g_port_offset,
				g_server_id_cam[g_port_offset],
				g_client_id_cam[g_port_offset],
				g_server_sockaddr_cam[g_port_offset],
				g_client_sockaddr_cam[g_port_offset]);
		//}

//#    ifdef WITH_SOCKET_UDP
//		char ack = -1;
//		recv_data_cam(&ack, sizeof(ack), false);
//#    endif

		init_sockets_data(server, port_data);

#  else

		const char* env_p_port_cam = std::getenv("SOCKET_SERVER_PORT_CAM");
		if (port_cam == 0) {
			// port_cam = atoi(env_p_port_cam);
			port_cam = (env_p_port_cam) ? atoi(env_p_port_cam) : 7000;
		}

		const char* env_p_name_cam = std::getenv("SOCKET_SERVER_NAME_CAM");
		char server_temp[1024];
		strcpy(server_temp, "localhost");

		if (env_p_name_cam != NULL) {
			strcpy(server_temp, env_p_name_cam);
		}

		if (server != NULL) {
			strcpy(server_temp, server);
		}

//#    if 0  // ndef _WIN32
//#      pragma omp parallel for num_threads(SOCKET_CONNECTIONS)
//#    endif
		//for (int tid = 0; tid < SOCKET_CONNECTIONS; tid++) {
			// int tid = omp_get_thread_num();
			client_create(server_temp, port_cam + g_port_offset, g_client_id_cam[g_port_offset], g_client_sockaddr_cam[g_port_offset]);
		//}

#    ifndef WITH_CLIENT_RENDERENGINE_SENDER
		init_sockets_data(server, port_data);
#    endif

//#    ifdef WITH_SOCKET_UDP
//		char ack = -1;
//		send_data_cam(&ack, sizeof(ack), false);
//#    endif

#  endif
	}
}

void init_sockets_data(const char* server, int port)
{
	init_port();

	if (g_client_id_data[g_port_offset] == -1) {
		init_wsa();

#  if (!defined(WITH_SOCKET_ONLY_DATA) && !defined(BLENDER_CLIENT) && \
       !defined(WITH_CLIENT_MPI_VRCLIENT)) || \
      (defined(WITH_SOCKET_ONLY_DATA) && defined(BLENDER_CLIENT))

		const char* env_p_port_data = std::getenv("SOCKET_SERVER_PORT_DATA");
		if (port == 0) {
			// port = atoi(env_p_port_data);
			port = (env_p_port_data) ? atoi(env_p_port_data) : 7001;
		}

		const char* env_p_name_data = std::getenv("SOCKET_SERVER_NAME_DATA");
		char server_temp[1024];
		strcpy(server_temp, "localhost");

		if (env_p_name_data != NULL) {
			strcpy(server_temp, env_p_name_data);
		}

		if (server != NULL) {
			strcpy(server_temp, server);
		}

//#    ifdef WITH_SOCKET_ONLY_DATA
//		//#ifndef _WIN32
//		//#        pragma omp parallel for num_threads(SOCKET_CONNECTIONS)
//		//#endif
//		//for (int tid = 0; tid < SOCKET_CONNECTIONS; tid++) {
//			// int tid = i;//omp_get_thread_num();
//			//#        pragma omp critical
//			client_create(server_temp, port + g_port_offset, g_client_id_data[g_port_offset], g_client_sockaddr_data[g_port_offset]);
//		//}
//#    else
//#      if 0  // ndef _WIN32
//#        pragma omp parallel for num_threads(SOCKET_CONNECTIONS)
//#      endif
		//for (int tid = 0; tid < SOCKET_CONNECTIONS; tid++) {
			// int tid = omp_get_thread_num();
			client_create(server_temp, port + g_port_offset, g_client_id_data[g_port_offset], g_client_sockaddr_data[g_port_offset]);
		//}
//#    endif
		// char ack = -1;
		// send_data_data(&ack, sizeof(ack));

#  else

		const char* env_p_port_data = std::getenv("SOCKET_SERVER_PORT_DATA");
		if (port == 0) {
			// port = atoi(env_p_port_data);
			port = (env_p_port_data) ? atoi(env_p_port_data) : 7001;
		}

//#    if 0// defined(WITH_SOCKET_ONLY_DATA)
//		server_create(port,
//			g_server_id_data[0],
//			g_client_id_data[0],
//			g_server_sockaddr_data[0],
//			g_client_sockaddr_data[0],
//			false);
//
//		for (int tid = 1; tid < SOCKET_CONNECTIONS; tid++) {
//			g_server_id_data[tid] = g_server_id_data[0];
//			g_server_sockaddr_data[tid] = g_server_sockaddr_data[0];
//		}
//
//		//#        pragma omp parallel for num_threads(SOCKET_CONNECTIONS)
//		for (int i = 0; i < SOCKET_CONNECTIONS; i++) {
//			int tid = i;  // omp_get_thread_num();
//			//#pragma omp critical
//			server_create(port,
//				g_server_id_data[tid],
//				g_client_id_data[tid],
//				g_server_sockaddr_data[tid],
//				g_client_sockaddr_data[tid],
//				true);
//		}
//
//#    else
//#      if 0  // ndef _WIN32
//#        pragma omp parallel for num_threads(SOCKET_CONNECTIONS)
//#      endif
		//for (int tid = 0; tid < SOCKET_CONNECTIONS; tid++) {
			// int tid = omp_get_thread_num();
			server_create(port + g_port_offset,
				g_server_id_data[g_port_offset],
				g_client_id_data[g_port_offset],
				g_server_sockaddr_data[g_port_offset],
				g_client_sockaddr_data[g_port_offset]);
		//}
		// char ack = -1;
		// recv_data_data(&ack, sizeof(ack));
//#    endif
#  endif
	}
}

#  define DEBUG_PRINT(size) //printf("%s: %lld\n", __FUNCTION__, size);

void send_data_cam(char* data, size_t size, bool ack_enabled)
{
	DEBUG_PRINT(size);

	init_sockets_cam();

	if (is_error())
		return;

	size_t sended_size = 0;

	while (sended_size != size) {
		size_t size_to_send = size - sended_size;
		if (size_to_send > TCP_MAX_SIZE) {
			size_to_send = TCP_MAX_SIZE;
		}

		int temp = KERNEL_SOCKET_SEND(g_client_id_cam[0], (char*)data + sended_size, size_to_send);

		if (temp < 1) {
			g_connection_error = 1;
			break;
		}

		sended_size += temp;
	}

	if (ack_enabled) {
		char ack = 0;
		KERNEL_SOCKET_RECV_IGNORE_RC(g_client_id_cam[0], &ack, 1);
		if (ack != 0) {
			printf("error in send_data_cam\n");
			g_connection_error = 1;
		}
	}
}

void send_data_data(char* data, size_t size, bool ack_enabled)
{
	DEBUG_PRINT(size);

	init_sockets_data();

	if (is_error())
		return;

	size_t sended_size = 0;

	while (sended_size != size) {
		size_t size_to_send = size - sended_size;
		if (size_to_send > TCP_MAX_SIZE) {
			size_to_send = TCP_MAX_SIZE;
		}

		int temp = KERNEL_SOCKET_SEND(g_client_id_data[0], (char*)data + sended_size, size_to_send);

		if (temp < 1) {
			g_connection_error = 1;
			break;
		}

		sended_size += temp;
	}

	if (ack_enabled) {
		char ack = 0;
		KERNEL_SOCKET_RECV_IGNORE_RC(g_client_id_data[0], &ack, 1);
		if (ack != 0) {
			printf("error in g_client_id_data\n");
			g_connection_error = 1;
		}
	}
}

void recv_data_cam(char* data, size_t size, bool ack_enabled)
{
	DEBUG_PRINT(size);

	init_sockets_cam();

	if (is_error())
		return;

	size_t sended_size = 0;

	while (sended_size != size) {
		size_t size_to_send = size - sended_size;
		if (size_to_send > TCP_MAX_SIZE) {
			size_to_send = TCP_MAX_SIZE;
		}

		int temp = KERNEL_SOCKET_RECV(g_client_id_cam[0], (char*)data + sended_size, size_to_send);

		if (temp < 1) {
			g_connection_error = 1;
			break;
		}

		sended_size += temp;
	}

	if (ack_enabled) {
		char ack = 0;
		KERNEL_SOCKET_SEND_IGNORE_RC(g_client_id_cam[0], &ack, 1);
		if (ack != 0) {
			printf("error in g_client_id_cam\n");
			g_connection_error = 1;
		}
	}
}

void recv_data_data(char* data, size_t size, bool ack_enabled)
{
	DEBUG_PRINT(size);

	init_sockets_data();

	if (is_error())
		return;

	size_t sended_size = 0;

	while (sended_size != size) {
		size_t size_to_send = size - sended_size;
		if (size_to_send > TCP_MAX_SIZE) {
			size_to_send = TCP_MAX_SIZE;
		}

		int temp = KERNEL_SOCKET_RECV(g_client_id_data[0], (char*)data + sended_size, size_to_send);

		if (temp < 1) {
			g_connection_error = 1;
			break;
		}

		sended_size += temp;
	}

	if (ack_enabled) {
		char ack = 0;
		KERNEL_SOCKET_SEND_IGNORE_RC(g_client_id_data[0], &ack, 1);
		if (ack != 0) {
			printf("error in g_client_id_data\n");
			g_connection_error = 1;
		}
	}
}

// limit UDP 65,507 bytes

void send_data(char* data, size_t size)
{
	send_data_data(data, size);
}

void recv_data(char* data, size_t size)
{
	recv_data_data(data, size);
}

void close_kernelglobal()
{
	client_close();
}

void write_data_kernelglobal(void* data, size_t size)
{
	send_data_data((char*)data, size);
}

bool read_data_kernelglobal(void* data, size_t size)
{
	recv_data_cam((char*)data, size);

	return true;
}

void rgb_to_yuv_i420(unsigned char* destination, unsigned char* source, int tile_h, int tile_w)
{
	unsigned char* dst_y = destination;
	unsigned char* dst_u = destination + tile_w * tile_h;
	unsigned char* dst_v = destination + tile_w * tile_h + tile_w * tile_h / 4;

#pragma omp parallel for
	for (int y = 0; y < tile_h; y++) {
		for (int x = 0; x < tile_w; x++) {

			int index_src = x + y * tile_w;

			// if (x >= tile_w) {
			//    index_src += tile_h * tile_w;
			//}

			unsigned char r = source[index_src * 4 + 0];
			unsigned char g = source[index_src * 4 + 1];
			unsigned char b = source[index_src * 4 + 2];

			// Y
			int index_y = x + y * tile_w;
			dst_y[index_y] = ((66 * r + 129 * g + 25 * b) >> 8) + 16;

			// U
			if (x % 2 == 0 && y % 2 == 0) {
				int index_u = (x / 2) + (y / 2) * (tile_w / 2);
				dst_u[index_u] = ((-38 * r + -74 * g + 112 * b) >> 8) + 128;
			}

			// V
			if (x % 2 == 0 && y % 2 == 0) {
				int index_v = (x / 2) + (y / 2) * (tile_w / 2);
				dst_v[index_v] = ((112 * r + -94 * g + -18 * b) >> 8) + 128;
			}
		}
	}
}

#if 0  // def WITH_CLIENT_GPUJPEG
void rgb_to_half(
	unsigned short* destination, unsigned char* source, int tile_h, int tile_w)
{
#  pragma omp parallel for
	for (int y = 0; y < tile_h; y++) {
		for (int x = 0; x < tile_w; x++) {

			int index_src = x + y * tile_w;
			int index_dst = x + y * tile_w;

			float scale = 1.0f / 255.0f;
			unsigned short* h = &destination[index_dst * 4 + 0];
			unsigned char* f = &source[index_src * 3 + 0];

			for (int i = 0; i < 4; i++) {
				/* optimized float to half for pixels:
				 * assumes no negative, no nan, no inf, and sets denormal to 0 */
				union {
					unsigned int i;
					float f;
				} in;
				float fscale = ((i == 3) ? 255.0f : f[i]) * scale;
				in.f = (fscale > 0.0f) ? ((fscale < 65504.0f) ? fscale : 65504.0f) : 0.0f;
				int x = in.i;

				int absolute = x & 0x7FFFFFFF;
				int Z = absolute + 0xC8000000;
				int result = (absolute < 0x38800000) ? 0 : Z;
				int rshift = (result >> 13);

				h[i] = (rshift & 0x7FFF);
			}
		}
	}
}

#else
void rgb_to_half(unsigned short* destination, unsigned char* source, int tile_h, int tile_w)
{
#  pragma omp parallel for
	for (int y = 0; y < tile_h; y++) {
		for (int x = 0; x < tile_w; x++) {

			int index_src = x + y * tile_w;
			int index_dst = x + y * tile_w;

			float scale = 1.0f / 255.0f;
			unsigned short* h = &destination[index_dst * 4 + 0];
			unsigned char* f = &source[index_src * 4 + 0];

			for (int i = 0; i < 4; i++) {
				/* optimized float to half for pixels:
				 * assumes no negative, no nan, no inf, and sets denormal to 0 */
				union {
					unsigned int i;
					float f;
				} in;
				float fscale = f[i] * scale;
				in.f = (fscale > 0.0f) ? ((fscale < 65504.0f) ? fscale : 65504.0f) : 0.0f;
				int x = in.i;

				int absolute = x & 0x7FFFFFFF;
				int Z = absolute + 0xC8000000;
				int result = (absolute < 0x38800000) ? 0 : Z;
				int rshift = (result >> 13);

				h[i] = (rshift & 0x7FFF);
			}
		}
	}
}
#endif
void yuv_i420_to_rgb(unsigned char* destination, unsigned char* source, int tile_h, int tile_w)
{

	unsigned char* src_y = source;
	unsigned char* src_u = source + tile_w * tile_h;
	unsigned char* src_v = source + tile_w * tile_h + tile_w * tile_h / 4;

#pragma omp parallel for
	for (int y = 0; y < tile_h; y++) {
		for (int x = 0; x < tile_w; x++) {

			int index_dst = x + y * tile_w;

			// if (x >= tile_w) {
			//    index_dst += tile_h * tile_w;
			//}

			unsigned char* r = &destination[index_dst * 4 + 0];
			unsigned char* g = &destination[index_dst * 4 + 1];
			unsigned char* b = &destination[index_dst * 4 + 2];
			unsigned char* a = &destination[index_dst * 4 + 3];

			// Y
			int index_y = x + y * tile_w;
			// dst_y[index_y] = ((66 * r + 129 * g + 25 * b) >> 8) + 16;
			unsigned char Y = src_y[index_y];

			// U
			// if (x % 2 == 0 && y % 2 == 0) {
			int index_u = (x / 2) + (y / 2) * tile_w / 2;
			// dst_u[index_u] = ((-38 * r + -74 * g + 112 * b) >> 8) + 128;
			//}
			unsigned char U = src_u[index_u];

			// V
			// if (x % 2 == 0 && y % 2 == 0) {
			int index_v = (x / 2) + (y / 2) * tile_w / 2;
			// dst_v[index_v] = ((112 * r + -94 * g + -18 * b) >> 8) + 128;
			//}
			unsigned char V = src_v[index_v];

			unsigned char C = Y - 16;
			unsigned char D = U - 128;
			unsigned char E = V - 128;

			// R = clip((298 * C + 409 * E + 128) >> 8)
			//    G = clip((298 * C - 100 * D - 208 * E + 128) >> 8)
			//    B = clip((298 * C + 516 * D + 128) >> 8)

			*r = (298 * C + 409 * E) >> 8;
			*g = (298 * C - 100 * D - 208 * E) >> 8;
			*b = (298 * C + 516 * D) >> 8;
			*a = 255;
		}
	}
}

void yuv_i420_to_rgb_half(
	unsigned short* destination, unsigned char* source, int tile_h, int tile_w)
{

	unsigned char* src_y = source;
	unsigned char* src_u = source + tile_w * tile_h;
	unsigned char* src_v = source + tile_w * tile_h + tile_w * tile_h / 4;

#pragma omp parallel for
	for (int y = 0; y < tile_h; y++) {
		for (int x = 0; x < tile_w; x++) {

			int index_dst = x + y * tile_w;

			// if (x >= tile_w) {
			//    index_dst += tile_h * tile_w;
			//}

			// unsigned short* r = &destination[index_dst * 4 + 0];
			// unsigned short* g = &destination[index_dst * 4 + 1];
			// unsigned short* b = &destination[index_dst * 4 + 2];
			// unsigned short* a = &destination[index_dst * 4 + 3];

			// Y
			int index_y = x + y * tile_w;
			// dst_y[index_y] = ((66 * r + 129 * g + 25 * b) >> 8) + 16;
			unsigned char Y = src_y[index_y];

			// U
			// if (x % 2 == 0 && y % 2 == 0) {
			int index_u = (x / 2) + (y / 2) * (tile_w / 2);
			// dst_u[index_u] = ((-38 * r + -74 * g + 112 * b) >> 8) + 128;
			//}
			unsigned char U = src_u[index_u];

			// V
			// if (x % 2 == 0 && y % 2 == 0) {
			int index_v = (x / 2) + (y / 2) * (tile_w / 2);
			// dst_v[index_v] = ((112 * r + -94 * g + -18 * b) >> 8) + 128;
			//}
			unsigned char V = src_v[index_v];

			unsigned char C = Y - 16;
			unsigned char D = U - 128;
			unsigned char E = V - 128;

			// R = clip((298 * C + 409 * E + 128) >> 8)
			//    G = clip((298 * C - 100 * D - 208 * E + 128) >> 8)
			//    B = clip((298 * C + 516 * D + 128) >> 8)

			unsigned char rgba[4];
			rgba[0] = (298 * C + 409 * E) >> 8;
			rgba[1] = (298 * C - 100 * D - 208 * E) >> 8;
			rgba[2] = (298 * C + 516 * D) >> 8;
			rgba[3] = 255;

			//*r = (1.164383 * C + 1.596027 * E) * 65535;
			//*g = (1.164383 * C - (0.391762 * D) - (0.812968 * E)) * 65535;
			//*b = (1.164383 * C + 2.017232 * D) * 65535;
			//*a = 65535;

			float scale = 1.0f / 255.0f;
			unsigned short* h = &destination[index_dst * 4 + 0];
			unsigned char* f = &rgba[0];

			for (int i = 0; i < 4; i++) {
				/* optimized float to half for pixels:
				 * assumes no negative, no nan, no inf, and sets denormal to 0 */
				union {
					unsigned int i;
					float f;
				} in;
				float fscale = f[i] * scale;
				in.f = (fscale > 0.0f) ? ((fscale < 65504.0f) ? fscale : 65504.0f) : 0.0f;
				int x = in.i;

				int absolute = x & 0x7FFFFFFF;
				int Z = absolute + 0xC8000000;
				int result = (absolute < 0x38800000) ? 0 : Z;
				int rshift = (result >> 13);

				h[i] = (rshift & 0x7FFF);
			}
		}
	}
}

#ifdef WITH_CLIENT_GPUJPEG
gpujpeg_encoder* g_encoder = NULL;
uint8_t* g_image_compressed;

int g_compressed_quality = -1; //0-100

int gpujpeg_encode(int width,
	int height,
	uint8_t* input_image,
	uint8_t* image_compressed,
	int& image_compressed_size)
{
	// set default encode parametrs, after calling, parameters can be tuned (eg. quality)
	struct gpujpeg_parameters param;
	gpujpeg_set_default_parameters(&param);

	if (g_compressed_quality == -1) {
		g_compressed_quality = 75;
		const char* compressed_quality_env = getenv("GPUJPEG_QUALITY");
		if (compressed_quality_env != NULL) {
			g_compressed_quality = atoi(compressed_quality_env);
		}
		param.quality = g_compressed_quality;
	}

	// here we set image parameters
	struct gpujpeg_image_parameters param_image;
	gpujpeg_image_set_default_parameters(&param_image);
	param_image.width = width;
	param_image.height = height;
	param_image.comp_count = 3;
	param_image.color_space = GPUJPEG_YCBCR_BT709;     // GPUJPEG_RGB;
	param_image.pixel_format = GPUJPEG_444_U8_P0P1P2;  // GPUJPEG_420_U8_P0P1P2;

	// create encoder
	if (g_encoder == NULL) {
		if ((g_encoder = gpujpeg_encoder_create(0)) == NULL) {
			return 1;
		}
	}

	struct gpujpeg_encoder_input encoder_input;
	// gpujpeg_encoder_input_set_gpu_image(&encoder_input, input_image);
	gpujpeg_encoder_input_set_image(&encoder_input, input_image);

	// compress the image
	if (gpujpeg_encoder_encode(g_encoder,
		&param,
		&param_image,
		&encoder_input,
		&g_image_compressed,
		&image_compressed_size) != 0) {
		return 1;
	}

	return 0;
}

gpujpeg_decoder* g_decoder = NULL;
int gpujpeg_decode(int width,
	int height,
	uint8_t* input_image,
	uint8_t* image_compressed,
	int& image_compressed_size)
{
	// create decoder
	if (g_decoder == NULL) {
		if ((g_decoder = gpujpeg_decoder_create(0)) == NULL) {
			return 1;
		}
	}

#  ifdef WITH_VRCLIENT
	gpujpeg_decoder_set_output_format(g_decoder, GPUJPEG_RGB, GPUJPEG_444_U8_P012Z);
#  else
	gpujpeg_decoder_set_output_format(
		g_decoder, GPUJPEG_RGB, GPUJPEG_444_U16_P012O /* GPUJPEG_444_U8_P012Z*/);
#  endif
	// set decoder default output destination
	gpujpeg_decoder_output decoder_output;
	// gpujpeg_decoder_output_set_default(&decoder_output);
	// gpujpeg_decoder_output_set_custom(&decoder_output, input_image);
	gpujpeg_decoder_output_set_custom_cuda(&decoder_output, input_image);
	// decoder_output.data = input_image;
	// decoder_output.type = GPUJPEG_DECODER_OUTPUT_CUSTOM_BUFFER;

	// decompress the image
	uint8_t* image_decompressed = NULL;
	int image_decompressed_size = 0;
	if (gpujpeg_decoder_decode(
		g_decoder, image_compressed, image_compressed_size, &decoder_output) != 0) {
		return 1;
	}

	return 0;
}
#endif
void send_gpujpeg(char* dmem, char* pixels, int width, int height)
{
#ifdef WITH_CLIENT_GPUJPEG
	//double t0 = omp_get_wtime();
	int frame_size = 0;
	gpujpeg_encode(width, height, (uint8_t*)dmem, (uint8_t*)pixels, frame_size);
	//double t1 = omp_get_wtime();
	send_data_data((char*)&frame_size, sizeof(int), false);
	send_data_data((char*)g_image_compressed, frame_size);
	//double t2 = omp_get_wtime();
	// printf("send_gpujpeg: %f, %f\n", t1 - t0, t2 - t1);
#endif
}

void recv_gpujpeg(char* dmem, char* pixels, int width, int height)
{
#ifdef WITH_CLIENT_GPUJPEG
	int frame_size = 0;
	//double t0 = omp_get_wtime();
	recv_data_data((char*)&frame_size, sizeof(int), false);
	recv_data_data((char*)pixels, frame_size);
	//double t1 = omp_get_wtime();
	gpujpeg_decode(width, height, (uint8_t*)dmem, (uint8_t*)pixels, frame_size);
	//double t2 = omp_get_wtime();
	// printf("recv_gpujpeg: %f, %f\n", t1 - t0, t2 - t1);
#endif
}

void recv_decode(char* dmem, char* pixels, int width, int height, int frame_size)
{
#ifdef WITH_CLIENT_GPUJPEG
	gpujpeg_decode(width, height, (uint8_t*)dmem, (uint8_t*)pixels, frame_size);
#endif
}

void set_port_offset(int offset)
{
	g_port_offset = offset;
}
