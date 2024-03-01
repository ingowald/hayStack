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

async def _ssh_tunnel(key_file, destination, port1, port2):
        """ Execute an ssh command """
        cmd = [
            'ssh',
            '-N',
            '-i', key_file,            
            '-L', port1,
            '-L', port2,
            destination,
            '&',
        ]

        import asyncio
        process = await asyncio.create_subprocess_exec(*cmd)
        await process.wait()

        if process.returncode != 0:
            print("ssh command failed: %s" % cmd)

async def _ssh_async(key_file, server, username, command):
    """ Execute an ssh command """

    if username is None:
        user_server = '%s' % (server)
    else:
        user_server = '%s@%s' % (username, server)        

    if key_file is None:
        cmd = [
            'ssh',
            user_server, command
        ]
    else:
        cmd = [
            'ssh',
            '-i',  key_file,
            user_server, command
        ]
        
    import asyncio
    process = await asyncio.create_subprocess_exec(
        *cmd, 
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE)        

    stdout, stderr = await process.communicate()

    if process.returncode != 0:
        if stdout:
            print(f'[stdout]\n{stdout.decode()}')
        if stderr:
            print(f'[stderr]\n{stderr.decode()}')        

        raise Exception("ssh command failed: %s" % cmd)

    return str(stdout.decode())

def _ssh_sync(key_file, server, username, command):
    """ Execute an ssh command """

    if username is None:
        user_server = '%s' % (server)
    else:
        user_server = '%s@%s' % (username, server)        

    if key_file is None:
        cmd = [
            'ssh',
            user_server, command
        ]
    else:
        cmd = [
            'ssh',
            '-i',  key_file,
            user_server, command
        ]

    import subprocess
    process = subprocess.Popen(cmd, stdin=subprocess.PIPE, stderr=subprocess.PIPE, stdout=subprocess.PIPE)
    stdout, stderr = process.communicate()

    if process.returncode != 0:
        if stdout:
            print(str(stdout.decode()))
        if stderr:
            print(str(stderr.decode()))        

        raise Exception("ssh command failed: %s" % cmd)

    return str(stdout.decode())

def _ssh_sync_sh(hostname, command):
    """ Execute an ssh command """

    # Using echo and properly escaping the script content for the remote shell
    # The command is constructed as a single string
    ssh_command = ['ssh', '-T', f'{hostname}']

    # Encode the command to bytes
    #encoded_command = ssh_command #.encode('utf-8')

    # Execute the SSH command using subprocess.Popen, passing the encoded command
    import subprocess
    process = subprocess.Popen(ssh_command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    process.stdin.write(command)
    process.stdin.close()  # Close stdin to indicate that we're done sending commands    

    # Wait for the command to complete and capture the output
    stdout, stderr = process.communicate()

    # Check for errors
    if process.returncode == 0:
        print("Script created successfully.")
        print(stdout)
    else:
        print("Failed to create script.")
        print(stderr)
        raise Exception("ssh command failed: %s" % command)

    return str(stdout)  

def _paramiko_ssh(key_file, server, username, password, command):
        """ Execute an paramiko ssh command """

        import paramiko

        ssh = None
        result = None
        try: 
            ssh = paramiko.SSHClient()
            ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            try:
                key = paramiko.RSAKey.from_private_key_file(key_file, password)
            except:
                key = paramiko.Ed25519Key.from_private_key_file(key_file, password)

            ssh.connect(server, username=username, pkey=key)
            stdin, stdout, stderr = ssh.exec_command(command)
            result = stdout.readlines()
            error = stderr.readlines()            

            if len(error) > 0:
                raise Exception(str(error))

            ssh.close()    

        except Exception as e:
            if ssh is not None:
                ssh.close()

            raise Exception("paramiko ssh command failed:  %s: %s" % (e.__class__, e))    

        return ''.join(result)

async def ssh_command_async(server, command):
    if command  is None:
        return None
    
    #return _paramiko_ssh(key_file, server, username, password, command)
    return await _ssh_async(None, server, None, command)

def ssh_command_sync(server, command):
    if command  is None:
        return None
        
    #return _paramiko_ssh(key_file, server, username, password, command)
    return _ssh_sync(None, server, None, command)

def ssh_command_sync_sh(server, command):
    if command  is None:
        return None
        
    #return _paramiko_ssh(key_file, server, username, password, command)
    return _ssh_sync_sh(server, command)
                  
async def _scp_async(key_file, source, destination):
        """ Execute an scp command """

        if key_file is None:
            cmd = [
                'scp',
                '-o', 'StrictHostKeyChecking=no',
                '-q',
                '-B',
                '-r',
                source, destination
            ]            
        else:
            cmd = [
                'scp',
                '-i',  key_file,
                '-o', 'StrictHostKeyChecking=no',
                '-q',
                '-B',
                '-r',
                source, destination
            ]

        import asyncio
        process = await asyncio.create_subprocess_exec(*cmd, 
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE)

        stdout, stderr = await process.communicate()

        if process.returncode != 0:
            if stdout:
                print(f'[stdout]\n{stdout.decode()}')
            if stderr:
                print(f'[stderr]\n{stderr.decode()}')        

            raise Exception("scp command failed: %s -> %s" % (source, destination))

def _paramiko_put(privateKey, serverHostname, username, password, source, destination):
        """ Execute an paramiko command """

        import paramiko
        from io import StringIO
        from base64 import b64decode
        from scp import SCPClient

        ssh = None
        scp = None
        try: 
            ssh = paramiko.SSHClient()
            ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            try:
                if password is None:
                    key = paramiko.RSAKey.from_private_key(StringIO(privateKey))
                else:
                    key = paramiko.RSAKey.from_private_key_file(privateKey, password)

            except:
                if password is None:
                    key = paramiko.Ed25519Key.from_private_key(StringIO(privateKey))
                else:
                    key = paramiko.Ed25519Key.from_private_key_file(privateKey, password)

            ssh.connect(serverHostname, username=username, pkey=key)
            scp = SCPClient(ssh.get_transport())
            scp.put(source, recursive=True, remote_path=destination)       
            scp.close()    

        except Exception as e:
            if scp is not None:
                scp.close()

            if ssh is not None:
                ssh.close()

            raise Exception("paramiko command failed:  %s: %s" % (e.__class__, e))


def _paramiko_get(privateKey, serverHostname, username, password, source, destination):
        """ Execute an paramiko command """

        import paramiko
        from io import StringIO
        from base64 import b64decode
        from scp import SCPClient

        ssh = None
        scp = None
        try: 
            ssh = paramiko.SSHClient()
            ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())

            try:
                if password is None:
                    key = paramiko.RSAKey.from_private_key(StringIO(privateKey))
                else:
                    key = paramiko.RSAKey.from_private_key_file(privateKey, password)                
            except:
                if password is None:
                    key = paramiko.Ed25519Key.from_private_key(StringIO(privateKey))
                else:
                    key = paramiko.Ed25519Key.from_private_key_file(privateKey, password)                

            ssh.connect(serverHostname, username=username, pkey=key)
            scp = SCPClient(ssh.get_transport())
            scp.get(source, local_path=destination, recursive=True)
            scp.close()

        except Exception as e:
            if scp is not None:
                scp.close()

            if ssh is not None:
                ssh.close()

            raise Exception("paramiko command failed:  %s: %s" % (e.__class__, e))
        

async def transfer_files(source, destination) -> None:
    await _scp_async(None, source, destination)        