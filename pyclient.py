import socket
import threading
import struct
import curses
from math import ceil

NAMESIZE = 128
MAXDATASIZE = 1024

packet_format = "!I" + str(NAMESIZE) + "s" + str(MAXDATASIZE) + "s"

history = dict()
historyIndex = 0
historyUpdated = False
done = False

def recv_data(server_socket):
    global historyIndex
    global historyUpdated
    global done
    while not done:
        data = bytearray()
        try:
            while (len(data) < 4+NAMESIZE+MAXDATASIZE):
                data += server_socket.recv(4+NAMESIZE+MAXDATASIZE)
        except Exception:
            break
        if not data:
            break
        unpacked_data = struct.unpack(packet_format, data)
        packet_index, sender, message = unpacked_data
        if (packet_index > historyIndex):
            historyIndex = packet_index
        if (packet_index in history):
            continue
        history[packet_index] = (sender.decode().split("\0")[0], message.decode().split("\0")[0])
        historyUpdated = True
    done = True

def print_chats():
    global historyUpdated
    while not done:
        if not historyUpdated:
            continue
        top = screen_height - 2
        for i in range(historyIndex, -1, -1):
            if (i not in history):
                exit(69)
            length = 1 + len(history[i][0]) + len(history[i][0])
            top -= ceil(length/screen_width)
            if top < 0:
                break
            stdscr.addstr(top, 0, history[i][0] + ": " + history[i][1])
            stdscr.clrtoeol()
        stdscr.move(screen_height-1, 0)
        stdscr.refresh()
        historyUpdated = False

host = input("host: ")
port = int(input("port: "))

server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

server_socket.connect((host, port))

name = input("username: ")
server_socket.send(name.encode())

stdscr = curses.initscr()
curses.cbreak()
curses.noecho()
stdscr.clear()
screen_height, screen_width = stdscr.getmaxyx()

buf = ""

stdscr.hline(screen_height-2, 0, '-', screen_width)
stdscr.move(screen_height-1, 0)
stdscr.refresh()

recv_thread = threading.Thread(target=recv_data, args=(server_socket,))
print_chats_thread = threading.Thread(target=print_chats, args=())

recv_thread.start()
print_chats_thread.start()

while (ch := stdscr.getch()) != 27:
    if (32 <= ch < 127 and len(buf) + 1 < MAXDATASIZE):
        if (len(buf) + 1 >= MAXDATASIZE):
            curses.beep()
        else:
            buf += chr(ch)
    if (ch == 127 and len(buf) > 0):
        buf = buf[:-1]
    if (ch == 10 and len(buf) > 0):
        server_socket.send(buf.encode())
        buf = ""

    stdscr.hline(screen_height-2, 0, '-', screen_width)
    if (len(buf) < screen_width):
        stdscr.addstr(screen_height-1, 0, buf)
    else:
        stdscr.addstr(screen_height-1, 0, "..." + buf[4-screen_width:])

    stdscr.clrtoeol()
    stdscr.refresh()

server_socket.close()

curses.endwin()
