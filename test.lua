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

local function rc()
	os.execute("prompt='p9; ' rc -i")
end

p9.rfork("env name")
os.execute("ramfs")



-- File I/O
do
	local s = string.rep("ABCD", 2048*2) -- 16k > standard 8k buffer
	local f = tmp()
	local fd = p9.create(f, "w")
	fd:write(s)
	fd:close()
	
	fd = p9.open(f)
	assert(fd:slurp() == s)
	fd:close()
	
	fd = p9.open(f, "r")
	assert(fd:slurp(2048) == string.rep("ABCD", 512))
	fd:close()
	
	fd = p9.open(f, "r")
	assert(fd:slurp(16*1024 + 999) == s)
	fd:close()
	
	fd = p9.open(f, "r")
	assert(fd:seek(0, "end") == 16*1024)
	assert(fd:seek(8192, "set") == 8192
		and fd:slurp() == string.rep("ABCD", 2*1024))
	fd:seek(0)
	assert(fd:seek(16*1024 - 4, "cur") == 16*1024 - 4
		and fd:slurp() == "ABCD")
	fd:close()
end

-- File objects
-- Closing
-- Make sure it's closed
local fd
do
	local f <close> = p9.create(tmp())
	fd = f.fd
end
assert(p9.file(fd):close() == nil)
-- Make sure it's not closed
local fd
do
	local f = p9.create(tmp())
	fd = f.fd
end
assert(p9.file(fd):seek(0))
p9.file(fd):close()

-- file:path()
do
	local f = p9.create("/tmp/fd2path")
	assert(f:path() == "/tmp/fd2path")
	f:close()
end

-- file:iounit()
do
	local f = assert(p9.open("/srv/slashmnt"))
	assert(f:iounit() ~= 0)
	f:close()
end

-- file:dup()
do
	local a, b = assert(p9.pipe())
	local c = assert(a:dup())
	a:write("hello")
	assert(b:read() == "hello")
	c:write("world")
	assert(b:read() == "world")
	a:close() b:close() c:close()
	
	a = assert(p9.open("/lib/glass"))
	local buf = a:read()
	b = assert(p9.open("/lib/bullshit"))
	b = assert(a:dup(b))
	b:seek(0)
	assert(b:read() == buf)
end

-- access
do
	assert(p9.access("/dev/null"))
	assert(p9.access("/dev/null", "read write"))
	assert(p9.access("/bin/rc", "exec"))
end
	
-- pipe
do
	local p₀, p₁ = assert(p9.pipe())
	p₀:write("ABCD")
	assert(p₁:read() == "ABCD")
	p₀:close(); p₁:close()
end

-- wstat
do
	local name = tmp()
	assert(p9.create(name, nil, "644")):close()
	assert(p9.wstat(name, {name = "notyourbusiness", mode = "append 777"}))
	local st = assert(p9.stat("/tmp/notyourbusiness"))
	assert(st.mode.file and st.mode.append and st.mode.perm == "rwxrwxrwx")
end

-- Filesystem
do
	-- Create a test tree
	local function File(data) return {
		type = "file", perm = "644", data = data
	} end
	local function Dir(children) return {
		type = "dir", perm = "dir 755", children = children
	} end
	local function mkfs(path, d)
		assert(p9.create(path, nil, d.perm))
		for name, c in pairs(d.children) do
			local new = path .. "/" .. name
			if c.type == "dir" then
				mkfs(new, c)
			else
				local f <close> = assert(p9.create(new, "w", c.perm))
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
	local function walk(fs)
		dump(fs.perm)
		for _, e in pairs(fs.children) do
			if e.type == "dir" then
				walk(e)
			end
		end
	end
	local ok, err = pcall(mkfs, "/tmp/fs", fs)
	if not ok then print(err) end
	
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
do
	local f
	assert(p9.bind("#|", "/n/pipe"))
	f = assert(p9.open("/n/pipe/data"))
	assert(p9.unmount("/n/pipe"))
	assert(p9.open("/n/pipe/data") == nil)
end
-- mount works
do
	assert(p9.mount("/srv/cwfs", "/n/test"))
	assert(p9.open("/n/test/lib/glass"))
end

-- Process control
-- No idea how to test some of this
do
	assert(p9.sleep(0) == true)
end



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




-- Misc.

-- cleanname
do
	assert(p9.cleanname("/usr///./glenda/.") == "/usr/glenda")
end
