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

#####################################################################################################################
# Based on https://github.com/GPUOpen-LibrariesAndSDKs/RadeonProRenderBlenderAddon.git
#####################################################################################################################

import bpy
import os
import platform
import time

import ctypes
import textwrap
import weakref
import threading

import numpy as np
import bgl
import math

import gpu
#from gpu_extras.presets import draw_texture_2d
#import bgl

import ctypes
from dataclasses import dataclass

from math import radians
from mathutils import Matrix

from . import haystack_pref
from . import haystack_dll
from . import haystack_nodes

#####################################################################################################################
# if platform.system() == 'Windows':
#     gl = ctypes.windll.opengl32
# elif platform.system() == 'Linux':
#     gl = cdll.LoadLibrary('libGL.so')
# elif platform.system() == 'Darwin':
#     gl = cdll.LoadLibrary(
#         '/System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/libGL.dylib')
# else:
#     raise ValueError(
#         "Libraries not available for this platform: " + platform.system())

# gl.glViewport.argtypes = [c_int32, c_int32, c_uint32, c_uint32]

#####################################################################################################################


# def check_gl_error():
#     error = bgl.glGetError()
#     if error != bgl.GL_NO_ERROR:
#         raise Exception(error)

class HayStackShowPopupErrorMessage(bpy.types.Operator):
    """Show Popup Error Message"""
    bl_idname = "haystack.error_message"
    bl_label = "Error Message"
    #bl_options = {'REGISTER', 'INTERNAL'}
    
    message: bpy.props.StringProperty(
        name="Message",
        description="The message to display",
        default='An error has occurred'
    ) # type: ignore

    def execute(self, context):
        self.report({'ERROR'}, self.message)
        return {'FINISHED'}
    
    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)
    
#####################################################################################################################    
# def node_tree_poll(self, object):
#     return isinstance(object, haystack_nodes.HayStackNodeTree)

# def node_poll(self, object):
#     return isinstance(object, haystack_nodes.HayStackRenderBaseNode)
class HayStackServerSettings(bpy.types.PropertyGroup):
    width: bpy.props.IntProperty(
        name="Width",
        default=0
    ) # type: ignore

    height: bpy.props.IntProperty(
        name="Height",
        default=0
    ) # type: ignore

    step_samples: bpy.props.IntProperty(
        name="Step Samples",
        min=0,
        max=100,
        default=1
    ) # type: ignore

    filename: bpy.props.StringProperty(
        name="Filename",
        default=""
    ) # type: ignore

    cam_rotation_X: bpy.props.FloatProperty(
        name="Cam Rotation X",
        default=0.0,
        subtype="ANGLE"
    ) # type: ignore

    timesteps: bpy.props.IntProperty(
        name="Time Steps",
        min=1,
        max=100,
        default=1
    ) # type: ignore

    save_to_file: bpy.props.BoolProperty(
        name="Save To File",
        default=False
    ) # type: ignore      

    mat_volume: bpy.props.PointerProperty(
        type=bpy.types.Material
    ) # type: ignore

    # node_tree: bpy.props.PointerProperty(
    #     type=bpy.types.NodeTree,
    #     poll=node_tree_poll,
    # ) # type: ignore    

    # node: bpy.props.PointerProperty(
    #     type=bpy.types.Node,
    #     poll=node_poll,
    # ) # type: ignore   

    haystack_command: bpy.props.PointerProperty(
        type=bpy.types.Text,
    ) # type: ignore

    job_command: bpy.props.PointerProperty(
        type=bpy.types.Text,
    ) # type: ignore           

class HayStackRenderSettings(bpy.types.PropertyGroup):
    server_settings: bpy.props.PointerProperty(
        type=HayStackServerSettings
        ) # type: ignore


class HayStackData:
    def __init__(self):
        self.haystack_context = None
        self.haystack_process = None
        self.haystack_tunnel = None

#####################################################################################################################


# class GLTexture:
#     channels = 4

#     def __init__(self, width, height):
#         self.width = width
#         self.height = height

#         textures = bgl.Buffer(bgl.GL_INT, [1,])
#         bgl.glGenTextures(1, textures)
#         self.texture_id = textures[0]

#         bgl.glBindTexture(bgl.GL_TEXTURE_2D, self.texture_id)
#         bgl.glTexParameteri(bgl.GL_TEXTURE_2D,
#                             bgl.GL_TEXTURE_MIN_FILTER, bgl.GL_LINEAR)
#         bgl.glTexParameteri(bgl.GL_TEXTURE_2D,
#                             bgl.GL_TEXTURE_MAG_FILTER, bgl.GL_LINEAR)
#         bgl.glTexParameteri(bgl.GL_TEXTURE_2D,
#                             bgl.GL_TEXTURE_WRAP_S, bgl.GL_REPEAT)
#         bgl.glTexParameteri(bgl.GL_TEXTURE_2D,
#                             bgl.GL_TEXTURE_WRAP_T, bgl.GL_REPEAT)

#         bgl.glTexImage2D(
#             bgl.GL_TEXTURE_2D, 0, bgl.GL_RGBA,
#             self.width, self.height, 0,
#             bgl.GL_RGBA, bgl.GL_UNSIGNED_BYTE,
#             bgl.Buffer(bgl.GL_BYTE, [self.width, self.height, self.channels])
#         )

#     def __del__(self):
#         textures = bgl.Buffer(bgl.GL_INT, [1, ], [self.texture_id, ])
#         bgl.glDeleteTextures(1, textures)

#     def set_image(self, im: np.array):
#         bgl.glBindTexture(bgl.GL_TEXTURE_2D, self.texture_id)
#         gl.glTexSubImage2D(
#             bgl.GL_TEXTURE_2D, 0,
#             0, 0, self.width, self.height,
#             bgl.GL_RGBA, bgl.GL_UNSIGNED_BYTE,
#             ctypes.c_void_p(im.ctypes.data)
#         )

#####################################################################################################################
# struct HsDataRender {
#     vec4f colorMap[128];
#     float domain[2];
#     float baseDensity;
# };
class HsDataRender:
    def __init__(self):
        self.colorMapCount = 1024
        self.colorMap = np.zeros((self.colorMapCount * 4), dtype=np.float32)
        self.domain = np.zeros((2), dtype=np.float32)
        self.baseDensity = np.zeros((1), dtype=np.float32)          

