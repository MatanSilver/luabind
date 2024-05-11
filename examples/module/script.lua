#!/usr/bin/env lua5.4
package.cpath = "./?.so"
require "hello"

print("Testing say_hello()")
assert("hello world!" == hello.say_hello())