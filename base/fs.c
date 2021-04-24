/*
 * The File object

 * p9.open and p9.create create a file object representing an
 * open file descriptor.  This object's methods are then used
 * for performing I/O and other operations.
 *
 * The file object holds a reference to a system resource, the
 * open file descriptor, which you likely want to release as
 * soon as it's not needed anymore.
 * To help with that the file implements both __gc and __close
 * metamethods, which will close it for you as soon as the file
 * falls out of reach.
 * file:close() will close the file descriptor immediately.
 * file:keep() will prevent automatic closing of the file
 * descriptor, which may be needed in some situations.
 *
 * During module load the standard file descriptors 0 through 2
 * are wrapped in File objects and get stashed in p9.fd table:
 * p9.fd[0] thus refers to the standard input, and so on.
 * Being reachable through the module these aren't subject to
 * garbage collection -- at least until Lua state is closed.
 * You may use this table for stashing files for similar reasons.
 * TODO: use this table to implement access to arbitrary
 * already-open file descriptors.
 */
 
typedef struct File File;

struct File {
	int fd;
	int keep;
};

static int
filenew(lua_State *L, int fd)
{
	File *f;
	
	f = lua_newuserdatauv(L, sizeof(File), 0);
	f->fd = fd;
	f->keep = 0;
	luaL_setmetatable(L, "p9-File");
	return 1;
}

static int
fileclose(lua_State *L)
{
	File *f;
	
	f = luaL_checkudata(L, 1, "p9-File");
	if(f->keep || f->fd == -1)
		return 0;
	if(close(f->fd) == -1)
		return error(L, "close: %r");
	f->fd = -1;
	return 0;
}

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
p9_pipe(lua_State *L)
{
	int fd[2];
	
	if(pipe(fd) == -1)
		return error(L, "pipe: %r");
	filenew(L, fd[0]);
	filenew(L, fd[1]);
	return 2;
}

static int
p9_file_close(lua_State *L)
{
	File *f;
	
	f = luaL_checkudata(L, 1, "p9-File");
	if(close(f->fd) == -1)
		return error(L, "close: %r");
	f->fd = -1;
	return 0;
}

static int
p9_file_keep(lua_State *L)
{
	File *f;
	
	f = luaL_checkudata(L, 1, "p9-File");
	f->keep = 1;
	return 1;
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
p9_file_seek(lua_State *L)
{
	File *f;
	int type;
	vlong n, off;
	
	f = luaL_checkudata(L, 1, "p9-File");
	n = luaL_checkinteger(L, 2);
	type = seekmode(L, luaL_optstring(L, 3, "set"));
	if((off = seek(f->fd, n, type)) == -1)
		return error(L, "seek: %r");
	lua_pushinteger(L, off);
	return 1;
}

static int
p9_file_read(lua_State *L)
{
	File *f;
	long n, nbytes;
	vlong offset;
	char *buf;
	
	f = luaL_checkudata(L, 1, "p9-File");
	nbytes = luaL_optinteger(L, 2, Iosize);
	offset = luaL_optinteger(L, 3, -1);
	buf = getbuffer(L, nbytes);
	if(offset == -1)
		n = read(f->fd, buf, nbytes);
	else
		n = pread(f->fd, buf, nbytes, offset);
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
p9_file_slurp(lua_State *L)
{
	File *f;
	long nbytes;
	
	f = luaL_checkudata(L, 1, "p9-File");
	nbytes = luaL_optinteger(L, 2, -1);
	slurp(L, f->fd, nbytes);
	return 1;
}

static int
p9_file_write(lua_State *L)
{
	File *f;
	lua_Integer offset;
	size_t nbytes;
	const char *buf;
	long n;

	f = luaL_checkudata(L, 1, "p9-File");
	buf = luaL_checklstring(L, 2, &nbytes);
	nbytes = luaL_optinteger(L, 3, nbytes);
	offset = luaL_optinteger(L, 4, -1);
	if(offset == -1)
		n = write(f->fd, buf, nbytes);
	else
		n = pwrite(f->fd, buf, nbytes, offset);
	if(n != nbytes)
		return error(L, "write: %r");
	lua_pushinteger(L, n);
	return 1;
}

static int
p9_file_path(lua_State *L)
{
	File *f;
	char *buf;
	
	f = luaL_checkudata(L, 1, "p9-File");
	buf = getbuffer(L, Iosize);
	if(fd2path(f->fd, buf, Iosize) != 0)
		return error(L, "fd2path: %r");
	lua_pushstring(L, buf);
	return 1;
}

static int
p9_file_iounit(lua_State *L)
{
	File *f;
	
	f = luaL_checkudata(L, 1, "p9-File");
	lua_pushinteger(L, iounit(f->fd));
	return 1;
}

static int
p9_file_dup(lua_State *L)
{
	int fd;
	File *f;
	
	f = luaL_checkudata(L, 1, "p9-File");
	if((fd = dup(f->fd, -1)) == -1)
		return error(L, "dup: %r");
	return filenew(L, fd);
}

static int
p9_file_set(lua_State *L)
{
	int fd;
	File *this, *that;
	
	this = luaL_checkudata(L, 1, "p9-File");
	that = luaL_checkudata(L, 2, "p9-File");
	if((fd = dup(that->fd, this->fd)) == -1)
		return error(L, "dup: %r");
	this->fd = fd;
	lua_pushvalue(L, 1);
	return 1;
}

static luaL_Reg p9_file_proto[] = {
	{"close", p9_file_close},
	{"keep", p9_file_keep},
	{"read", p9_file_read},
	{"slurp", p9_file_slurp},
	{"write", p9_file_write},
	{"seek", p9_file_seek},
	{"iounit", p9_file_iounit},
	{"path", p9_file_path},
	{"dup", p9_file_dup},
	{"set", p9_file_set},
	
	{"__gc", fileclose},
	{"__close", fileclose},
	{"__index", nil}, /* placeholder for ourselves */
	
	{nil, nil},
};


/*
 * Assorted functions that don't work with or create Files
 */

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
accessmode(lua_State *L, const char *s)
{
	int i, n, mode;
	char buf[64], *f[10], *p;
	
	snprint(buf, sizeof buf, "%s", s);
	n = getfields(buf, f, sizeof f, 1, " \t\n");
	mode = 0;
	for(i = 0; p = f[i], i < n; i++){
		if(strcmp(p, "exist") == 0 || strcmp(p, "exists") == 0)
			mode |= AEXIST;
		else if(strcmp(p, "r") == 0 || strcmp(p, "read") == 0)
			mode |= AREAD;
		else if(strcmp(p, "w") == 0 || strcmp(p, "write") == 0)
			mode |= AWRITE;
		else if(strcmp(p, "rw") == 0 || strcmp(p, "rdwr") == 0)
			mode |= AREAD|AWRITE;
		else if(strcmp(p, "x") == 0 || strcmp(p, "exec") == 0)
			mode |= AEXEC;
		else
			return luaL_error(L, "unknown access flag '%s'", p);
	}
	return mode;
}

static int
p9_access(lua_State *L)
{
	const char *path;
	int mode;

	path = luaL_checkstring(L, 1);
	mode = accessmode(L, luaL_optstring(L, 2, "exists"));
	lua_pushboolean(L, 
		access(path, mode) == 0 ? 1 : 0
	);
	return 1;
}