class HayStackContext:
    channels = 4
    send_cam_data_result = -1

    def __init__(self):
        self.server = None
        self.port_cam = None
        self.port_data = None
        self.width = None
        self.height = None
        self.step_samples = None
        self.filename = None

        #self.data = None
        # self.data_right = None        

    def init(self, context, server, port_cam, port_data, width, height, step_samples, filename):

        self.server = server
        self.port_cam = port_cam
        self.port_data = port_data
        self.width = width
        self.height = height
        self.step_samples = step_samples
        self.filename = filename

        #self.data = np.empty((height, width, self.channels), dtype=np.uint8)
        # self.data_right = np.empty((height, width, self.channels), dtype=np.uint8)

        print(self.server.encode(), self.port_cam, self.port_data, self.width,
              self.height, self.step_samples, self.filename.encode())

    def client_init(self):
        haystack_dll._renderengine_dll.client_init(self.server.encode(), self.port_cam, self.port_data,
                                      self.width, self.height) #, self.step_samples, self.filename.encode()

        self.g_width = self.width
        self.g_height = self.height

        #check_gl_error()

    def client_close_connection(self):
        haystack_dll._renderengine_dll.reset()
        haystack_dll._renderengine_dll.client_close_connection()      

    def render(self, restart=False, tile=None):
        # cam
        if bpy.context.scene.haystack.server_settings.timesteps > 1:
            haystack_dll._renderengine_dll.set_timestep(bpy.context.scene.frame_current % bpy.context.scene.haystack.server_settings.timesteps)

        haystack_dll._renderengine_dll.send_cam_data()

        # volume       
        haystack_data = HsDataRender()
        #haystack_data_size = int(129 * 4 * 4)

        mat = bpy.context.scene.haystack.server_settings.mat_volume

        if mat:
            node_color_ramp = None
            node_float_curve = None

            if 'Color Ramp' in mat.node_tree.nodes:
                node_color_ramp = mat.node_tree.nodes['Color Ramp']

            if 'Float Curve' in mat.node_tree.nodes:
                node_float_curve = mat.node_tree.nodes['Float Curve']
                curve_map = node_float_curve.mapping.curves[0]
            
            for v in range(haystack_data.colorMapCount):
                color_rgba = (1.0,0.0,0.0,1.0)
                density = 1.0

                if node_color_ramp:
                    color_rgba = node_color_ramp.color_ramp.evaluate(float(v) / float(haystack_data.colorMapCount))
                    density = color_rgba[3]
                
                if node_float_curve:
                    density = node_float_curve.mapping.evaluate(curve_map, float(v) / float(haystack_data.colorMapCount))

                haystack_data.colorMap[0 + v * 4] = color_rgba[0]
                haystack_data.colorMap[1 + v * 4] = color_rgba[1]
                haystack_data.colorMap[2 + v * 4] = color_rgba[2]
                haystack_data.colorMap[3 + v * 4] = density

            domain = [float(0.0), float(1.0)]
            if 'DomainX' in mat.node_tree.nodes:
                domain[0] = mat.node_tree.nodes['DomainX'].outputs[0].default_value
            if 'DomainY' in mat.node_tree.nodes:
                domain[1] = mat.node_tree.nodes['DomainY'].outputs[0].default_value                
            
            haystack_data.domain[0] = domain[0]
            haystack_data.domain[1] = domain[1]

            baseDensity = float(1.0)
            if 'Base Density' in mat.node_tree.nodes:
                baseDensity = mat.node_tree.nodes['Base Density'].outputs[0].default_value

            haystack_data.baseDensity[0] = baseDensity

            # if bpy.context.scene.haystack.server_settings.save_to_file == True:
            #     haystack_data[3 + 128 * 4] = 1
            # else:
            #     haystack_data[3 + 128 * 4] = 0
        
        haystack_dll._renderengine_dll.send_haystack_data_render(haystack_data.colorMap.ctypes.data, haystack_data.colorMapCount, haystack_data.domain.ctypes.data, haystack_data.baseDensity.ctypes.data)

        # image
        haystack_dll._renderengine_dll.recv_pixels_data()

        return 1

    def set_camera(self, camera_data):
        transformL = np.array(camera_data.transform, dtype=np.float32)

        haystack_dll._renderengine_dll.set_camera(transformL.ctypes.data,
                                     camera_data.focal_length,
                                     camera_data.clip_plane[0],
                                     camera_data.clip_plane[1],
                                     camera_data.sensor_size[0],
                                     camera_data.sensor_size[1],
                                     camera_data.sensor_fit,
                                     camera_data.view_camera_zoom,
                                     camera_data.view_camera_offset[0],
                                     camera_data.view_camera_offset[1],
                                     camera_data.use_view_camera,
                                     camera_data.shift_x,
                                     camera_data.shift_y)
        

    # def get_image(self):
    #     haystack_dll._renderengine_dll.get_pixels(ctypes.c_void_p(self.data.ctypes.data))
    #     return self.data
    
    def draw_texture(self):
        haystack_dll._renderengine_dll.draw_texture()

    def get_current_samples(self):
        return haystack_dll._renderengine_dll.get_current_samples()
    
    def get_texture_id(self):
        return haystack_dll._renderengine_dll.get_texture_id()

    def resize(self, width, height):
        self.width = width
        self.height = height
        haystack_dll._renderengine_dll.resize( width, height)

        #return hsDataInit

    # @staticmethod
    # def _draw_texture(texture_id, x, y, width, height):
    #     # INITIALIZATION

    #     # Getting shader program
    #     shader_program = bgl.Buffer(bgl.GL_INT, 1)
    #     bgl.glGetIntegerv(bgl.GL_CURRENT_PROGRAM, shader_program)

    #     # Generate vertex array
    #     vertex_array = bgl.Buffer(bgl.GL_INT, 1)
    #     bgl.glGenVertexArrays(1, vertex_array)

    #     texturecoord_location = bgl.glGetAttribLocation(
    #         shader_program[0], "texCoord")
    #     position_location = bgl.glGetAttribLocation(shader_program[0], "pos")

    #     # hsViewer
    #     # glTexCoord2f(0.f, 0.f);
    #     # glVertex3f(0.f, 0.f, 0.f);

    #     # glTexCoord2f(0.f, 1.f);
    #     # glVertex3f(0.f, (float)fbSize.y, 0.f);

    #     # glTexCoord2f(1.f, 1.f);
    #     # glVertex3f((float)fbSize.x, (float)fbSize.y, 0.f);

    #     # glTexCoord2f(1.f, 0.f);
    #     # glVertex3f((float)fbSize.x, 0.f, 0.f);

    #     # Generate geometry buffers for drawing textured quad
    #     position = [x, y, x + width, y, x + width, y + height, x, y + height]
    #     position = bgl.Buffer(bgl.GL_FLOAT, len(position), position)
    #     texcoord = [0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0]
    #     texcoord = bgl.Buffer(bgl.GL_FLOAT, len(texcoord), texcoord)
    #     # position = [x, y, x, y + height, x + width, y + height, x + width, y]
    #     # position = bgl.Buffer(bgl.GL_FLOAT, len(position), position)
    #     # texcoord = [0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 0.0]
    #     # texcoord = bgl.Buffer(bgl.GL_FLOAT, len(texcoord), texcoord)

    #     vertex_buffer = bgl.Buffer(bgl.GL_INT, 2)
    #     bgl.glGenBuffers(2, vertex_buffer)
    #     bgl.glBindBuffer(bgl.GL_ARRAY_BUFFER, vertex_buffer[0])
    #     bgl.glBufferData(bgl.GL_ARRAY_BUFFER, 32, position, bgl.GL_STATIC_DRAW)
    #     bgl.glBindBuffer(bgl.GL_ARRAY_BUFFER, vertex_buffer[1])
    #     bgl.glBufferData(bgl.GL_ARRAY_BUFFER, 32, texcoord, bgl.GL_STATIC_DRAW)
    #     bgl.glBindBuffer(bgl.GL_ARRAY_BUFFER, 0)

    #     # DRAWING
    #     bgl.glActiveTexture(bgl.GL_TEXTURE0)
    #     bgl.glBindTexture(bgl.GL_TEXTURE_2D, texture_id)

    #     bgl.glBindVertexArray(vertex_array[0])
    #     bgl.glEnableVertexAttribArray(texturecoord_location)
    #     bgl.glEnableVertexAttribArray(position_location)

    #     bgl.glBindBuffer(bgl.GL_ARRAY_BUFFER, vertex_buffer[0])
    #     bgl.glVertexAttribPointer(
    #         position_location, 2, bgl.GL_FLOAT, bgl.GL_FALSE, 0, None)
    #     bgl.glBindBuffer(bgl.GL_ARRAY_BUFFER, vertex_buffer[1])
    #     bgl.glVertexAttribPointer(
    #         texturecoord_location, 2, bgl.GL_FLOAT, bgl.GL_FALSE, 0, None)
    #     bgl.glBindBuffer(bgl.GL_ARRAY_BUFFER, 0)

    #     bgl.glDrawArrays(bgl.GL_TRIANGLE_FAN, 0, 4)

    #     bgl.glBindVertexArray(0)
    #     bgl.glBindTexture(bgl.GL_TEXTURE_2D, 0)

    #     # DELETING
    #     bgl.glDeleteBuffers(2, vertex_buffer)
    #     bgl.glDeleteVertexArrays(1, vertex_array)


