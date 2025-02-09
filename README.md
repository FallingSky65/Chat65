# Chat65

Chat65 is a messaging app. The server is written in C, and currently there are two available clients, written in C and python.

## Hosting the Server

Clone this repository, then compile `server.c` with `gcc` (you may need some extra flags, but it works without extra flags for me).
Run the server, and it will be waiting for connections on port `3490`.
At this point you can connect with the client on your LAN. If you want to connect through the internet, you should look into tunneling with [ngrok](https://ngrok.com/)

## Connecting with the client

### C client

Clone this repository, then compile `ncursesclient.c` with `gcc` (you may need to add the `-lncurses` flag).
Run the client in the terminal with the server address and the port number as the arguments
Ex. on localhost
`./ncursesclient.c localhost 3490`
Ex. with ngrok tcp tunnel
`./ncursesclient.c 0.tcp.ngrok.io 12345`

### Python client

Clone this repository. If you are on windows you should pip install `windows-curses`.
Run `python pyclient.py` to use the client, then enter the server address and port number.
