/*
 * The File object

 * p9.file(fd) takes an open file descriptor and returns a
 * File object f which provides a convenient method interface
 * to the usual file operations.
 * p9.open and p9.create take a file name to open or create.

 * The file descriptor stored in f.fd is garbage collected,
 * that is, it will be automatically closed once the File
 * object becomes unreachable. Note how this means that f.fd
 * should be used sparringly and with much care. In particular
 * you shouldn't store it outside of f, since the actual file
 * descriptor number might become invalid (closed) or refer
 * to a completely different file after f is collected.
 *
 * Store the File object in some global place to prevent it
 * from being collected.
 */
 
static int
openmode(lua_State *L, char *s)
{
	int i, n, mode;
	char r, w, x;
	char buf[64], *f[10], *p;
	
	snprint(buf, sizeof buf, "%s", s);
	mode = r = w = x = 0;
	n = getfields(buf, f, sizeof f, 1, " \t\n");
	if(n < 1)
		return OREAD;
	for(i = 0; p = f[i], i < n; i++){
		if(strcmp(p, "r") == 0 || strcmp(p, "read") == 0)
			r = 1;
		else if(strcmp(p, "w") == 0 || strcmp(p, "write") == 0)
			w = 1;
		else if(strcmp(p, "rw") == 0 || strcmp(p, "rdwr") == 0)
			r = w = 1;
		else if(strcmp(p, "x") == 0 || strcmp(p, "exec") == 0)
			x = 1;
		else if(strcmp(p, "trunc") == 0)
			mode |= OTRUNC;
		else if(strcmp(p, "cexec") == 0)
			mode |= OCEXEC;
		else if(strcmp(p, "rclose") == 0)
			mode |= ORCLOSE;
		else if(strcmp(p, "excl") == 0)
			mode |= OEXCL;
		else
			return luaL_error(L, "unknown mode flag '%s'", p);
	}
	if(x) mode |= OEXEC;
	else if(r && w) mode |= ORDWR;
	else if(r) mode |= OREAD;
	else if(w) mode |= OWRITE;
	return mode;
}

static ulong
createperm(lua_State *L, char *s)
{
	int i, n;
	ulong perm;
	char buf[64], *f[10], *p;
	
	snprint(buf, sizeof buf, "%s", s);
	perm = 0;
	n = getfields(buf, f, sizeof f, 1, " \t\n");
	if(n < 1)
		return 0644;
	for(i = 0; p = f[i], i < n; i++){
		if(isdigit(p[0]))
			perm |= strtol(p, nil, 8);
		else if(strcmp(p, "dir") == 0)
			perm |= DMDIR;
		else if(strcmp(p, "tmp") == 0)
			perm |= DMTMP;
		else if(strcmp(p, "excl") == 0)
			perm |= DMEXCL;
		else if(strcmp(p, "append") == 0)
			perm |= DMAPPEND;
		else
			return luaL_error(L, "unknown permission flag '%s'", p);
	}
	return perm;
}

static int filenew(lua_State*, int);
static int fileclose(lua_State*);
static int filefd(lua_State*, int);

static int
filenew(lua_State *L, int fd)
{
	int f;

	lua_createtable(L, 0, 4);
	f = lua_gettop(L);
	lua_pushinteger(L, fd);
		lua_setfield(L, f, "fd");
	luaL_getmetatable(L, "p9-File");
		lua_setfield(L, f, "__index");
	lua_pushcfunction(L, fileclose);
		lua_setfield(L, f, "__close");
	lua_pushcfunction(L, fileclose);
		lua_setfield(L, f, "__gc");
	lua_pushvalue(L, f);
		lua_setmetatable(L, f);
	return 1;
}

static int
fileclose(lua_State *L)
{
	int fd;
	
	fd = filefd(L, 1);
	if(fd == -1)
		return 0;
	lua_pushinteger(L, -1);
		lua_setfield(L, 1, "fd");
	close(fd);
	return 0;
}

static int
filefd(lua_State *L, int idx)
{
	int fd;
	
	if(lua_getfield(L, idx, "fd") != LUA_TNUMBER)
		return luaL_error(L, "fd must be integer");
	fd = lua_tonumber(L, -1);
	lua_pop(L, 1);
	return fd;
}

static int
p9_file(lua_State *L)
{
	int fd;
	
	fd = luaL_checkinteger(L, 1);
	return filenew(L, fd);
}