#####################################################################################################################
MAX_ORTHO_DEPTH = 200.0


@dataclass(init=False, eq=True)
class CameraData:
    """ Comparable dataclass which holds all camera settings """

    transform: tuple = None  # void *camera_object,
    focal_length: float = None  # float lens,
    clip_plane: (float, float) = None  # float nearclip, float farclip,
    # float sensor_width, float sensor_height,
    sensor_size: (float, float) = None
    sensor_fit: int = None  # int sensor_fit,
    view_camera_zoom: float = None  # float view_camera_zoom,
    # float view_camera_offset0, float view_camera_offset1,
    view_camera_offset: (float, float) = None
    use_view_camera: int = None  # int use_view_camera
    shift_x: float = None
    shift_y: float = None
    quat: tuple = None
    pos: tuple = None

    @staticmethod
    def get_view_matrix(context):
        R = Matrix.Rotation(context.scene.haystack.server_settings.cam_rotation_X, 4, 'X')
        vmat = R @ context.region_data.view_matrix.inverted()
        return vmat

    @staticmethod
    def init_from_camera(camera: bpy.types.Camera, transform, ratio, border=((0, 0), (1, 1))):
        """ Returns CameraData from bpy.types.Camera """

        # pos, size = border

        data = CameraData()
        data.clip_plane = (camera.clip_start, camera.clip_end)
        data.transform = tuple(transform)
        data.use_view_camera = 1

        data.shift_x = camera.shift_x
        data.shift_y = camera.shift_y

        data.quat = tuple(transform.to_quaternion())
        data.pos = tuple(transform.to_translation())

        # projection_matrix = camera.calc_matrix_camera(
        #     bpy.context.view_layer.depsgraph,
        #     x = bpy.context.scene.render.resolution_x,
        #     y = bpy.context.scene.render.resolution_y,
        #     scale_x = bpy.context.scene.render.pixel_aspect_x,
        #     scale_y = bpy.context.scene.render.pixel_aspect_y,
        # )

        # if camera.dof.use_dof:
        #     # calculating focus_distance
        #     if not camera.dof.focus_object:
        #         focus_distance = camera.dof.focus_distance
        #     else:
        #         obj_pos = camera.dof.focus_object.matrix_world.to_translation()
        #         camera_pos = transform.to_translation()
        #         focus_distance = (obj_pos - camera_pos).length

        #     data.dof_data = (max(focus_distance, 0.001),
        #                      camera.dof.aperture_fstop,
        #                      max(camera.dof.aperture_blades, 4))
        # else:
        #     data.dof_data = None

        if camera.sensor_fit == 'VERTICAL':
            # data.lens_shift = (camera.shift_x / ratio, camera.shift_y)
            data.sensor_fit = 2
        elif camera.sensor_fit == 'HORIZONTAL':
            # data.lens_shift = (camera.shift_x, camera.shift_y * ratio)
            data.sensor_fit = 1
        elif camera.sensor_fit == 'AUTO':
            # data.lens_shift = (camera.shift_x, camera.shift_y * ratio) if ratio > 1.0 else \
            #     (camera.shift_x / ratio, camera.shift_y)
            data.sensor_fit = 0
        else:
            raise ValueError("Incorrect camera.sensor_fit value",
                             camera, camera.sensor_fit)

        # data.lens_shift = tuple(data.lens_shift[i] / size[i] + (pos[i] + size[i] * 0.5 - 0.5) / size[i] for i in (0, 1))

        if camera.type == 'PERSP':
            # data.mode = pyrpr.CAMERA_MODE_PERSPECTIVE
            # data.focal_length = camera.lens
            # data.sensor_fit = camera.sensor_fit
            data.sensor_size = (camera.sensor_width, camera.sensor_height)
            # if camera.sensor_fit == 'VERTICAL':
            #     data.sensor_size = (camera.sensor_height * ratio, camera.sensor_height)
            # elif camera.sensor_fit == 'HORIZONTAL':
            #     data.sensor_size = (camera.sensor_width, camera.sensor_width / ratio)
            # else:
            #     data.sensor_size = (camera.sensor_width, camera.sensor_width / ratio) if ratio > 1.0 else \
            #                        (camera.sensor_width * ratio, camera.sensor_width)

            # data.sensor_size = tuple(data.sensor_size[i] * size[i] for i in (0, 1))

            # data.fov = 2.0 * math.atan(0.5 * camera.sensor_width / camera.lens / ratio )
            # data.focal_length = math.degrees(2.0 * math.atan(0.5 * camera.sensor_width / camera.lens / ratio ))
            # fov = 2.0 * math.atan(0.5 * camera.sensor_width / camera.lens)
            # data.focal_length = math.degrees(fov)

            fov = 2.0 * math.atan(0.5 * 72.0 / camera.lens / ratio)
            data.focal_length = math.degrees(fov)

        # elif camera.type == 'ORTHO':
        #     #data.mode = pyrpr.CAMERA_MODE_ORTHOGRAPHIC
        #     if camera.sensor_fit == 'VERTICAL':
        #         data.ortho_size = (camera.ortho_scale * ratio, camera.ortho_scale)
        #     elif camera.sensor_fit == 'HORIZONTAL':
        #         data.ortho_size = (camera.ortho_scale, camera.ortho_scale / ratio)
        #     else:
        #         data.ortho_size = (camera.ortho_scale, camera.ortho_scale / ratio) if ratio > 1.0 else \
        #                           (camera.ortho_scale * ratio, camera.ortho_scale)

        #     data.ortho_size = tuple(data.ortho_size[i] * size[i] for i in (0, 1))
        #     data.clip_plane = (camera.clip_start, min(camera.clip_end, MAX_ORTHO_DEPTH + camera.clip_start))

        # elif camera.type == 'PANO':
        #     # TODO: Recheck parameters for PANO camera
        #     #data.mode = pyrpr.CAMERA_MODE_LATITUDE_LONGITUDE_360
        #     data.focal_length = camera.lens
        #     if camera.sensor_fit == 'VERTICAL':
        #         data.sensor_size = (camera.sensor_height * ratio, camera.sensor_height)
        #     elif camera.sensor_fit == 'HORIZONTAL':
        #         data.sensor_size = (camera.sensor_width, camera.sensor_width / ratio)
        #     else:
        #         data.sensor_size = (camera.sensor_width, camera.sensor_width / ratio) if ratio > 1.0 else \
        #                            (camera.sensor_width * ratio, camera.sensor_width)

        #     data.sensor_size = tuple(data.sensor_size[i] * size[i] for i in (0, 1))

        else:
            raise ValueError("Incorrect camera.type value",
                             camera, camera.type)

        return data

    @staticmethod
    def init_from_context(context: bpy.types.Context):
        """ Returns CameraData from bpy.types.Context """

        # this constant was found experimentally, didn't find such option in
        VIEWPORT_SENSOR_SIZE = 72.0
        # context.space_data or context.region_data

        if context.region.width < context.region.height:
            min_wh = context.region.width
        else:
            min_wh = context.region.height

        if context.region.width > context.region.height:
            max_wh = context.region.width
        else:
            max_wh = context.region.height

        ratio = max_wh / min_wh
        if context.region_data.view_perspective == 'PERSP':
            data = CameraData()
            # data.mode = pyrpr.CAMERA_MODE_PERSPECTIVE
            data.clip_plane = (context.space_data.clip_start,
                               context.space_data.clip_end)
            # data.lens_shift = (0.0, 0.0)
            # data.sensor_size = (VIEWPORT_SENSOR_SIZE, VIEWPORT_SENSOR_SIZE / ratio) if ratio > 1.0 else \
            #                    (VIEWPORT_SENSOR_SIZE * ratio, VIEWPORT_SENSOR_SIZE)
            # data.fov = 2.0 * math.atan(0.5 * VIEWPORT_SENSOR_SIZE / context.space_data.lens / ratio

            fov = 2.0 * math.atan(0.5 * VIEWPORT_SENSOR_SIZE /
                                  context.space_data.lens / ratio)
            data.focal_length = math.degrees(fov)

            # context.region_data.view_matrix.inverted()
            vmat = CameraData.get_view_matrix(context)
            data.transform = tuple(vmat)
            # data.focal_length = context.space_data.lens
            data.use_view_camera = 0
            data.sensor_size = (VIEWPORT_SENSOR_SIZE, VIEWPORT_SENSOR_SIZE)
            data.view_camera_offset = (0, 0)
            data.view_camera_zoom = 1.0
            data.sensor_fit = 0
            data.shift_x = 0
            data.shift_y = 0

            data.quat = tuple(vmat.to_quaternion())
            data.pos = tuple(vmat.to_translation())

        # elif context.region_data.view_perspective == 'ORTHO':
        #     data = CameraData()
        #     #data.mode = pyrpr.CAMERA_MODE_ORTHOGRAPHIC
        #     #ortho_size = context.region_data.view_distance * VIEWPORT_SENSOR_SIZE / context.space_data.lens
        #     #data.lens_shift = (0.0, 0.0)
        #     ortho_depth = min(context.space_data.clip_end, MAX_ORTHO_DEPTH)
        #     data.clip_plane = (-ortho_depth * 0.5, ortho_depth * 0.5)
        #     #data.ortho_size = (ortho_size, ortho_size / ratio) if ratio > 1.0 else \
        #     #                  (ortho_size * ratio, ortho_size)

        #     data.transform = tuple(context.region_data.view_matrix.inverted())

        elif context.region_data.view_perspective == 'CAMERA':
            camera_obj = context.space_data.camera
            data = CameraData.init_from_camera(
                camera_obj.data, CameraData.get_view_matrix(context), ratio)
            # data = CameraData()
            # data.clip_plane = (camera_obj.data.clip_start, camera_obj.data.clip_end)
            # data.transform = tuple(context.region_data.view_matrix.inverted())
            # data.fov = 2.0 * atan(0.5 * camera_obj.data.sensor_width / camera_obj.data.lens / ratio )
            # data.transform = tuple(camera_obj.matrix_world) #tuple(context.region_data.view_matrix.inverted())

            # # This formula was taken from previous plugin with corresponded comment
            # # See blender/intern/cycles/blender/blender_camera.cpp:blender_camera_from_view (look for 1.41421f)
            # zoom = 4.0 / (2.0 ** 0.5 + context.region_data.view_camera_zoom / 50.0) ** 2
            data.view_camera_zoom = context.region_data.view_camera_zoom

            # # Updating lens_shift due to viewport zoom and view_camera_offset
            # # view_camera_offset should be multiplied by 2
            # data.lens_shift = ((data.lens_shift[0] + context.region_data.view_camera_offset[0] * 2) / zoom,
            #                    (data.lens_shift[1] + context.region_data.view_camera_offset[1] * 2) / zoom)
            data.view_camera_offset = (
                context.region_data.view_camera_offset[0], context.region_data.view_camera_offset[1])
            # if data.mode == pyrpr.CAMERA_MODE_ORTHOGRAPHIC:
            #     data.ortho_size = (data.ortho_size[0] * zoom, data.ortho_size[1] * zoom)
            # else:
            #     data.sensor_size = (data.sensor_size[0] * zoom, data.sensor_size[1] * zoom)

        else:
            raise ValueError("Incorrect view_perspective value",
                             context.region_data.view_perspective)

        return data

