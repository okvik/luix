local p9 = {}
local raw = require "p9.raw"
p9.raw = raw

-- Defaults &c
p9.iosize = 8192




-- Utility / debugging

local function fwrite(fmt, ...)
	io.write(string.format(fmt, ...))
end

-- typecheck(values... : any, types : string) -> string...
-- typecheck matches the types of values against a string
-- of space-separated type names.
-- If a value without a matching type is found an error
-- describing the type mismatch is thrown.
-- Otherwise the function returns a type of each value.
local function typecheck(...)
	local values = table.pack(...)
	local t = table.remove(values)
	local accept = {}
	for w in t:gmatch("%w+") do
		accept[w] = true
	end
	local types = {}
	for _, v in ipairs(values) do
		local tv = type(v)
		types[#types + 1] = tv
		if not accept[tv] then
			error(("expected type(s) [%s], got %s"):format(t, tv), 2)
		end
	end
	return table.unpack(types)
end



-- Namespace

local mntmap <const> = {
	["a"] = raw.MAFTER,
	["b"] = raw.MBEFORE,
	["c"] = raw.MCREATE,
	["C"] = raw.MCACHE,
}
local function parsemntflags(s)
	local f = raw.MREPL
	for c in s:gmatch("%w") do
		f = f | (mntmap[c] | 0)
	end
	return f
end

--- bind(string, string, [string]) -> int
function p9.bind(name, over, flags)
	return raw.bind(name, over, parsemntflags(flags or ""))
end

--- mount(int | string, int, string, [string, [string]]) -> int
function p9.mount(fd, afd, over, flags, tree)
	if type(fd) == string then
		fd = p9.open(fd, "rw")
	end
	flags = parsemntflags(flags or "")
	return raw.mount(fd, afd or -1, over, flags, tree or "")
end

--- unmount(string, [string]) -> int
function p9.unmount(name, over)
	if over == nil then
		return raw.unmount(nil, name)
	end
	return raw.unmount(name, over)
end




-- Processes

local rforkmap <const> = {
	-- these are the same as in rc(1)
	["n"] = raw.RFNAMEG,
	["N"] = raw.RFCNAMEG,
	["e"] = raw.RFENVG,
	["E"] = raw.RFCENVG,
	["s"] = raw.RFNOTEG,
	["f"] = raw.RFFDG,
	["F"] = raw.RFCFDG,
	["m"] = raw.RFNOMNT,
	-- these are new
	["p"] = raw.RFPROC,
	["&"] = raw.RFNOWAIT,
	-- this is likely useless
	["r"] = raw.RFREND,
	-- this is gonna panic
	["M"] = raw.RFMEM,
}
--- rfork(string) -> int
function p9.rfork(flags)
	local f = 0
	for c in flags:gmatch("%w") do
		f = f | (rforkmap[c] or 0)
	end
	return raw.rfork(f)
end




-- File I/O
-- These are built on top of the raw API written in C and
-- where applicable they provide a more polished interface
-- with reasonable defaults.

local modemap <const> = {
	["r"] = raw.OREAD,
	["w"] = raw.OWRITE,
	["x"] = raw.OEXEC,
	["T"] = raw.OTRUNC,
	["C"] = raw.OCEXEC,
	["R"] = raw.ORCLOSE,
	["E"] = raw.OEXCL,
}
local function parsemode(s)
	local m, o, c
	
	m = 0;
	o = {r = false, w = false, x = false}
	for c in s:gmatch("%w") do
		if o[c] ~= nil then
			o[c] = true
		else
			m = m | (modemap[c] or 0)
		end
	end
	if o.r and o.w then
		m = m | raw.ORDWR
	elseif o.r then
		m = m | raw.OREAD
	elseif o.w then
		m = m | raw.OWRITE
	elseif o.x then
		m = m | raw.OEXEC
	end
	return m
end

local permmap <const> = {
	["d"] = raw.DMDIR,
	["a"] = raw.DMAPPEND,
	["e"] = raw.DMEXCL,
	["t"] = raw.DMTMP,
}
local function parseperm(s)
	local perm, m, p, c
	
	m, p = s:match("([daet]*)([0-7]*)")
	perm = tonumber(p, 8) or 0644
	for c in m:gmatch("%w") do
		perm = perm | (permmap[c] or 0)
	end
	return perm
end

--- open(string, string) -> int
function p9.open(file, ...)
	local mode = ...
	mode = parsemode(mode or "r")
	return raw.open(file, mode)
end

--- create(string, [string, [string]]) -> int
function p9.create(file, mode, perm)
	if not mode or #mode == 0 then mode = "rw" end
	if not perm or #perm == 0 then perm = "644" end
	mode = parsemode(mode)
	perm = parseperm(perm)
	if perm & raw.DMDIR then
		mode = mode & ~(raw.OWRITE)
	end
	return raw.create(file, mode, perm)
end

--- read(int, [int, [int]]) -> string
function p9.read(fd, n, off)
	return raw.read(fd, n or p9.iosize, off or -1)
end

--- slurp(int, [int]) -> string
function p9.slurp(fd, max)
	max = max or math.huge
	local tot, n = 0, 0
	local buf = {}
	while true do
		n = math.min(max - tot, p9.iosize)
		local r = p9.read(fd, n)
		if #r == 0 then
			break
		end
		buf[#buf + 1] = r
		tot = tot + #r
		if tot == max then
			break
		end
	end
	return table.concat(buf)
end

--- write(int, string, [int, [int]]) -> int
function p9.write(fd, buf, n, off)
	return raw.write(fd, buf, n or #buf, off or -1)
end

local whencemap <const> = {
	["set"] = 0,
	["cur"] = 1,
	["end"] = 2
}
--- seek(int, int, [string]) -> int
function p9.seek(fd, n, whence)
	whence = whence or "set"
	if whencemap[whence] == nil then
		error("whence must be one of [cur, set, end]")
	end
	return raw.seek(fd, n, whencemap[whence])
end

--- close(int) -> int
function p9.close(fd)
	return raw.close(fd)
end

--- remove(string) -> int
function p9.remove(file)
	return raw.remove(file)
end

--- fd2path(int) -> string
function p9.fd2path(fd)
	return raw.fd2path(fd)
end

--- pipe() -> int, int
function p9.pipe()
end



-- The File object

-- A file descriptor wrapper.

-- p9.file(fd) takes an open file descriptor and returns a
-- File object f which provides a convenient method interface
-- to the usual file operations.
-- p9.openfile and p9.createfile take a file name and open
-- or create a file. They accept the same arguments as
-- regular open and close.

-- The file descriptor stored in f.fd is garbage collected,
-- that is, it will be automatically closed once the File
-- object becomes unreachable. Note how this means that f.fd
-- should be used sparringly and with much care. In particular
-- you shouldn't store it outside of f, since the actual file
-- descriptor number might become invalid (closed) or refer
-- to a completely different file after f is collected.

-- The f.keep field can be set to true to prevent the finalizer
-- from closing the file.

local fileproto = {
	fd = -1,
	name = nil,
	path = nil,
	
	read = function(self, ...) return p9.read(self.fd, ...) end,
	write = function(self, ...) return p9.write(self.fd, ...) end,
	slurp = function(self, ...) return p9.slurp(self.fd, ...) end,
	seek = function(self, ...) return p9.seek(self.fd, ...) end,
	close = function(self)
		if self.fd == -1 then
			return
		end
		p9.close(self.fd)
		self.fd = -1
	end,
	
	__close = function(f, _)
		f:close()
	end
}

--- file(int, [string]) -> file{}
function p9.file(fd, name)
	local f = {}
	for k, v in pairs(fileproto) do
		f[k] = v
	end
	f.fd = fd
	f.name = name
	f.path = p9.fd2path(fd)
	return setmetatable(f, f)
end

-- openfile(string, ...) -> file{}
function p9.openfile(file, ...)
	return p9.file(p9.open(file, ...), file)
end

-- createfile(string, ...) -> file{}
function p9.createfile(file, ...)
	return p9.file(p9.create(file, ...), file)
end




-- Environment variables
--
-- p9.env object provides a map between the /env device and Lua,
-- with its dynamic fields representing the environment variables.
-- Assigning a value to the field writes to the environment:
--
-- 	p9.env.var = "value"
--
-- while reading a value reads from the environment:
--
-- 	assert(p9.env.var == "value")
--
-- A value can be a string or a list.
-- A list is encoded (decoded) to (from) the environment as a
-- list of strings according to the encoding used by the rc(1)
-- shell (0-byte separated fields).
--
--	lua> p9.env.list = {"a", "b", "c"}
--	rc> echo $#list -- $list
--	3 -- a b c
--
-- p9.getenv(name) and p9.setenv(name, val) provide the more
-- usual API.

--- getenv(string) -> string | list
function p9.getenv(key)
	typecheck(key, "string")
	
	local f, err = io.open("/env/" .. key)
	if err then
		return nil, err
	end
	local buf = f:read("a")
	f:close()
	-- a value
	if #buf == 0 or buf:sub(-1) ~= "\0" then
		return buf
	end
	-- a list (as encoded by rc(1) shell)
	local t, p = {}, 1
	while p <= #buf do
		t[#t + 1], p = string.unpack("z", buf, p)
	end
	return t
end

--- setenv(string, string | list)
function p9.setenv(key, val)
	local tk = typecheck(key, "string")
	local tv = typecheck(val, "nil string table")
	if val == nil then
		pcall(p9.remove, "/env/" .. key)
		return
	end
	local buf
	if tv == "string" then
		buf = val
	elseif tv == "table" then
		local t = {}
		for i = 1, #val do
			t[#t + 1] = string.pack("z", val[i])
		end
		buf = table.concat(t)
	end
	local f = assert(io.open("/env/" .. key, "w"))
	if not f:write(buf) then
		error("can't write environment")
	end
	f:close()
end

p9.env = setmetatable({}, {
	__index = function(_, key)
		return p9.getenv(key)
	end,
	__newindex = function(_, key, val)
		p9.setenv(key, val)
	end,
})




-- Lethal API
-- p9.lethal() returns a proxy table that is just like the
-- regular p9 module table, except that each function field
-- is wrapped such that any error raised during a call
-- results in program termination. An error is printed to
-- fd 2 and the same error is used for the program's exit status.
-- Note that this only mechanism only works for the fields
-- of the p9 module, like the p9.open and friends.
-- In particular, it doesn't apply to methods of File objects
-- nor to the p9.env object.
-- This limitation should be removed in the future.

local lethalproxy = setmetatable({}, {
	__index = function(_, key)
		if type(p9[key]) ~= "function" then
			return p9[key]
		end
		return function(...)
			local res = table.pack(pcall(p9[key], ...))
			if not res[1] then
				p9.write(2, string.format("%s: %s\n", arg[0] or "lua", res[2]))
				os.exit(res[2])
			end
			table.remove(res, 1)
			return table.unpack(res)
		end
	end
})

--- lethal() -> p9{}
function p9.lethal()
	return lethalproxy
end

return p9
