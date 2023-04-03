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
-define(BT_EINVAL, 111).
-define(BT_ENOTFOUND, 112).
-define(BT_EBINARY, 113).
-define(BT_EBIG, 114).
-define(BT_EUNK, 115).

% Transforms a response code from memcached to an atom.
response_code_to_atom(?BT_OK) -> ok;
response_code_to_atom(?BT_EINVAL) -> einval;
response_code_to_atom(?BT_ENOTFOUND) -> enotfound;
response_code_to_atom(?BT_EBINARY) -> ebinary;
response_code_to_atom(?BT_EBIG) -> ebig;
response_code_to_atom(?BT_EUNK) -> eunk;
response_code_to_atom(_) -> unknown.

start(Ip) ->
    case gen_tcp:connect(Ip, ?BINARY_PORT, [binary, {active, false}, {packet, raw}]) of
        {ok, Socket} -> {ok, Socket};
        {error, Reason} -> io:format("Error connecting to memcached. Reason: ~p~n", [Reason])
    end.

start() -> start(localhost).

receive_and_print(Socket) ->
    case gen_tcp:recv(Socket, 1) of
        {ok, <<ResponseCode>>} ->
            case response_code_to_atom(ResponseCode) of
                ok ->
                    case gen_tcp:recv(Socket, 4) of
                        {ok, BinSize} ->
                            ContentSize = binary:decode_unsigned(BinSize),
                            io:format("gonna read ~p bytes~n", [ContentSize]),
                            case gen_tcp:recv(Socket, ContentSize) of
                                {ok, BinContent} ->
                                    io:format("A ver: ~p~n", [BinContent]);
                                {error, Reason} ->
                                    io:format("Error receiving content. Reason: ~p~n", [Reason])
                            end,
                            gen_tcp:close(Socket),
                            ok;
                        {error, Reason} ->
                            io:format("Error receiving content size. Reason: ~p~n", [Reason]),
                            failure
                    end;
                _ ->
                    io:format("mal")
            end;
        {error, Reason} ->
            io:format("Error receiving response code. Reason: ~p~n", [Reason])
    end.

stats(Socket) ->
    case gen_tcp:send(Socket, <<?BT_STATS>>) of
        ok ->
            receive_and_print(Socket);
        {error, Reason} ->
            io:format("Error sending STATS to memcached. Reason: ~p~n", [Reason]),
            failure
    end.