#####################################################################################################################


@dataclass(init=False, eq=True)
class ViewportSettings:
    """
    Comparable dataclass which holds render settings for ViewportEngine:
    - camera viewport settings
    - render resolution
    - screen resolution
    - render border
    """

    camera_data: CameraData
    # camera_dataR: CameraData
    width: int
    height: int
    screen_width: int
    screen_height: int
    border: tuple

    def __init__(self, context: bpy.types.Context):
        """Initializes settings from Blender's context"""
        # if haystack_dll._renderengine_dll.get_renderengine_type() == 1:
        #     self.camera_data,self.camera_dataR = CameraData.init_from_context_openvr(context)
        #     self.screen_height = context.scene.openvr_user_prop.openVrGlRenderer.getHeight()
        #     self.screen_width = context.scene.openvr_user_prop.openVrGlRenderer.getWidth()
        # else:
        self.camera_data = CameraData.init_from_context(context)
        self.screen_width, self.screen_height = context.region.width, context.region.height

        scene = context.scene

        # getting render border
        x1, y1 = 0, 0
        x2, y2 = self.screen_width, self.screen_height

        # getting render resolution and render border
        self.width, self.height = x2 - x1, y2 - y1
        self.border = (x1, y1), (self.width, self.height)

#####################################################################################################################


