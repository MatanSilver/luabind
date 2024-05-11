FROM alpine:3.19.0 AS build

RUN apk update && \
    apk add --no-cache \
        build-base \
        lua5.4 \
        lua5.4-dev

WORKDIR /module
COPY mymodule.cpp mymodule.cpp
COPY ../../src/luabind/ luabind/

RUN gcc mymodule.cpp -llua -o mymodule.so

ENTRYPOINT [ "./script.lua" ]