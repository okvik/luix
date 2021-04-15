static int
p9_open(lua_State *L)
{
	const char *file;
	int mode;
	int fd;
	
	file = luaL_checkstring(L, 1);
	mode = luaL_checkinteger(L, 2);
	if((fd = open(file, mode)) == -1)
		return error(L, "open: %r");
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
		return error(L, "create: %r");
	lua_pushinteger(L, fd);
	return 1;
}

static int
p9_close(lua_State *L)
{
	if(close(luaL_checkinteger(L, 1)) == -1)
		return error(L, "close: %r");
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
		return error(L, "read: %r");
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
		return error(L, "write: %r");
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
		return error(L, "seek: %r");
	lua_pushinteger(L, off);
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
	buf = getbuffer(L, 8192);
	if(fd2path(fd, buf, 8192) != 0)
		return error(L, "fd2path: %r");
	lua_pushstring(L, buf);
	return 1;
}

static char*
perms(int p, char *buf)
{
	buf[0] = p & 04 ? 'r' : '-';
	buf[1] = p & 02 ? 'w' : '-';
	buf[2] = p & 01 ? 'x' : '-';
	return buf;
}

static void
createdirtable(lua_State *L, Dir *d)
{
	#define set(t, k, v) do { \
		lua_pushstring(L, (k)); \
		lua_push##t(L, (v)); \
		lua_rawset(L, -3); \
	} while(0)
	
	lua_createtable(L, 0, 11);
	set(integer, "type", d->type);
	set(integer, "dev", d->dev);
	set(integer, "atime", d->atime);
	set(integer, "mtime", d->mtime);
	set(integer, "length", d->length);
	set(string, "name", d->name);
	set(string, "uid", d->uid);
	set(string, "gid", d->gid);
	set(string, "muid", d->muid);
	
	lua_pushstring(L, "qid");
	lua_createtable(L, 0, 3);
	set(integer, "path", d->qid.path);
	set(integer, "vers", d->qid.vers);
	set(integer, "type", d->qid.type);
	lua_rawset(L, -3);
	
	lua_pushstring(L, "mode");
	lua_createtable(L, 0, 7);
	ulong m = d->mode;
	set(integer, "raw", m);
	if(m & DMDIR)
		set(boolean, "dir", 1);
	else
		set(boolean, "file", 1);
	if(m & DMAPPEND)
		set(boolean, "append", 1);
	if(m & DMTMP)
		set(boolean, "tmp", 1);
	if(m & DMMOUNT)
		set(boolean, "mount", 1);
	if(m & DMAUTH)
		set(boolean, "auth", 1);
	char buf[10] = {0};
	set(string, "user", perms((m & 0700) >> 6, buf));
	set(string, "group", perms((m & 0070) >> 3, buf+3));
	set(string, "other", perms((m & 0007) >> 0, buf+6));
	set(string, "perm", buf);
	lua_rawset(L, -3);
	
	#undef set
}

static int
p9_stat(lua_State *L)
{
	Dir *d;
	
	d = nil;
	switch(lua_type(L, 1)){
	default:
		USED(d);
		return luaL_typeerror(L, 1, "string or number");
	case LUA_TSTRING:
		d = dirstat(lua_tostring(L, 1)); break;
	case LUA_TNUMBER:
		d = dirfstat(lua_tonumber(L, 1)); break;
	}
	if(d == nil)
		return error(L, "stat: %r");
	createdirtable(L, d);
	free(d);
	return 1;
}

typedef struct Walk {
	int fd;
	int nleft;
	Dir *dirs, *p;
} Walk;

static int
p9_walk(lua_State *L)
{
	static int p9_walkout(lua_State*);
	static int p9_walknext(lua_State*);
	int nargs, wstate;
	Dir *d;
	Walk *w;
	
	nargs = lua_gettop(L);
	w = lua_newuserdatauv(L, sizeof(Walk), 1);
	wstate = nargs + 1;
	w->fd = -1;
	w->nleft = 0;
	w->dirs = w->p = nil;
	luaL_setmetatable(L, "p9-Walk");
	if(nargs == 2){
		lua_pushvalue(L, 2);
		lua_setiuservalue(L, wstate, 1);
	}
	if(lua_isnumber(L, 1))
		w->fd = lua_tointeger(L, 1);
	else{
		if((w->fd = open(luaL_checkstring(L, 1), OREAD|OCEXEC)) == -1){
			error(L, "open: %r");
			goto Error;
		}
	}
	if((d = dirfstat(w->fd)) == nil){
		error(L, "stat: %r");
		goto Error;
	}
	int isdir = d->mode & DMDIR;
	free(d);
	if(!isdir){
		error(L, "walk in a non-directory");
		goto Error;
	}
	/* return p9_walknext, p9-Walk, nil, p9-Walk */
	lua_pushcfunction(L, p9_walknext);
	lua_pushvalue(L, wstate);
	lua_pushnil(L);
	lua_pushvalue(L, wstate);
	return 4;
Error:
	if(nargs == 2){
		lua_setfield(L, 2, "error");
		lua_pushcfunction(L, p9_walkout);
		return 1;
	}
	return lua_error(L);
}

static int
p9_walkout(lua_State*)
{
	return 0;
}

static int
p9_walknext(lua_State *L)
{
	Walk *w;
	Dir *d;
	
	w = luaL_checkudata(L, 1, "p9-Walk");
	if(w->nleft == 0){
		if(w->dirs != nil){
			free(w->dirs);
			w->dirs = nil;
		}
		if((w->nleft = dirread(w->fd, &w->dirs)) == -1){
			error(L, "dirread: %r");
			goto Error;
		}
		w->p = w->dirs;
		if(w->nleft == 0)
			return 0; /* Last Walk state will be closed */
	}
	w->nleft--;
	d = w->p++;
	createdirtable(L, d);
	return 1;
Error:
	if(lua_getiuservalue(L, 1, 1) == LUA_TTABLE){
		lua_pushvalue(L, -2);
		lua_setfield(L, -2, "error");
		lua_pushnil(L);
		return 1;
	}
	lua_pop(L, 1);
	return 2;
}

static int
p9_walkclose(lua_State *L)
{
	Walk *w;
	
	w = luaL_checkudata(L, 1, "p9-Walk");
	if(w->dirs != nil){
		free(w->dirs);
		w->dirs = nil;
	}
	if(w->fd != -1){
		close(w->fd);
		w->fd = -1;
	}
	return 0;
}

