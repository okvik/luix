-- LuaFileSystem API emulation
-- https://keplerproject.github.io/luafilesystem/manual.html#reference

local lfs = {}
local p9 = require "p9"

local function nope()
	return nil, "unimplemented"
end
lfs.lock = nope
lfs.unlock = nope
lfs.lock_dir = nope
lfs.link = nope

-- st:next() and st:close() missing.
function lfs.dir(path)
	local witer, wstate = p9.walk(path)
	local iter = function(st)
		local r = witer(st)
		return r and r.name or nil
	end
	return iter, wstate, nil, wstate
end

function lfs.attributes(path, t)
	local dir, err = p9.stat(path)
	if not dir then
		return nil, err, 999
	end
	local r = type(t) == "table" and t or {}
	r.dev = dir.type
	r.rdev = dir.type
	r.ino = dir.dev
	r.mode = dir.mode.file and "file" or "directory"
	r.nlink = 0
	r.uid = 0
	r.gid = 0
	r.access = dir.atime
	r.modification = dir.mtime
	r.change = dir.mtime
	r.size = dir.length
	r.permissions = dir.perm -- TODO check if formats match
	if type(t) == "string" then
		return r[t]
	end
	return r
end
lfs.symlinkattributes = lfs.attributes

function lfs.chdir(path)
	return p9.wdir(path)
end

function lfs.currentdir()
	return p9.wdir()
end

function lfs.mkdir(path)
	local f, err = p9.create(path, "read", "dir 755")
	if not f then return nil, err, 999 end
	f:close()
	return true
end

function lfs.rmdir(path)
	local ok, err = p9.remove(path)
	return ok and true or nil, err, 999
end

-- Access time may not be modified on Plan 9
function lfs.touch(path, mtime)
	local ok, err = p9.wstat(path, {
		mtime = mtime or os.time()
	})
	return ok and true or nil, err, 999
end

function lfs.setmode()
	return "binary"
end

return lfs