class Engine:
    """ This is the basic Engine class """

    def __init__(self, haystack_engine):
        self.haystack_engine = weakref.proxy(haystack_engine)
        self.haystack_context = HayStackContext()
        bpy.context.scene.haystack_data.haystack_context = self.haystack_context

#####################################################################################################################


class ViewportEngine(Engine):
    """ Viewport render engine """

    def __init__(self, haystack_engine):
        super().__init__(haystack_engine)

        #self.gl_texture = None #GLTexture = None
        self.viewport_settings: ViewportSettings = None

        self.sync_render_thread: threading.Thread = None
        self.restart_render_event = threading.Event()
        self.render_lock = threading.Lock()
        self.resolve_lock = threading.Lock()

        self.is_finished = True
        self.is_synced = False
        #self.is_rendered = False
        # self.is_denoised = False
        #self.is_resized = False

        # self.render_iterations = 0
        # self.render_time = 0

        # g_viewport_engine = self
        # self.render_callback = render_callback_type(self.render_callback)

    def start_render(self):
        print("start_render")
        self.is_finished = False

        #print('Start _do_sync_render')
        self.restart_render_event.clear()
        self.sync_render_thread = threading.Thread(target=self._do_sync_render)
        self.sync_render_thread.start()
        #print('Finish sync')   

    def stop_render(self):
        print("stop_render")
        self.is_finished = True

        self.restart_render_event.set()
        self.sync_render_thread.join()        

        #self.haystack_context.client_close_connection()

        #self.haystack_context = None
        #self.image_filter = None
        #pass

    def _do_sync_render(self):
        """
        Thread function for self.sync_render_thread. It always run during viewport render.
        If it doesn't render it waits for self.restart_render_event
        """

        def notify_status(info, status):
            """ Display export progress status """
            wrap_info = textwrap.fill(info, 120)
            self.haystack_engine.update_stats(status, wrap_info)
            # log(status, wrap_info)

            # requesting blender to call draw()
            self.haystack_engine.tag_redraw()

        class FinishRender(Exception):
            pass

        # print('Start _do_sync_render')
        # self.haystack_context.client_init()

        try:
            # SYNCING OBJECTS AND INSTANCES
            notify_status("Starting...", "Sync")
            time_begin = time.perf_counter()

            self.is_synced = True

            # RENDERING
            notify_status("Starting...", "Render")

            # Infinite cycle, which starts when scene has to be re-rendered.
            # It waits for restart_render_event be enabled.
            # Exit from this cycle is implemented through raising FinishRender
            # when self.is_finished be enabled from main thread.
            while True:
                self.restart_render_event.wait()

                if self.is_finished:
                    raise FinishRender

                # preparations to start rendering
                iteration = 0
                time_begin = 0.0
                # if is_adaptive:
                #     all_pixels = active_pixels = self.haystack_context.width * self.haystack_context.height
                # is_last_iteration = False

                # this cycle renders each iteration
                while True:
                    if self.is_finished:
                        raise FinishRender

                    if self.restart_render_event.is_set():
                        # clears restart_render_event, prepares to start rendering
                        self.restart_render_event.clear()
                        iteration = 0

                        # if self.is_resized:
                        #     # if not self.haystack_context.gl_interop:
                        #     # When gl_interop is not enabled, than resize is better to do in
                        #     # this thread. This is important for hybrid.
                        #     # with self.render_lock:
                        #     #     self.haystack_context.resize(self.viewport_settings.width,
                        #     #                                 self.viewport_settings.height)
                        #     self.is_resized = False

                        time_begin = time.perf_counter()

                    # rendering
                    # with self.render_lock:
                    #     if self.restart_render_event.is_set():
                    #         break

                    self.haystack_context.render(restart=(iteration == 0))

                    self.is_rendered = True
                    current_samples = self.haystack_context.get_current_samples()

                    time_render = time.perf_counter() - time_begin
                    fps = current_samples / time_render
                    info_str = f"Time: {time_render:.1f} sec"\
                            f" | Samples: {current_samples}" \
                            f" | FPS: {fps:.1f}"

                    notify_status(info_str, "Render")

        except FinishRender:
            #print("Finish by user")
            pass

        except Exception as e:
            print(e)
            
        self.is_finished = True

        # notifying viewport about error
        #notify_status(f"{e}.\nPlease see logs for more details.", "ERROR")

        #bpy.ops.haystack.stop_process()
        #print('Finish _do_sync_render')        

    def sync(self, context, depsgraph):
        
        if context.scene.haystack_data.haystack_process is None:
            message = "Haystack process is not started"
            bpy.ops.haystack.error_message('INVOKE_DEFAULT', message=message)
            raise Exception(message)
    
        print('Start sync')
        #bpy.ops.haystack.start_process()

        scene = depsgraph.scene
        view_layer = depsgraph.view_layer

        scene.view_settings.view_transform = 'Raw'

        # getting initial render resolution
        # if scene.haystack.server_settings.use_viewport == True:
        viewport_settings = ViewportSettings(context)
        width, height = viewport_settings.width, viewport_settings.height
        if width * height == 0:
            # if width, height == 0, 0, then we set it to 1, 1 to be able to set AOVs
            width, height = 1, 1

        scene.haystack.server_settings.width = width
        scene.haystack.server_settings.height = height

        pref = haystack_pref.preferences()

        # client_init(const char *server, int port_cam, int port_data, int w, int h, int step_samples)
        self.haystack_context.init(context, pref.haystack_server_name,
                                  pref.haystack_port_cam,
                                  pref.haystack_port_data,
                                  scene.haystack.server_settings.width,
                                  scene.haystack.server_settings.height,
                                  scene.haystack.server_settings.step_samples,
                                  scene.haystack.server_settings.filename)
        # if not self.haystack_context.gl_interop:
        #self.gl_texture = GLTexture(width, height)
        self.haystack_context.client_init()     
        #################################

        # reset scene
        # haystack_dll._renderengine_dll.reset()

        #import array
        #pixels = width * height * array.array('f', [0.1, 0.2, 0.1, 1.0])
        #pixels = gpu.types.Buffer('FLOAT', width * height * 4)

        # Generate texture
        #self.texture = gpu.types.GPUTexture((width, height), format='RGBA16F', data=pixels)

        self.start_render()

    # @staticmethod
    # def _draw_texture(texture_id, x, y, width, height):
    #     # INITIALIZATION

    #     # Getting shader program
    #     shader_program = bgl.Buffer(bgl.GL_INT, 1)
    #     bgl.glGetIntegerv(bgl.GL_CURRENT_PROGRAM, shader_program)

    #     # Generate vertex array
    #     vertex_array = bgl.Buffer(bgl.GL_INT, 1)
    #     bgl.glGenVertexArrays(1, vertex_array)

    #     texturecoord_location = bgl.glGetAttribLocation(
    #         shader_program[0], "texCoord")
    #     position_location = bgl.glGetAttribLocation(shader_program[0], "pos")

    #     # Generate geometry buffers for drawing textured quad
    #     position = [x, y, x + width, y, x + width, y + height, x, y + height]
    #     position = bgl.Buffer(bgl.GL_FLOAT, len(position), position)
    #     texcoord = [0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0]
    #     texcoord = bgl.Buffer(bgl.GL_FLOAT, len(texcoord), texcoord)

    #     vertex_buffer = bgl.Buffer(bgl.GL_INT, 2)
    #     bgl.glGenBuffers(2, vertex_buffer)
    #     bgl.glBindBuffer(bgl.GL_ARRAY_BUFFER, vertex_buffer[0])
    #     bgl.glBufferData(bgl.GL_ARRAY_BUFFER, 32, position, bgl.GL_STATIC_DRAW)
    #     bgl.glBindBuffer(bgl.GL_ARRAY_BUFFER, vertex_buffer[1])
    #     bgl.glBufferData(bgl.GL_ARRAY_BUFFER, 32, texcoord, bgl.GL_STATIC_DRAW)
    #     bgl.glBindBuffer(bgl.GL_ARRAY_BUFFER, 0)

    #     # DRAWING
    #     bgl.glActiveTexture(bgl.GL_TEXTURE0)
    #     bgl.glBindTexture(bgl.GL_TEXTURE_2D, texture_id)

    #     bgl.glBindVertexArray(vertex_array[0])
    #     bgl.glEnableVertexAttribArray(texturecoord_location)
    #     bgl.glEnableVertexAttribArray(position_location)

    #     bgl.glBindBuffer(bgl.GL_ARRAY_BUFFER, vertex_buffer[0])
    #     bgl.glVertexAttribPointer(
    #         position_location, 2, bgl.GL_FLOAT, bgl.GL_FALSE, 0, None)
    #     bgl.glBindBuffer(bgl.GL_ARRAY_BUFFER, vertex_buffer[1])
    #     bgl.glVertexAttribPointer(
    #         texturecoord_location, 2, bgl.GL_FLOAT, bgl.GL_FALSE, 0, None)
    #     bgl.glBindBuffer(bgl.GL_ARRAY_BUFFER, 0)

    #     bgl.glDrawArrays(bgl.GL_TRIANGLE_FAN, 0, 4)

    #     bgl.glBindVertexArray(0)
    #     bgl.glBindTexture(bgl.GL_TEXTURE_2D, 0)

    #     # DELETING
    #     bgl.glDeleteBuffers(2, vertex_buffer)
    #     bgl.glDeleteVertexArrays(1, vertex_array)

    # def _get_render_image(self):
    #     ''' This is only called for non-GL interop image gets '''
    #     # if utils.IS_MAC:
    #     #     with self.render_lock:
    #     #         return self.haystack_context.get_image()
    #     # else:
    #     # , self.haystack_context.get_image_right()
    #     return self.haystack_context.get_image()
        
    def draw_texture_2d(self, texture, position, width, height):
        import gpu
        from gpu_extras.batch import batch_for_shader

        coords = ((0, 0), (1, 0), (1, 1), (0, 1))

        shader = gpu.shader.from_builtin('IMAGE')
        batch = batch_for_shader(
            shader, 'TRI_FAN',
            {"pos": coords, "texCoord": coords},
        )

        with gpu.matrix.push_pop():
            gpu.matrix.translate(position)
            gpu.matrix.scale((width, height))

            shader = gpu.shader.from_builtin('IMAGE')

            # if isinstance(texture, int):
            #     # Call the legacy bgl to not break the existing API
            #     import bgl
            #     bgl.glActiveTexture(bgl.GL_TEXTURE0)
            #     bgl.glBindTexture(bgl.GL_TEXTURE_2D, texture)
            #     shader.uniform_int("image", 0)
            # else:
            #     shader.uniform_sampler("image", texture)            

            self.haystack_context.draw_texture()
            shader.uniform_int("image", 0)

            batch.draw(shader)        

    def draw(self, context):
        # log("Draw")

        if not self.is_synced or self.is_finished:
            return

        scene = context.scene

        with self.render_lock:
            viewport_settings = ViewportSettings(context)

            if viewport_settings.width * viewport_settings.height == 0:
                return

            # or viewport_settings.camera_dataR is None:
            if viewport_settings.camera_data is None:
                return

            if self.viewport_settings != viewport_settings:
                # viewport_settings.export_camera(self.haystack_context.scene.camera)
                # , viewport_settings.camera_dataR)
                self.haystack_context.set_camera(viewport_settings.camera_data)
                self.viewport_settings = viewport_settings

                if self.haystack_context.width != viewport_settings.width \
                        or self.haystack_context.height != viewport_settings.height:

                    resolution = (viewport_settings.width,
                                  viewport_settings.height)
                    
                    self.stop_render()
                    self.haystack_context.resize(*resolution)
                    self.start_render()

                    #self.restart_render_event.set()

                    #return       

                    # if self.gl_texture:
                    #     self.gl_texture = GLTexture(*resolution)

                    #self.is_resized = True

                # if haystack_dll._renderengine_dll.get_renderengine_type() != 2:
                #else:
                self.restart_render_event.set()

        # if self.is_resized or not self.is_rendered:
        #     return

        # def draw_(texture_id):
        #     # Bind shader that converts from scene linear to display space,
        #     bgl.glEnable(bgl.GL_BLEND)
        #     bgl.glBlendFunc(bgl.GL_ONE, bgl.GL_ONE_MINUS_SRC_ALPHA)
        #     self.haystack_engine.bind_display_space_shader(scene)

        #     # note this has to draw to region size, not scaled down size
        #     self._draw_texture(
        #         texture_id, *self.viewport_settings.border[0], *self.viewport_settings.border[1])

        #     self.haystack_engine.unbind_display_space_shader()
        #     bgl.glDisable(bgl.GL_BLEND)

        # with self.resolve_lock:
        #     #im = self._get_render_image()
        #     self.haystack_context.bind_texture()

        # self.gl_texture.set_image(im)

        #self.haystack_context.draw_texture()
        #self.haystack_context.render()
        texture_id = self.haystack_context.get_texture_id()
        # draw_(self.gl_texture.texture_id)
        #draw_(texture_id)
        
        # present
        gpu.state.blend_set('ALPHA_PREMULT')
        self.haystack_engine.bind_display_space_shader(scene)
        self.draw_texture_2d(texture_id, self.viewport_settings.border[0], self.viewport_settings.border[1][0], self.viewport_settings.border[1][1])        
        self.haystack_engine.unbind_display_space_shader()
        gpu.state.blend_set('NONE')            

        # check_gl_error()

