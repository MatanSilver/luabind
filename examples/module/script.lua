#!/usr/bin/env lua5.4
package.cpath = "./?.so"
require "mymodule"

print("Testing say_hello()")
assert("hello world!" == mymodule.say_hello())
