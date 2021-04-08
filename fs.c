static int
p9_open(lua_State *L)
{
	const char *file;
	int mode;
	int fd;
	
	file = luaL_checkstring(L, 1);
	mode = luaL_checkinteger(L, 2);
	if((fd = open(file, mode)) == -1)
		lerror(L, "open");
	lua_pushinteger(L, fd);
	return 1;
}

static int
p9_create(lua_State *L)
{
	const char *file;
	int fd, mode;
	ulong perm;
	
	file = luaL_checkstring(L, 1);
	mode = luaL_checkinteger(L, 2);
	perm = luaL_checkinteger(L, 3);
	if((fd = create(file, mode, perm)) == -1)
		lerror(L, "create");
	lua_pushinteger(L, fd);
	return 1;
}

static int
p9_close(lua_State *L)
{
	if(close(luaL_checkinteger(L, 1)) == -1)
		lerror(L, "close");
	return 0;
}

static int
p9_read(lua_State *L)
{
	lua_Integer fd, nbytes, offset;
	long n;
	char *buf;
	
	fd = luaL_checkinteger(L, 1);
	nbytes = luaL_checkinteger(L, 2);
	offset = luaL_optinteger(L, 3, -1);
	buf = getbuffer(L, nbytes);
	if(offset == -1)
		n = read(fd, buf, nbytes);
	else
		n = pread(fd, buf, nbytes, offset);
	if(n == -1)
		lerror(L, "read");
	lua_pushlstring(L, buf, n);
	return 1;
}

static int
p9_write(lua_State *L)
{
	lua_Integer fd, offset;
	size_t nbytes;
	const char *buf;
	long n;

	fd = luaL_checkinteger(L, 1);
	buf = luaL_checklstring(L, 2, &nbytes);
	nbytes = luaL_optinteger(L, 3, nbytes);
	offset = luaL_optinteger(L, 4, -1);
	if(offset == -1)
		n = write(fd, buf, nbytes);
	else
		n = pwrite(fd, buf, nbytes, offset);
	if(n != nbytes)
		lerror(L, "write");
	lua_pushinteger(L, n);
	return 1;
}

static int
p9_seek(lua_State *L)
{
	lua_Integer fd, n, type;
	vlong off;
	
	fd = luaL_checkinteger(L, 1);
	n = luaL_checkinteger(L, 2);
	type = luaL_checkinteger(L, 3);
	if((off = seek(fd, n, type)) == -1)
		lerror(L, "seek");
	lua_pushinteger(L, off);
	return 1;
}

static int
p9_remove(lua_State *L)
{
	const char *file;
	
	file = luaL_checkstring(L, 1);
	if(remove(file) == -1)
		lerror(L, "remove");
	lua_pushboolean(L, 1);
	return 1;
}

static int
p9_fd2path(lua_State *L)
{
	lua_Integer fd;
	char *buf;
	
	fd = luaL_checkinteger(L, 1);
	buf = getbuffer(L, 8192);
	if(fd2path(fd, buf, 8192) != 0)
		lerror(L, "fd2path");
	lua_pushstring(L, buf);
	return 1;
}
