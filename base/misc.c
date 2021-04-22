/* Nowhere in particular */

static int
p9_cleanname(lua_State *L)
{
	const char *path, *p;
	lua_Unsigned len;
	luaL_Buffer b;
	
	path = luaL_checklstring(L, 1, &len);
	luaL_buffinit(L, &b);
	luaL_addlstring(&b, path, len);
	luaL_addchar(&b, '\0');
	p = cleanname(luaL_buffaddr(&b));
	lua_pushstring(L, p);
	return 1;
}
