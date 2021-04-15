static int
p9_bind(lua_State *L)
{
	const char *this, *over;
	lua_Integer flag;
	int r;
	
	this = luaL_checkstring(L, 1);
	over = luaL_checkstring(L, 2);
	flag = luaL_checkinteger(L, 3);
	if((r = bind(this, over, flag)) == -1)
		return error(L, "bind: %r");
	lua_pushinteger(L, r);
	return 1;
}

static int
p9_mount(lua_State *L)
{
	const char *over, *aname;
	lua_Integer fd, afd, flag, r;
	
	fd = luaL_checkinteger(L, 1);
	afd = luaL_checkinteger(L, 2);
	over = luaL_checkstring(L, 3);
	flag = luaL_checkinteger(L, 4);
	aname = luaL_checkstring(L, 5);
	if((r = mount(fd, afd, over, flag, aname)) == -1)
		return error(L, "mount: %r");
	lua_pushinteger(L, r);
	return 1;
}

static int
p9_unmount(lua_State *L)
{
	const char *name, *over;
	int r;
	
	name = luaL_optstring(L, 1, nil);
	over = luaL_checkstring(L, 2);
	if((r = unmount(name, over)) == -1)
		return error(L, "unmount: %r");
	lua_pushinteger(L, r);
	return 1;
}
