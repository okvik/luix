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

/* Taken from treason(1), thanks sigrid */
static uvlong
nanosec(void)
{
	static uvlong fasthz, xstart;
	uvlong x, div;

	if(fasthz == ~0ULL)
		return nsec() - xstart;

	if(fasthz == 0){
		fasthz = _tos->cyclefreq;
		if(fasthz == 0){
			fasthz = ~0ULL;
			xstart = nsec();
		}else{
			cycles(&xstart);
		}
		return 0;
	}
	cycles(&x);
	x -= xstart;

	/* this is ugly */
	for(div = 1000000000ULL; x < 0x1999999999999999ULL && div > 1 ; div /= 10ULL, x *= 10ULL);

	return x / (fasthz / div);
}

static int
p9_nanosec(lua_State *L)
{
	lua_pushinteger(L, nanosec());
	return 1;
}

static int
p9_nsec(lua_State *L)
{
	lua_pushinteger(L, nsec());
	return 1;
}
