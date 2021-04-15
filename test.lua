#!/bin/lu9

local p9 = require "p9"
local dump = (function()
	local ok, inspect = pcall(require, "inspect")
	if ok then return function(v) print(inspect(v)) end end
	return print
end)()

local function tmp()
	return string.format("/tmp/lua.%x", math.random(1e10))
end

p9.rfork("en")
os.execute("ramfs")

local t = {
	{nil, nil}, {"", nil}, {nil, ""},
	{"r", nil}, {"r", ""},
	{"w", "d644"},
	{"rw", "d644"},
	{nil, "d644"},
	{"", "d644"},
	{"r", "d644"},
	{"w", "d644"},
	{"rw", "d644"}
}

for i = 1, #t do
	local mode = t[i][1]
	local perm = t[i][2]
	p9.close(p9.create(tmp(), mode, perm))
end



-- File I/O
do
	local s = string.rep("ABCD", 2048*2) -- 16k > standard 8k buffer
	local f = tmp()
	local fd = p9.create(f, "rw")
	p9.write(fd, s)
	p9.close(fd)
	fd = p9.open(f, "r")
	assert(p9.slurp(fd) == s)
	p9.close(fd)
	fd = p9.open(f, "r")
	assert(p9.slurp(fd, 2048) == string.rep("ABCD", 512))
	p9.close(fd)
	fd = p9.open(f, "r")
	assert(p9.slurp(fd, 16*1024 + 999) == s)
	p9.close(fd)
	
	fd = p9.open(f, "r")
	assert(p9.seek(fd, 0, "end") == 16*1024)
	assert(p9.seek(fd, 8192, "set") == 8192
		and p9.slurp(fd) == string.rep("ABCD", 2*1024))
	p9.seek(fd, 0)
	assert(p9.seek(fd, 16*1024 - 4, "cur") == 16*1024 - 4
		and p9.slurp(fd) == "ABCD")
	p9.close(fd)
end

-- fd2path
do
	local fd = p9.create("/tmp/fd2path")
	assert(p9.fd2path(fd) == "/tmp/fd2path")
end

-- File objects
-- Closing
-- Make sure it's closed
local fd
do
	local f <close> = p9.createfile(tmp())
	fd = f.fd
end
assert(p9.seek(fd, 0) == nil)
-- Make sure it's not closed
local fd
do
	local f = p9.createfile(tmp())
	fd = f.fd
end
assert(p9.seek(fd, 0))
p9.close(fd)

-- Basic operations. These are the same as regular
-- function calls so no need for much testing.
do
	local f <close> = p9.createfile(tmp(), "rw")
	local data = string.rep("ABCD", 1024)
	f:write(data)
	f:seek(0)
	assert(f:slurp() == data)
end

-- Filesystem
do
	-- Create a test tree
	local function File(data) return {
		type = "file", perm = "644", data = data
	} end
	local function Dir(children) return {
		type = "dir", perm = "d755", children = children
	} end
	local function mkfs(path, d)
		assert(d.type == "dir")
		p9.createfile(path, nil, d.perm):close()
		for name, c in pairs(d.children) do
			local new = path .. "/" .. name
			if c.type == "dir" then
				mkfs(new, c)
			else
				local f <close> = p9.createfile(new, "w", c.perm)
				f:write(c.data)
			end
		end
	end
	local fs = Dir {
		a = File "a",
		b = Dir {},
		c = Dir {
			ca = File "ca",
			cb = Dir {
				cba = File "cba",
			},
			cc = File "cc",
		},
		d = File "d",
	}
	mkfs("/tmp/fs", fs)
	
	-- Stat a file
	assert(p9.stat("/tmp/fs/a").mode.file)
	
	-- Walking
	-- Walking a file (or any other error) must report an error
	local e = {}
	for w in p9.walk("/tmp/fs/a", e) do
		assert(false)
	end
	assert(e.error == "walk in a non-directory")
	-- Without error object an error must be thrown
	assert(false == pcall(function()
		for w in p9.walk("tmp/fs/a") do end
	end))
	-- Same should happen if the iterator function fails inside
	-- the loop because of dirread(2) failure, but this kind of
	-- failure is hard to simulate.
	
	-- Walking a directory
	local function compare(path, fs)
		assert(fs.type == "dir")
		for f in p9.walk(path) do
			local new = path .. "/" .. f.name
			if f.mode.dir then
				if compare(new, fs.children[f.name]) == false then
					return false
				end
			else
				if fs.children[f.name] == nil then
					error("file does not exist in proto")
				end
			end
		end
		return true
	end
	assert(compare("/tmp/fs", fs) == true)
end



-- Namespaces
-- bind and unmount work
assert(function()
	local f
	assert(p9.bind("#|", "/n/pipe"))
	f = assert(p9.openfile("/n/pipe/data"))
	assert(p9.unmount("/n/pipe"))
	assert(p9.openfile("/n/pipe/data") == nil)
end)
-- mount works
assert(function()
	assert(p9.mount(p9.open("/srv/cwfs", "rw"), nil, "/n/test"))
	assert(p9.openfile("/n/test/lib/glass"))
end)


-- Process control
-- No idea how to test this properly.



-- Environment variables
do
	local e
	
	assert(p9.env["sure-is-empty"] == nil)
	-- Set and get a string variable
	p9.env.test = "ABC"; assert(p9.env.test == "ABC")
	-- Delete a variable
	p9.env.test = nil; assert(p9.env.test == nil)
	
	-- Set and get a list variable
	p9.env.test = {"a", "b", "c"}
	e = p9.env.test
	assert(type(e) == "table"
		and #e == 3 and e[1] == "a" and e[2] == "b" and e[3] == "c")
	-- Ensure it's understood by rc
	os.execute("echo -n $#test $test >/env/res")
	assert(p9.env.res == "3 a b c")
	-- Ensure we understand rc
	os.execute("test=(d e f)")
	e = p9.env.test
	assert(type(e) == "table"
		and #e == 3 and e[1] == "d" and e[2] == "e" and e[3] == "f")
	p9.env.test = nil
end
