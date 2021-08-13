/* Nowhere in particular */

static int
p9_cleanname(lua_State *L)
{
	const char *path, *p;
	usize len;
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

static int
decode(lua_State *L, int base)
{
	const char *in;
	char *buf;
	usize insz, bufsz, sz;
	luaL_Buffer b;
	int (*dec)(uchar*, int, char*, int);
	
	in = luaL_checklstring(L, 1, &insz);
	buf = luaL_buffinitsize(L, &b, insz+1);
	switch(base){
	case 64:
		dec = dec64;
		bufsz = 6 * (insz + 3) / 8;
		break;
	case 32:
		dec = dec32;
		bufsz = 5 * (insz + 7) / 8;
		break;
	case 16:
		dec = dec16;
		bufsz = 4 * (insz + 1) / 8;
		break;
	default:
		return error(L, "unsupported base");
	}
	if((sz = dec((uchar*)buf, bufsz, in, insz)) == -1)
		return error(L, "dec: failed");
	luaL_pushresultsize(&b, sz);
	return 1;
}
static int p9_dec64(lua_State *L) { return decode(L, 64); }
static int p9_dec32(lua_State *L) { return decode(L, 32); }
static int p9_dec16(lua_State *L) { return decode(L, 16); }

static int
encode(lua_State *L, int base)
{
	const char *in;
	char *buf;
	usize insz, bufsz, sz;
	luaL_Buffer b;
	int (*enc)(char*, int, uchar*, int);
	
	in = luaL_checklstring(L, 1, &insz);
	switch(base){
	case 64:
		enc = enc64;
 		bufsz = (insz + 3) * 8 / 6;
		break;
	case 32:
		enc = enc32;
 		bufsz = (insz + 7) * 8 / 5;
		break;
	case 16:
		enc = enc16;
 		bufsz = (insz + 1) * 8 / 4;
		break;
	default:
		return error(L, "unsupported base");
	}
	buf = luaL_buffinitsize(L, &b, bufsz);
	if((sz = enc(buf, bufsz, (uchar*)in, insz)) == -1)
		return error(L, "enc: failed");
	luaL_pushresultsize(&b, sz);
	return 1;
}
static int p9_enc64(lua_State *L) { return encode(L, 64); }
static int p9_enc32(lua_State *L) { return encode(L, 32); }
static int p9_enc16(lua_State *L) { return encode(L, 16); }