#####################################################################################################################


class HayStackRenderEngine(bpy.types.RenderEngine):
    # These three members are used by blender to set up the
    # RenderEngine; define its internal name, visible name and capabilities.
    bl_idname = "HAYSTACK"
    bl_label = "HayStack"
    bl_use_preview = False
    bl_use_shading_nodes_custom=False

    engine: Engine = None

    # Init is called whenever a new render engine instance is created. Multiple
    # instances may exist at the same time, for example for a viewport and final
    # render.
    def __init__(self):
        self.engine = None

        dummy = gpu.types.GPUFrameBuffer()
        dummy.bind()  

    # When the render engine instance is destroy, this is called. Clean up any
    # render engine data here, for example stopping running render threads.
    def __del__(self):
        if isinstance(self.engine, ViewportEngine):
            self.engine.stop_render()
            self.engine = None
        pass

    # final render
    def update(self, data, depsgraph):
        """ Called for final render """
        pass

    # This is the method called by Blender for both final renders (F12) and
    # small preview for materials, world and lights.
    def render(self, depsgraph):
        pass

    # For viewport renders, this method gets called once at the start and
    # whenever the scene or 3D viewport changes. This method is where data
    # should be read from Blender in the same thread. Typically a render
    # thread will be started to do the work while keeping Blender responsive.
    def view_update(self, context, depsgraph):
        if self.engine:
            return

        self.engine = ViewportEngine(self)
        self.engine.sync(context, depsgraph)

    # For viewport renders, this method is called whenever Blender redraws
    # the 3D viewport. The renderer is expected to quickly draw the render
    # with OpenGL, and not perform other expensive work.
    # Blender will draw overlays for selection and editing on top of the
    # rendered image automatically.
    def view_draw(self, context, depsgraph):
        self.engine.draw(context)

    def update_render_passes(self, render_scene=None, render_layer=None):
        pass


class RenderButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "render"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)


class RENDER_PT_haystack_server(RenderButtonsPanel, bpy.types.Panel):
    bl_label = "Server"
    COMPAT_ENGINES = {'HAYSTACK'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        scene = context.scene
        server_settings = scene.haystack.server_settings

        pref = haystack_pref.preferences()
        box = layout.box()
        box.label(text='Remote/Local:')
        col = box.column()
        col.prop(pref, 'haystack_remote')              
        if pref.haystack_remote:
            col.prop(pref, 'ssh_server_name')
            col.prop(pref, 'ssh_server_node_name')

        box = layout.box()
        box.label(text='Haystack TCP Server:')
        col = box.column()
        col.prop(pref, "haystack_server_name", text="Server")
        col.prop(pref, "haystack_port_cam", text="Port Cam")
        col.prop(pref, "haystack_port_data", text="Port Data")             

        box = layout.box()
        col = box.column()
        #col.prop(server_settings, "node_tree", text="Node Tree")  
        #col.prop(server_settings, "node", text="Node")        
        if pref.haystack_remote:
            col.prop(server_settings, "job_command", text="Job Command")

        col.prop(server_settings, "haystack_command", text="HayStack Command")

        if context.scene.haystack_data.haystack_process is None:
            col.operator("haystack.start_process")
        else:
            col.operator("haystack.stop_process")

        if pref.haystack_remote:
            if context.scene.haystack_data.haystack_tunnel is None:
                col.operator("haystack.start_ssh_tunnel")
            else:
                col.operator("haystack.stop_ssh_tunnel")                

        if not pref.haystack_remote and not context.scene.haystack_data.haystack_process is None:            
            col.operator("haystack.create_bbox")

        if pref.haystack_remote and not context.scene.haystack_data.haystack_process is None and not context.scene.haystack_data.haystack_tunnel is None:
            col.operator("haystack.create_bbox")

        # box = layout.box()
        # col = box.column()
        # col.prop(server_settings, "server_name", text="Server")
        # col.prop(server_settings, "port_cam", text="Port Cam")
        # col.prop(server_settings, "port_data", text="Port Data")
        #col.prop(server_settings, "filename", text="Filename")

        # box = layout.box()
        # col = box.column()
        # col.prop(server_settings, "cam_rotation_X", text="Cam Rotation X")

        #box = layout.box()
        #col = box.column()
        #col.prop(server_settings, "timesteps", text="Time Steps")
        #col.prop(server_settings, "save_to_file", text="Save To File")                        

        box = layout.box()
        col = box.column()
        col.enabled = False
        col.prop(server_settings, "width", text="Width")
        col.prop(server_settings, "height", text="Height")
        #col.prop(server_settings, "step_samples", text="Step samples")

        box = layout.box()
        col = box.column()
        col.prop(server_settings, "mat_volume", text="Volume Material")  
        
###################################################################################

# RenderEngines also need to tell UI Panels that they are compatible with.
# We recommend to enable all panels marked as BLENDER_RENDER, and then
# exclude any panels that are replaced by custom panels registered by the
# render engine, or that are not supported.


def get_panels():
    exclude_panels = {
        'VIEWLAYER_PT_filter',
        'VIEWLAYER_PT_layer_passes',
        'RENDER_PT_eevee_ambient_occlusion',
        'RENDER_PT_eevee_motion_blur',
        'RENDER_PT_eevee_next_motion_blur',
        'RENDER_PT_motion_blur_curve',
        'RENDER_PT_eevee_depth_of_field',
        'RENDER_PT_eevee_next_depth_of_field',
        'RENDER_PT_eevee_bloom',
        'RENDER_PT_eevee_volumetric',
        'RENDER_PT_eevee_volumetric_lighting',
        'RENDER_PT_eevee_volumetric_shadows',
        'RENDER_PT_eevee_subsurface_scattering',
        'RENDER_PT_eevee_screen_space_reflections',
        'RENDER_PT_eevee_shadows',
        'RENDER_PT_eevee_next_shadows',
        'RENDER_PT_eevee_sampling',
        'RENDER_PT_eevee_indirect_lighting',
        'RENDER_PT_eevee_indirect_lighting_display',
        'RENDER_PT_eevee_film',
        'RENDER_PT_eevee_hair',
        'RENDER_PT_eevee_performance',

        'RENDER_PT_gpencil',
        'RENDER_PT_freestyle',
        'RENDER_PT_simplify',
    }    
    panels = []
    panels.append(RENDER_PT_haystack_server)

    for panel in bpy.types.Panel.__subclasses__():
        if hasattr(panel, 'COMPAT_ENGINES') and ('BLENDER_RENDER' in panel.COMPAT_ENGINES or 'BLENDER_EEVEE' in panel.COMPAT_ENGINES):
            if panel.__name__ not in exclude_panels:
                panels.append(panel)

    return panels


def register():
    # Register the RenderEngine
    bpy.utils.register_class(HayStackRenderEngine)
    bpy.utils.register_class(HayStackServerSettings)
    bpy.utils.register_class(HayStackRenderSettings)
    bpy.utils.register_class(RENDER_PT_haystack_server)
    bpy.utils.register_class(HayStackShowPopupErrorMessage)

    bpy.types.Scene.haystack = bpy.props.PointerProperty(
        name="HayStack Render Settings",
        description="HayStack render settings",
        type=HayStackRenderSettings,
    )

    bpy.types.Scene.haystack_data = HayStackData()

    for panel in get_panels():
        panel.COMPAT_ENGINES.add('HAYSTACK')


def unregister():
    bpy.utils.unregister_class(HayStackRenderEngine)
    bpy.utils.unregister_class(HayStackServerSettings)
    bpy.utils.unregister_class(HayStackRenderSettings)
    bpy.utils.unregister_class(RENDER_PT_haystack_server)
    bpy.utils.unregister_class(HayStackShowPopupErrorMessage)

    delattr(bpy.types.Scene, 'haystack')
    delattr(bpy.types.Scene, 'haystack_data')

    for panel in get_panels():
        if 'HAYSTACK' in panel.COMPAT_ENGINES:
            panel.COMPAT_ENGINES.remove('HAYSTACK')

