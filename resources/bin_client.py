import socket

HOST = "localhost"
BIN_PORT = 7667
TEXT_PORT = 7666

COMMANDS = {"PUT", "GET", "DEL", "TAKE", "STATS"}

BT_PUT = 11
BT_DEL = 12
BT_GET = 13
BT_TAKE = 14
BT_STATS = 21
BT_OK = 101
BT_EINVAL = 111
BT_ENOTFOUND = 112
BT_EBINARY = 113
BT_EBIG = 114
BT_EUNK = 115

def get_printable_kind(kind: int):
    if kind == 11:
        return "PUT"
    elif kind == 12:
        return "DEL"
    elif kind == 13:
        return "GET"
    elif kind == 14:
        return "TAKE"
    elif kind == 21:
        return "STATS"
    elif kind == 101:
        return "OK"
    elif kind == 111:
        return "EINVAL"
    elif kind == 112:
        return "ENOTFOUND"
    elif kind == 113:
        return "EBINARY"
    elif kind == 114:
        return "EBIG"
    elif kind == 115:
        return "EUNK"

# bytearray() can be modified, bytes() cannot

# parses the given response as a immutable bytes array and prints it.
def parse_response(response: bytes):
    kind_bytes = response[0]
    kind_str = get_printable_kind(kind_bytes)

    print(f"Received {kind_str}")

    if kind_bytes != BT_OK:
        # All kinds except BT_OK are single byte responses.
        return
    
    if len(response) == 1:
        # An OK response with no more data means no content.
        return
    
    content_size_bytes = response[1:5]
    content_size_int = int.from_bytes(content_size_bytes, byteorder='big', signed = False)
    print("should receive a content with length", content_size_int)

    content = response[5:5+content_size_int]
    content_str = content.decode("ascii")
    print(f"Received the content <{content_str}> ({len(content_str)} chars)")

def handle_response(sock: socket.socket):
    print(" Handling response?")
    # print the response
    response = sock.recv(2048)
    parse_response(response)


def handle_stats(sock: socket.socket, args: list[str]):
    print("sending stats")

    msg = bytearray()
    #command
    msg.append(BT_STATS)

    # send the bytes
    sock.send(msg)

def handle_put(sock: socket.socket, args: list[str]):
    if len(args) != 2:
        print("NOT ENOUGH ARGUMENTS!")
        return
    
    print(f"sending PUT {args[0]} {args[1]}")

    msg = bytearray()
    # command
    msg.append(BT_PUT)

    # key
    key_str = args[0]
    key_bytes = key_str.encode("ascii")
    key_size_int = len(key_str)
    key_size_bytes = int.to_bytes(key_size_int, 4, 'big', signed=False)
    msg.extend(key_size_bytes)
    msg.extend(key_bytes)

    #value
    value_str = args[1]
    value_bytes = value_str.encode("ascii")
    value_size_int = len(value_str)
    value_size_bytes = int.to_bytes(value_size_int, 4, 'big', signed=False)
    msg.extend(value_size_bytes)
    msg.extend(value_bytes)

    # send the bytes
    sock.send(msg)


def handle_get(sock: socket.socket, args: list[str]):
    if len(args) != 1:
        print("NOT ENOUGH ARGUMENTS!")
        return
    
    print(f"sending GET {args[0]}")

    msg = bytearray()
    # command
    msg.append(BT_GET)

    # key
    key_str = args[0]
    key_bytes = key_str.encode("ascii")
    key_size_int = len(key_str)
    key_size_bytes = int.to_bytes(key_size_int, 4, 'big', signed=False)
    msg.extend(key_size_bytes)
    msg.extend(key_bytes)

    # send the bytes
    sock.send(msg)

def handle_line(sock: socket.socket, line: str):
    tokens = line.split()
    if len(tokens) == 0:
        print("WRONG!")
        return
    
    command = tokens[0].upper()
    args = tokens[1:]

    if command not in COMMANDS:
        print("UNKNOWN COMMAND!")
        return
    
    print(f"argumentos: {args}")
    
    if command == "STATS":
        handle_stats(sock, args)
    elif command == "PUT":
        handle_put(sock, args)
    elif command == "GET":
        handle_get(sock, args)

    handle_response(sock)

def cli():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((HOST, BIN_PORT))
    try:
        while True:
            line = input("memcached> ")
            handle_line(sock, line)
    except KeyboardInterrupt:
        pass




if __name__ == "__main__":
    cli()

