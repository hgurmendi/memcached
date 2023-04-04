-module(memcached).
-compile(export_all).

-define(BINARY_PORT, 8889).

% memcached commands.
-define(BT_PUT, 11).
-define(BT_DEL, 12).
-define(BT_GET, 13).
-define(BT_TAKE, 14).
-define(BT_STATS, 21).

% memcached response codes.
-define(BT_OK, 101).
%% not actually used.
-define(BT_EINVAL, 111).
-define(BT_ENOTFOUND, 112).
% not actually used.
-define(BT_EBINARY, 113).
% not actually used.
-define(BT_EBIG, 114).
% use to signal a memory error.
-define(BT_EUNK, 115).

start(Ip) ->
    case gen_tcp:connect(Ip, ?BINARY_PORT, [binary, {active, false}, {packet, raw}]) of
        {ok, Socket} -> {ok, Socket};
        Error -> Error
    end.

start() -> start(localhost).

% Builds the binary payload of a request argument according to the protocol.
build_arg_payload(Term) ->
    BinTerm = term_to_binary(Term),
    BinTermSize = byte_size(BinTerm),
    <<<<BinTermSize:32/unsigned>>/binary, BinTerm/binary>>.

% Builds the binary payload of a request according to the protocol. Comes in 3 flavors depending
% on the number of arguments.
build_request_payload(Command) ->
    <<Command>>.
build_request_payload(Command, Key) ->
    BinKey = build_arg_payload(Key),
    <<Command, BinKey/binary>>.
build_request_payload(Command, Key, Value) ->
    BinKey = build_arg_payload(Key),
    BinValue = build_arg_payload(Value),
    <<Command, BinKey/binary, BinValue/binary>>.

% Receives a variable sized binary value from the given socket and returns it.
receive_binary_value(Socket) ->
    case gen_tcp:recv(Socket, 4) of
        {ok, SizeInBinary} ->
            Size = binary:decode_unsigned(SizeInBinary),
            case gen_tcp:recv(Socket, Size) of
                {ok, BinContent} -> {ok, BinContent};
                Error -> Error
            end;
        Error ->
            Error
    end.

% Receives an Erlang term from the given socket and returns it.
receive_term(Socket) ->
    case receive_binary_value(Socket) of
        {ok, BinContent} -> {ok, binary_to_term(BinContent)};
        Error -> Error
    end.

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%% PUT
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

% Sends the PUT request with Erlang terms.
send_put_request(Socket, Key, Value) ->
    Payload = build_request_payload(?BT_PUT, Key, Value),
    gen_tcp:send(Socket, Payload).

% Receives the response to the PUT request.
receive_put_response(Socket) ->
    case gen_tcp:recv(Socket, 1) of
        {ok, <<?BT_OK>>} -> {ok, ok};
        {ok, <<?BT_EUNK>>} -> {ok, eunk};
        Error -> Error
    end.

% Sends a PUT request and waits for the response.
put(Socket, Key, Value) ->
    case send_put_request(Socket, Key, Value) of
        ok -> receive_put_response(Socket);
        Error -> Error
    end.

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%% GET
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

% Sends the GET request with an Erlang term.
send_get_request(Socket, Key) ->
    Payload = build_request_payload(?BT_GET, Key),
    gen_tcp:send(Socket, Payload).

% Receives the response to the GET request.
receive_get_response(Socket) ->
    case gen_tcp:recv(Socket, 1) of
        {ok, <<?BT_OK>>} ->
            case receive_term(Socket) of
                {ok, Term} -> {ok, Term};
                Error -> Error
            end;
        {ok, <<?BT_ENOTFOUND>>} ->
            {ok, enotfound};
        {ok, <<?BT_EUNK>>} ->
            {ok, eunk};
        {ok, _} ->
            {ok, unknown};
        Error ->
            Error
    end.

% Sends a PUT request and waits for the response.
get(Socket, Key) ->
    case send_get_request(Socket, Key) of
        ok -> receive_get_response(Socket);
        Error -> Error
    end.

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%% TAKE
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

% Sends the TAKE request with an Erlang term.
send_take_request(Socket, Key) ->
    Payload = build_request_payload(?BT_TAKE, Key),
    gen_tcp:send(Socket, Payload).

% Receives the response to the TAKE request.
receive_take_response(Socket) ->
    case gen_tcp:recv(Socket, 1) of
        {ok, <<?BT_OK>>} ->
            case receive_term(Socket) of
                {ok, Term} -> {ok, Term};
                Error -> Error
            end;
        {ok, <<?BT_ENOTFOUND>>} ->
            {ok, enotfound};
        {ok, <<?BT_EUNK>>} ->
            {ok, eunk};
        {ok, _} ->
            {ok, unknown};
        Error ->
            Error
    end.

% Sends a TAKE request and waits for the response.
take(Socket, Key) ->
    case send_take_request(Socket, Key) of
        ok -> receive_take_response(Socket);
        Error -> Error
    end.

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%% DEL
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

% Sends the DEL request with an Erlang term.
send_del_request(Socket, Key) ->
    Payload = build_request_payload(?BT_DEL, Key),
    gen_tcp:send(Socket, Payload).

% Receives the response to the DEL request.
receive_del_response(Socket) ->
    case gen_tcp:recv(Socket, 1) of
        {ok, <<?BT_OK>>} ->
            {ok, ok};
        {ok, <<?BT_ENOTFOUND>>} ->
            {ok, enotfound};
        {ok, <<?BT_EUNK>>} ->
            {ok, eunk};
        {ok, _} ->
            {ok, unknown};
        Error ->
            Error
    end.

% Sends a DEL request and waits for the response.
del(Socket, Key) ->
    case send_del_request(Socket, Key) of
        ok -> receive_del_response(Socket);
        Error -> Error
    end.

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%% STATS
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

% Sends the STATS request.
send_stats_request(Socket) ->
    Payload = build_request_payload(?BT_STATS),
    gen_tcp:send(Socket, Payload).

% Receives the response to the STATS request.
receive_stats_response(Socket) ->
    case gen_tcp:recv(Socket, 1) of
        {ok, <<?BT_OK>>} ->
            case receive_binary_value(Socket) of
                {ok, BinValue} ->
                    {ok, binary_to_list(BinValue)};
                Error ->
                    Error
            end;
        {ok, <<?BT_EUNK>>} ->
            {ok, eunk};
        {ok, _} ->
            {ok, unknown};
        Error ->
            Error
    end.

% Sends a STATS request and waits for the response.
stats(Socket) ->
    case send_stats_request(Socket) of
        ok -> receive_stats_response(Socket);
        Error -> Error
    end.
