from socket import socket, AF_INET, SOCK_STREAM
import time
import uuid
import sys
import argparse

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

def build_arg(arg: bytes) -> bytearray:
    """Builds the given arg in the format required by the protocol.
    
    The format is: 4 bytes for representing the size, then as many bytes as the size for
    the actual content.
    """
    arr = bytearray()
    arg_size = int.to_bytes(len(arg), 4, 'big', signed=False)
    arr.extend(arg_size)
    arr.extend(arg)
    return arr

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

def put(sock: socket, key: bytes, value: bytes) -> int:

    command = bytearray()
    command.append(BT_PUT)
    sock.send(command)
    
    arg1 = build_arg(key)
    sock.send(arg1)

    arg2 = build_arg(value)
    sock.send(arg2)

    response = sock.recv(1)
    return response[0]


DEFAULT_ID = "memcached siege"
DEFAULT_HOST = "localhost"
DEFAULT_PORT = 8889
DEFAULT_VALUE_SIZE = 2_000_000 # in bytes
SERVER_MEMORY = 500_000_000 # in bytes
SEND_INTERVAL = 0.01

def main():
    """Reads the command line arguments and executes the memcached siege script.

    To run in parallel:
    $ seq 1 ${NUM} | parallel --jobs {NUM} --ungroup python siege.py --id {} --log-every 2 --value 5000
    """

    # Parser
    parser = argparse.ArgumentParser(description="Siege memcached")
    parser.add_argument("--id", default=DEFAULT_ID, type=str, help="Identifier for the siege instance")
    parser.add_argument("--host", default=DEFAULT_HOST, type=str, help="memcached host")
    parser.add_argument("--port", default=DEFAULT_PORT, type=int, help="memcached port")
    parser.add_argument("--value-size", default=DEFAULT_VALUE_SIZE, type=int, help="Size of each value")
    parser.add_argument("--server-memory", default=SERVER_MEMORY, type=int, help="Size of the memory of memcached (for reporting)")
    parser.add_argument("--random-values", action="store_true", help="Generate random values")
    parser.add_argument("--interval", default=SEND_INTERVAL, type=float, help="Interfal for sending values")
    parser.add_argument("--stop-after-failure", action="store_true", help="Stop after failure")
    parser.add_argument("--log-every", default=50, type=int, help="Amount of requests sent before logging progress")
    parser.add_argument("--total", default=None, type=int, help="Total amount of requests to send")

    args = parser.parse_args()

    print(f"Starting with id \"{args.id}\"...")

    sock = socket(AF_INET, SOCK_STREAM)
    sock.connect((args.host, args.port))

    value_str = "*" * args.value_size
    value = value_str.encode("ascii")
    memory_sent = 0
    
    counter = 0
    while True:
        key_str = uuid.uuid4().hex
        key = key_str.encode("ascii")
        key_size = len(key)

        response_code = put(sock, key, value)
        if (response_code != BT_OK):
            print(f"{args.id}> Received response: {get_printable_kind(response_code)}")
            if args.stop_after_failure:
                exit()
        
        counter += 1
        memory_sent += args.value_size + key_size
        time.sleep(args.interval)

        if counter % args.log_every == 0:
            print(f"{args.id}> Total PUTs sent: {counter}")
            memory_percent = memory_sent / args.server_memory * 100
            print(f"{args.id}> Approx memory sent: {memory_sent} (~{memory_percent}%)")
        
        if args.total is not None and counter >= args.total:
            print(f"{args.id}> Done!")
            return



if __name__ == "__main__":
    main()