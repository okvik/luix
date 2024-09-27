#!/bin/lu9
local lfs = require "lfs"

for n in lfs.dir(".") do
	assert(type(n) == "string")
end
assert(lfs.attributes(".", "mode") == "directory")
local attr = assert(lfs.attributes("."))
assert(attr.mode == "directory")

local cwd = assert(lfs.currentdir())
assert(lfs.chdir("/"))
assert(lfs.currentdir() == "/")
assert(lfs.chdir(cwd))
assert(lfs.currentdir() == cwd)

assert(lfs.mkdir("DELETEME"))
assert(lfs.rmdir("DELETEME"))
