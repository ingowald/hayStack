
# ======================================================================== //
# Copyright 2022-2023 Ingo Wald                                            //
# Copyright 2022-2023 IT4Innovations, VSB - Technical University of Ostrava//
#                                                                          //
# Licensed under the Apache License, Version 2.0 (the "License");          //
# you may not use this file except in compliance with the License.         //
# You may obtain a copy of the License at                                  //
#                                                                          //
#     http://www.apache.org/licenses/LICENSE-2.0                           //
#                                                                          //
# Unless required by applicable law or agreed to in writing, software      //
# distributed under the License is distributed on an "AS IS" BASIS,        //
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
# See the License for the specific language governing permissions and      //
# limitations under the License.                                           //
# ======================================================================== //

import bpy

import ctypes
import os
import platform
from ctypes import Array, cdll, c_void_p, c_char, c_char_p, c_int, c_int32, c_uint32, c_float, c_bool, c_ulong, POINTER

#####################################################################################################################
# platform specific library loading
if platform.system() == 'Windows':
    _renderengine_dll_name = "hayStack-renderengine.dll"
elif platform.system() == 'Linux':
    _renderengine_dll_name = "libhayStack-renderengine.so"
elif platform.system() == 'Darwin':
    _renderengine_dll_name = "libhayStack-renderengine.dylib"
else:
    raise ValueError(
        "Libraries not available for this platform: " + platform.system())

#####################################################################################################################
try:
    # Load library
    _renderengine_dll_name = os.path.join(
        os.path.dirname(__file__), _renderengine_dll_name)
    _renderengine_dll = cdll.LoadLibrary(_renderengine_dll_name)

    #####################################################################################################################

    # HAYSTACK_EXPORT_DLL void HAYSTACK_EXPORT_STD resize(int width, int height);
    _renderengine_dll.resize.argtypes = [c_int32, c_int32]

    # HAYSTACK_EXPORT_DLL int HAYSTACK_EXPORT_STD recv_pixels_data();
    # HAYSTACK_EXPORT_DLL int HAYSTACK_EXPORT_STD send_cam_data();
    _renderengine_dll.recv_pixels_data.restype = c_int32
    _renderengine_dll.send_cam_data.restype = c_int32

    #HAYSTACK_EXPORT_DLL void HAYSTACK_EXPORT_STD set_timestep(int timestep);
    _renderengine_dll.set_timestep.argtypes = [c_int32]

    # HAYSTACK_EXPORT_DLL void HAYSTACK_EXPORT_STD client_init(const char *server, int port_cam, int port_data, int w, int h);
    _renderengine_dll.client_init.argtypes = [
        c_char_p, c_int32, c_int32, c_int32, c_int32]

    # HAYSTACK_EXPORT_DLL void HAYSTACK_EXPORT_STD client_close_connection();

    # HAYSTACK_EXPORT_DLL void HAYSTACK_EXPORT_STD set_camera(void* view_martix,
    # 	float lens,
    # 	float nearclip,
    # 	float farclip,
    # 	float sensor_width,
    # 	float sensor_height,
    # 	int sensor_fit,
    # 	float view_camera_zoom,
    # 	float view_camera_offset0,
    # 	float view_camera_offset1,
    # 	int use_view_camera,
    # 	float shift_x,
    # 	float shift_y);
    _renderengine_dll.set_camera.argtypes = [c_void_p, c_float, c_float, c_float,
                                            c_float, c_float, c_int, c_float, c_float, c_float, c_int, c_float, c_float]


    # HAYSTACK_EXPORT_DLL void HAYSTACK_EXPORT_STD get_pixels(void* pixels);
    #_renderengine_dll.get_pixels.argtypes = [c_void_p]
    #HAYSTACK_EXPORT_DLL void HAYSTACK_EXPORT_STD draw_texture(void* gpu_buffer);
    #_renderengine_dll.draw_texture.argtypes = [c_void_p]

    # HAYSTACK_EXPORT_DLL int HAYSTACK_EXPORT_STD get_samples();
    # HAYSTACK_EXPORT_DLL int HAYSTACK_EXPORT_STD get_current_samples();
    #_renderengine_dll.get_samples.restype = c_int32
    _renderengine_dll.get_current_samples.restype = c_int32
    _renderengine_dll.get_remote_fps.restype = c_float
    _renderengine_dll.get_local_fps.restype = c_float

    # HAYSTACK_EXPORT_DLL void HAYSTACK_EXPORT_STD reset();
    # HAYSTACK_EXPORT_DLL void HAYSTACK_EXPORT_STD send_haystack_data_render(void* colorMap, int colorMapSize, void* domain, void* baseDensity);
    _renderengine_dll.send_haystack_data_render.argtypes = [c_void_p, c_int32, c_void_p, c_void_p]

    #HAYSTACK_EXPORT_DLL void HAYSTACK_EXPORT_STD rcv_haystack_data_init(const char *server, int port_cam, int port_data, 
    #   void* world_bounds_spatial_lower, void* world_bounds_spatial_upper, void* scalars_range);
    #_renderengine_dll.rcv_haystack_data_init.argtypes = [c_char_p, c_int32, c_int32, c_void_p, c_void_p, c_void_p]
   	# HAYSTACK_EXPORT_DLL void HAYSTACK_EXPORT_STD  get_haystack_range(
	# void* world_bounds_spatial_lower,
	# void* world_bounds_spatial_upper,
	# void* scalars_range);
    _renderengine_dll.get_haystack_range.argtypes = [c_void_p, c_void_p, c_void_p]

    # HAYSTACK_EXPORT_DLL int HAYSTACK_EXPORT_STD get_texture_id();
    _renderengine_dll.get_texture_id.restype = c_int32


except:
    print("Missing: ", _renderengine_dll_name)


def register():
    pass

def unregister():
    pass