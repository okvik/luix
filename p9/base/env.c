/* Environment variables
 *
 * p9.env object provides a map between the /env device and Lua,
 * with its dynamic fields representing the environment variables.
 * Assigning a value to the field writes to the environment:
 *
 * 	p9.env.var = "value"
 *
 * while reading a value reads from the environment:
 *
 * 	assert(p9.env.var == "value")
 *
 * A value can be a string or a list.
 * A list is encoded (decoded) to (from) the environment as a
 * list of strings according to the encoding used by the rc(1)
 * shell (0-byte separated fields).
 *
 *	lua> p9.env.list = {"a", "b", "c"}
 *	rc> echo $#list  * $list
 *	3  * a b c
 *
 * p9.getenv(name) and p9.setenv(name, val) provide the more
 * usual API.
 */

static int
p9_getenv(lua_State *L)
{
	int fd;
	long len, elems, i;
	char env[Smallbuf];
	const char *buf, *s, *e;
	
	snprint(env, sizeof env, "/env/%s", luaL_checkstring(L, 1));
	if((fd = open(env, OREAD)) == -1){
		lua_pushnil(L);
		return 1;
	}
	slurp(L, fd, -1);
	close(fd);
	len = luaL_len(L, -1);
	buf = lua_tostring(L, -1);
	/* Empty */
	if(len == 0){
		lua_pushnil(L);
		return 1;
	}
	/* String (not a list) */
	if(buf[len-1] != '\0')
		return 1;
	/* List */
	for(elems = i = 0; i < len; i++)
		if(buf[i] == '\0') elems++;
	s = buf;
	e = buf + len;
	lua_createtable(L, elems, 0);
	for(i = 1; s < e; i++){
		lua_pushstring(L, s);
		lua_rawseti(L, -2, i);
		s = memchr(s, '\0', e - s);
		s++;
	}
	return 1;
}

static int
p9_getenv_index(lua_State *L)
{
	lua_remove(L, 1);
	return p9_getenv(L);
}

static int
p9_setenv(lua_State *L)
{
	int fd, ntab, n, t, i;
	char env[Smallbuf];
	luaL_Buffer b;
	
	snprint(env, sizeof env, "/env/%s", luaL_checkstring(L, 1));
	t = lua_type(L, 2);
	if(t != LUA_TNIL && t != LUA_TSTRING && t != LUA_TTABLE)
		return luaL_argerror(L, 2, "nil, string, or table expected");
	if((fd = create(env, OWRITE, 0644)) == -1)
		return error(L, "open: %r");
	/*
	 * Writes below are not fully checked for
	 * error (write(n) != n), because env(3)
	 * may truncate the value at its dumb 16k
	 * limit. Encoding a knowledge of this limit
	 * sucks, as does env(3).
	 */
	if(t == LUA_TNIL){
		close(fd);
		return 0;
	}else if(t == LUA_TSTRING){
		n = luaL_len(L, 2);
		if(write(fd, lua_tostring(L, 2), n) == -1){
			close(fd);
			return error(L, "write: %r");
		}
	}else{
		ntab = luaL_len(L, 2);
		luaL_buffinit(L, &b);
		for(i = 1; i <= ntab; i++){
			t = lua_geti(L, 2, i);
			if(t != LUA_TSTRING){
				if(luaL_callmeta(L, -1, "__tostring")
				&& lua_type(L, -1) == LUA_TSTRING){
					lua_replace(L, -2);
				}else{
					lua_pop(L, 1);
					continue;
				}
			}
			luaL_addvalue(&b);
			luaL_addchar(&b, '\0');
		}
		if(write(fd, luaL_buffaddr(&b), luaL_bufflen(&b)) == -1){
			close(fd);
			return error(L, "write: %r");
		}
	}
	close(fd);
	return 0;
}

static int
p9_setenv_newindex(lua_State *L)
{
	lua_remove(L, 1);
	return p9_setenv(L);
}
