import socket
import threading
import struct

NAMESIZE = 128
MAXDATASIZE = 1024

packet_format = "i" + str(NAMESIZE) + "s" + str(MAXDATASIZE) + "s"

def send_data(server_socket):
    while True:
        message = input("send: ")
        server_socket.send(message.encode())

def recv_data(server_socket):
    while True:
        data = server_socket.recv(4+NAMESIZE+MAXDATASIZE)
        if not data:
            break
        unpacked_data = struct.unpack(packet_format, data)
        print(f"\r{unpacked_data[1].decode()}: {unpacked_data[2].decode()}\nsend: ", end="")

host = input("host: ")
port = int(input("port: "))

server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

server_socket.connect((host, port))

name = input("username: ")
server_socket.send(name.encode())

recv_thread = threading.Thread(target=recv_data, args=(server_socket,))

recv_thread.start()

send_data(server_socket)

server_socket.close()
