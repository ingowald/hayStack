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
from . import haystack_dll
from . import haystack_remote

def start_process(context):
    # Specify the process's executable path
    server_settings = context.scene.haystack.server_settings

    if server_settings.haystack_command is None:
        raise Exception("Missing haystack command")

    process_command = str(server_settings.haystack_command.as_string())
    
    # Ensure the previous haystack_process is not running
    if context.scene.haystack_data.haystack_process is not None:
        context.scene.haystack_data.haystack_process.terminate()

    # set env
    pref = haystack_pref.preferences()
    if pref.haystack_remote:
        if server_settings.job_command is None:
            raise Exception("Missing job command")
                
        job_command = str(server_settings.job_command.as_string())

        if not "{HAYSTACK_COMMAND}" in job_command:
            raise Exception("Missing {HAYSTACK_COMMAND} in job command")

        job_command = job_command.replace("{HAYSTACK_COMMAND}", process_command).replace('\r\n', '\n')

        haystack_remote.ssh_command_sync_sh(pref.ssh_server_name, f"echo \'{job_command}\' > ~/haystack_command.sh; chmod +x ~/haystack_command.sh; sed -i \'s/\r$//\' ~/haystack_command.sh")
        process_command = "ssh " + pref.ssh_server_name + " ~/haystack_command.sh "

        # import platform
        # if platform.system() == 'Windows':
        #     process_command = "cmd /C \" start " + process_command + "\""
    
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


def start_ssh_tunnel(context):
    pref = haystack_pref.preferences()
    if not pref.haystack_remote:
        return
    
    # Ensure the previous haystack_tunnel is not running
    if context.scene.haystack_data.haystack_tunnel is not None:
        context.scene.haystack_data.haystack_tunnel.terminate()

    process_command = "ssh -L " + str(pref.haystack_port_cam) + ":" + \
         str(pref.ssh_server_node_name) + ":" + str(pref.haystack_port_cam) + \
         " -L " + str(pref.haystack_port_data) + ":" + \
         str(pref.ssh_server_node_name) + ":" + str(pref.haystack_port_data) + \
         " " + pref.ssh_server_name
    
    import platform
    if platform.system() == 'Windows':
        process_command = "cmd /C \" start " + process_command + "\""
    
    # Start the process in a new thread to avoid blocking Blender's UI
    def run():
        context.scene.haystack_data.haystack_tunnel = subprocess.Popen(process_command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1, universal_newlines=True)

        # Use threads to continuously read stdout and stderr without blocking
        def read_stream(stream, display_function):
            for line in iter(stream.readline, ''):
                display_function(line)
            stream.close()

        # Create and start threads for reading stdout and stderr
        threading.Thread(target=read_stream, args=(context.scene.haystack_data.haystack_tunnel.stdout, sys.stdout.write)).start()
        threading.Thread(target=read_stream, args=(context.scene.haystack_data.haystack_tunnel.stderr, sys.stderr.write)).start()

    print("Starting Tunnel: ", process_command)
    threading.Thread(target=run).start()    

def stop_process(context):
    if context.scene.haystack_data.haystack_process is not None:
        print("Terminating Process!")
        context.scene.haystack_data.haystack_process.terminate()
        context.scene.haystack_data.haystack_process = None

def stop_ssh_tunnel(context):
    if context.scene.haystack_data.haystack_tunnel is not None:
        print("Terminating tunnel!")
        context.scene.haystack_data.haystack_tunnel.terminate()
        context.scene.haystack_data.haystack_tunnel = None        

class HayStackStartProcessOperator(bpy.types.Operator):
    """Start the external process"""
    bl_idname = "haystack.start_process"
    bl_label = "Start Process"

    def execute(self, context):
        start_process(context)
        #bpy.ops.haystack.create_bbox()

        return {'FINISHED'}
    
class HayStackStopProcessOperator(bpy.types.Operator):
    """Stop the external process"""
    bl_idname = "haystack.stop_process"
    bl_label = "Stop Process"

    def execute(self, context):
        context.scene.haystack_data.haystack_engine.engine.stop_render()

        haystack_dll._renderengine_dll.reset()
        haystack_dll._renderengine_dll.client_close_connection()

        stop_process(context)
        return {'FINISHED'}    
    
class HayStackStartSSHTunnelOperator(bpy.types.Operator):
    """Start the Tunnel"""
    bl_idname = "haystack.start_ssh_tunnel"
    bl_label = "Start SSHTunnel"

    def execute(self, context):
        start_ssh_tunnel(context)

        return {'FINISHED'}            
class HayStackStopSSHTunnelOperator(bpy.types.Operator):
    """Stop the Tunnel"""
    bl_idname = "haystack.stop_ssh_tunnel"
    bl_label = "Stop SSHTunnel"

    def execute(self, context):
        stop_ssh_tunnel(context)

        return {'FINISHED'}      

def register():
    bpy.utils.register_class(HayStackStartProcessOperator)
    bpy.utils.register_class(HayStackStopProcessOperator)
    bpy.utils.register_class(HayStackStartSSHTunnelOperator)       
    bpy.utils.register_class(HayStackStopSSHTunnelOperator)

def unregister():
    bpy.utils.unregister_class(HayStackStartProcessOperator)
    bpy.utils.unregister_class(HayStackStopProcessOperator)
    bpy.utils.unregister_class(HayStackStartSSHTunnelOperator)
    bpy.utils.unregister_class(HayStackStopSSHTunnelOperator)