static int
p9_open(lua_State *L)
{
	const char *file;
	int mode;
	int fd;
	
	file = luaL_checkstring(L, 1);
	mode = openmode(L, luaL_optstring(L, 2, "read"));
	if((fd = open(file, mode)) == -1)
		return error(L, "open: %r");
	return filenew(L, fd);
}

static int
p9_create(lua_State *L)
{
	const char *file;
	int fd, mode;
	ulong perm;
	
	file = luaL_checkstring(L, 1);
	mode = openmode(L, luaL_optstring(L, 2, "rdwr"));
	perm = createperm(L, luaL_optstring(L, 3, "644"));
	if((fd = create(file, mode, perm)) == -1)
		return error(L, "create: %r");
	return filenew(L, fd);
}

static int
p9_close(lua_State *L)
{
	if(close(filefd(L, 1)) == -1)
		return error(L, "close: %r");
	return 0;
}

static int
seekmode(lua_State *L, char *s)
{
	if(strcmp(s, "set") == 0)
		return 0;
	if(strcmp(s, "cur") == 0)
		return 1;
	if(strcmp(s, "end") == 0)
		return 2;
	return luaL_error(L, "unknown seek mode '%s'", s);
}

static int
p9_seek(lua_State *L)
{
	int fd, type;
	vlong n, off;
	
	fd = filefd(L, 1);
	n = luaL_checkinteger(L, 2);
	type = seekmode(L, luaL_optstring(L, 3, "set"));
	if((off = seek(fd, n, type)) == -1)
		return error(L, "seek: %r");
	lua_pushinteger(L, off);
	return 1;
}

static int
p9_read(lua_State *L)
{
	int fd;
	long n, nbytes;
	vlong offset;
	char *buf;
	
	fd = filefd(L, 1);
	nbytes = luaL_optinteger(L, 2, Iosize);
	offset = luaL_optinteger(L, 3, -1);
	buf = getbuffer(L, nbytes);
	if(offset == -1)
		n = read(fd, buf, nbytes);
	else
		n = pread(fd, buf, nbytes, offset);
	if(n == -1)
		return error(L, "read: %r");
	lua_pushlstring(L, buf, n);
	return 1;
}

static int
slurp(lua_State *L, int fd, long nbytes)
{
	int all;
	long n, nr, tot;
	char *buf;
	luaL_Buffer b;
	
	all = (nbytes == -1) ? 1 : 0;
	luaL_buffinit(L, &b);
	for(tot = 0; all || tot < nbytes; tot += nr){
		n = all ? Iosize : min(Iosize, nbytes - tot);
		buf = luaL_prepbuffsize(&b, n);
		if((nr = read(fd, buf, n)) == -1)
			return error(L, "read: %r");
		if(nr == 0)
			break;
		luaL_addsize(&b, nr);
	}
	luaL_pushresult(&b);
	return 1;
}

static int
p9_slurp(lua_State *L)
{
	int fd;
	long nbytes;
	
	fd = filefd(L, 1);
	nbytes = luaL_optinteger(L, 2, -1);
	slurp(L, fd, nbytes);
	return 1;
}

static int
p9_write(lua_State *L)
{
	lua_Integer fd, offset;
	size_t nbytes;
	const char *buf;
	long n;

	fd = filefd(L, 1);
	buf = luaL_checklstring(L, 2, &nbytes);
	nbytes = luaL_optinteger(L, 3, nbytes);
	offset = luaL_optinteger(L, 4, -1);
	if(offset == -1)
		n = write(fd, buf, nbytes);
	else
		n = pwrite(fd, buf, nbytes, offset);
	if(n != nbytes)
		return error(L, "write: %r");
	lua_pushinteger(L, n);
	return 1;
}

static int
p9_remove(lua_State *L)
{
	const char *file;
	
	file = luaL_checkstring(L, 1);
	if(remove(file) == -1)
		return error(L, "remove: %r");
	lua_pushboolean(L, 1);
	return 1;
}

static int
p9_fd2path(lua_State *L)
{
	lua_Integer fd;
	char *buf;
	
	fd = luaL_checkinteger(L, 1);
	buf = getbuffer(L, Iosize);
	if(fd2path(fd, buf, Iosize) != 0)
		return error(L, "fd2path: %r");
	lua_pushstring(L, buf);
	return 1;
}

static int
p9_pipe(lua_State *L)
{
	int fd[2];
	
	if(pipe(fd) == -1)
		return error(L, "pipe: %r");
	filenew(L, fd[0]);
	filenew(L, fd[1]);
	return 2;
}
