static int
p9_rfork(lua_State *L)
{
	lua_Integer flags;
	int r;
	
	flags = luaL_checkinteger(L, 1);
	if((r = rfork(flags)) == -1)
		lerror(L, "rfork");
	lua_pushinteger(L, r);
	return 1;
}
