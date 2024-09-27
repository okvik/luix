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

static int
p9_wstat(lua_State *L)
{
	static int createperm(lua_State*, char*); /* from fs.c */
	const char *path, *k;
	Dir new;
	
	path = luaL_checkstring(L, 1);
	luaL_argexpected(L, lua_type(L, 2) == LUA_TTABLE, 2, "table");
	nulldir(&new);
	lua_pushnil(L);
	while(lua_next(L, -2)){
		k = lua_tostring(L, -2);
		if(strcmp(k, "name") == 0)
			new.name = (char*)lua_tostring(L, -1); /* dw */
		else if(strcmp(k, "mode") == 0)
			new.mode = createperm(L, lua_tostring(L, -1));
		else if(strcmp(k, "mtime") == 0)
			new.mtime = lua_tointeger(L, -1);
		else if(strcmp(k, "gid") == 0)
			new.gid = (char*)lua_tostring(L, -1);
		else if(strcmp(k, "length") == 0)
			new.length = lua_tointeger(L, -1);
		lua_pop(L, 1);
	}
	if(dirwstat(path, &new) == -1)
		return error(L, "wstat: %r");
	lua_pushboolean(L, 1);
	return 1;
}
