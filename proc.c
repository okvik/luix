static int
p9_abort(lua_State*)
{
	abort();
	/* never */ return 0;
}

static int
p9_exits(lua_State *L)
{
	exits(luaL_optstring(L, 1, nil));
	/* never */ return 0;
}

static int
p9_fatal(lua_State *L)
{
	if(lua_gettop(L) > 1
	&& lua_getglobal(L, "string") == LUA_TTABLE
	&& lua_getfield(L, -1, "format") == LUA_TFUNCTION){
		lua_remove(L, -2);
		lua_insert(L, 1);
		if(lua_pcall(L, lua_gettop(L) - 1, 1, 0) == LUA_OK)
			sysfatal(lua_tostring(L, -1));
	}
	sysfatal(luaL_optstring(L, 1, "fatal"));
	/* never */ return 0;
}

static int
p9_sleep(lua_State *L)
{
	long t;
	
	t = luaL_checkinteger(L, 1);
	lua_pushboolean(L,
		sleep(t) == -1 ? 0 : 1
	);
	return 1;
}

static int
p9_alarm(lua_State *L)
{
	long t, rem;
	
	t = luaL_checkinteger(L, 1);
	rem = alarm(t);
	lua_pushinteger(L, rem);
	return 1;
}

static int
p9_wdir(lua_State *L)
{
	const char *path;
	char *buf;
	luaL_Buffer b;
	
	path = luaL_optstring(L, 1, nil);
	if(path != nil){
		if(chdir(path) == -1)
			return error(L, "chdir: %r");
		lua_pushboolean(L, 1);
		return 1;
	}
	luaL_buffinitsize(L, &b, Iosize);
	buf = luaL_buffaddr(&b);
	if(getwd(buf, Iosize) == nil)
		return error(L, "getwd: %r");
	luaL_pushresultsize(&b, strlen(buf));
	return 1;
}

static int
p9_user(lua_State *L)
{
	lua_pushstring(L, getuser());
	return 1;
}

static int
p9_sysname(lua_State *L)
{
	lua_pushstring(L, sysname());
	return 1;
}

static int
p9_pid(lua_State *L)
{
	lua_pushinteger(L, getpid());
	return 1;
}

static int
p9_ppid(lua_State *L)
{
	lua_pushinteger(L, getppid());
	return 1;
}

static int
p9_rfork(lua_State *L)
{
	int flags, i, n, r;
	char *f[12];
	
	flags = RFENVG|RFNAMEG|RFNOTEG;
	n = getfields(luaL_optstring(L, 1, ""), f, sizeof f, 0, " \t\n");
	if(n > 0) for(flags = 0, i = 0; i < n; i++){
		if     (strcmp(f[i], "name") == 0)
			flags |= RFNAMEG;
		else if(strcmp(f[i], "cname") == 0)
			flags |= RFCNAMEG;
		else if(strcmp(f[i], "env") == 0)
			flags |= RFENVG;
		else if(strcmp(f[i], "cenv") == 0)
			flags |= RFCENVG;
		else if(strcmp(f[i], "note") == 0)
			flags |= RFNOTEG;
		else if(strcmp(f[i], "fd") == 0)
			flags |= RFFDG;
		else if(strcmp(f[i], "cfd") == 0)
			flags |= RFCFDG;
		else if(strcmp(f[i], "nomnt") == 0)
			flags |= RFNOMNT;
		else if(strcmp(f[i], "proc") == 0)
			flags |= RFPROC;
		else if(strcmp(f[i], "nowait") == 0)
			flags |= RFNOWAIT;
		else if(strcmp(f[i], "rend") == 0)
			flags |= RFREND;
		else if(strcmp(f[i], "mem") == 0)
			flags |= RFMEM;
		else
			return luaL_error(L, "unknown rfork flag '%s'", f[i]);
	}
	if((r = rfork(flags)) == -1)
		return error(L, "rfork %r");
	lua_pushinteger(L, r);
	return 1;
}

static int
p9_exec(lua_State *L)
{
	int argc, i;
	const char **argv, *p;
	char buf[Smallbuf];
	
	argc = lua_gettop(L);
	if(argc < 1)
		luaL_argerror(L, 1, "string arguments expected");
	argv = lalloc(L, nil, (argc+1) * sizeof(char*));
	for(i = 1; i <= argc; i++)
		argv[i-1] = luaL_checkstring(L, i);
	argv[argc] = nil;
	p = argv[0];
	if(p[0] != '/' && (p[0] != '.' && p[1] != '/')){
		snprint(buf, sizeof buf, "/bin/%s", argv[0]);
		argv[0] = buf;
	}
	exec(argv[0], argv);
	free(argv);
	return error(L, "exec: %r");
}
