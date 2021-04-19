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
