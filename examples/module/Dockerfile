FROM alpine:3.19.0 AS build

RUN apk update && \
    apk add --no-cache \
        build-base \
        lua5.4 \
        lua5.4-dev

WORKDIR /module
COPY examples/module/ ./
COPY src/luabind/ luabind/

RUN gcc -std=c++20 -I/usr/include/lua5.4 -L/usr/lib/lua5.4 -llua -shared mymodule.cpp -o mymodule.so
RUN lua5.4 script.lua

ENTRYPOINT [ "./script.lua" ]
