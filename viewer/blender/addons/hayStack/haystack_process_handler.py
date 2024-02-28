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
import subprocess
import threading
import sys
import os

from . import haystack_pref

def start_process(context):
    # Specify the process's executable path
    server_settings = context.scene.haystack.server_settings

    if server_settings.command is None:
        return

    process_command = str(server_settings.command.as_string())
    
    # Ensure the previous haystack_process is not running
    if context.scene.haystack_data.haystack_process is not None:
        context.scene.haystack_data.haystack_process.terminate()

    # set env
    pref = haystack_pref.preferences()
    os.environ['SOCKET_SERVER_PORT_CAM'] = str(pref.haystack_port_cam)
    os.environ['SOCKET_SERVER_PORT_DATA'] = str(pref.haystack_port_data)
    os.environ['SOCKET_SERVER_NAME_CAM'] = str(pref.haystack_server_name)
    os.environ['SOCKET_SERVER_NAME_DATA'] = str(pref.haystack_server_name)        
    
    # Start the process in a new thread to avoid blocking Blender's UI
    def run():
        context.scene.haystack_data.haystack_process = subprocess.Popen(process_command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1, universal_newlines=True)

        # Use threads to continuously read stdout and stderr without blocking
        def read_stream(stream, display_function):
            for line in iter(stream.readline, ''):
                display_function(line)
            stream.close()

        # Create and start threads for reading stdout and stderr
        threading.Thread(target=read_stream, args=(context.scene.haystack_data.haystack_process.stdout, sys.stdout.write)).start()
        threading.Thread(target=read_stream, args=(context.scene.haystack_data.haystack_process.stderr, sys.stderr.write)).start()

    print("Starting Process: ", process_command)
    threading.Thread(target=run).start()

def stop_process(context):
    if context.scene.haystack_data.haystack_process is not None:
        print("Terminating Process!")
        context.scene.haystack_data.haystack_process.terminate()
        context.scene.haystack_data.haystack_process = None

class HayStackStartProcessOperator(bpy.types.Operator):
    """Start the external process"""
    bl_idname = "haystack.start_process"
    bl_label = "Start Process"

    def execute(self, context):
        start_process(context)
        return {'FINISHED'}

class HayStackStopProcessOperator(bpy.types.Operator):
    """Stop the external process"""
    bl_idname = "haystack.stop_process"
    bl_label = "Stop Process"

    def execute(self, context):
        stop_process(context)
        return {'FINISHED'}

def register():
    bpy.utils.register_class(HayStackStartProcessOperator)
    bpy.utils.register_class(HayStackStopProcessOperator)    

def unregister():
    bpy.utils.unregister_class(HayStackStartProcessOperator)
    bpy.utils.unregister_class(HayStackStopProcessOperator)


