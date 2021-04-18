int
parsemntflags(lua_State *L, char *s)
{
	int flags, n;
	char *f[4], buf[128];
	
	flags = MREPL;
	n = getfields(s, f, sizeof f, 1, " \t\r\n");
	if(n > 0) for(int i = 0; i < n; i++){
		if     (strcmp(f[i], "after") == 0)
			flags |= MAFTER;
		else if(strcmp(f[i], "before") == 0)
			flags |= MBEFORE;
		else if(strcmp(f[i], "create") == 0)
			flags |= MCREATE;
		else if(strcmp(f[i], "cache") == 0)
			flags |= MCACHE;
		else{
			snprint(buf, sizeof buf, "skipping unknown mount flag '%s'", f[i]);
			lua_warning(L, buf, 0);
		}
	}
	return flags;
}

static int
p9_bind(lua_State *L)
{
	const char *this, *over;
	int flags, r;
	
	this = luaL_checkstring(L, 1);
	over = luaL_checkstring(L, 2);
	flags = parsemntflags(L, luaL_optstring(L, 3, ""));
	if((r = bind(this, over, flags)) == -1)
		return error(L, "bind: %r");
	lua_pushinteger(L, r);
	return 1;
}

static int
p9_mount(lua_State *L)
{
	const char *over, *aname;
	int fd, afd, closefd, flags, r;
	
	closefd = -1;
	switch(lua_type(L, 1)){
	default:
		return luaL_typeerror(L, 1, "file descriptor or path");
	case LUA_TNUMBER:
		fd = lua_tointeger(L, 1); break;
	case LUA_TSTRING:
		if((fd = open(lua_tostring(L, 1), ORDWR)) == -1)
			return error(L, "open: %r");
		closefd = fd;
		break;
	}
	over = luaL_checkstring(L, 2);
	flags = parsemntflags(L, luaL_optstring(L, 3, ""));
	aname = luaL_optstring(L, 4, "");
	afd = luaL_optinteger(L, 5, -1);
	if((r = mount(fd, afd, over, flags, aname)) == -1){
		close(closefd);
		return error(L, "mount: %r");
	}
	close(closefd);
	lua_pushinteger(L, r);
	return 1;
}

static int
p9_unmount(lua_State *L)
{
	const char *name, *over;
	int r;
	
	name = luaL_checkstring(L, 1);
	over = luaL_optstring(L, 2, nil);
	if(over == nil){
		over = name;
		name = nil;
	}
	if((r = unmount(name, over)) == -1)
		return error(L, "unmount: %r");
	lua_pushinteger(L, r);
	return 1;
}
