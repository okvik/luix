enum {
	Iosize = 8192,
	Smallbuf = 512,
};

#define min(a, b) ((a) < (b) ? (a) : (b))

static int
error(lua_State *L, char *fmt, ...)
{
	va_list varg;
	int n;
	char *buf;
	luaL_Buffer b;
	
	lua_pushnil(L);
	buf = luaL_buffinitsize(L, &b, Smallbuf);
	va_start(varg, fmt);
	n = vsnprint(buf, Smallbuf, fmt, varg);
	va_end(varg);
	luaL_pushresultsize(&b, n);
	return 2;
}

/* Memory allocator associated with Lua state */
static void*
Lallocf(lua_State *L, void *ptr, usize sz)
{
	void *ud;
	
	if((ptr = (lua_getallocf(L, &ud))(ud, ptr, LUA_TUSERDATA, sz)) == nil){
		lua_pushliteral(L, "out of memory");
		lua_error(L);
	}
	setmalloctag(ptr, getcallerpc(&L));
	return ptr;
}
#define Lrealloc(L, ptr, sz) Lallocf(L, ptr, sz);
#define Lmalloc(L, sz) Lallocf(L, nil, sz)
#define Lfree(L, ptr) Lallocf(L, nil, 0)

/* 
 * Various functions in this library require a
 * variably sized buffer for their operation.
 * Rather than allocating one for each call
 * we preallocate a shared buffer of reasonable
 * size and grow it as needed.
 * The buffer gets associated with a Lua state
 * at library load time.
 * getbuffer(L, sz) returns a pointer to the
 * memory area of at least sz bytes.
 * 
 * To avoid stepping on each other's toes the
 * buffer use must be constrained to a single
 * call.
 */

typedef struct Buf Buf;

struct Buf {
	usize sz;
	char b[1];
};

static Buf*
resizebuffer(lua_State *L, Buf *buf, usize sz)
{
	if(buf == nil){
		buf = Lmalloc(L, sizeof(Buf) + sz);
		buf->sz = sz;
	}else if(buf->sz < sz){
		Lfree(L, buf);
		return resizebuffer(L, nil, sz);
	}
	return buf;
}

static char*
getbuffer(lua_State *L, usize sz)
{
	Buf *buf;
	
	lua_getfield(L, LUA_REGISTRYINDEX, "p9-buffer");
	buf = lua_touserdata(L, -1);
	return resizebuffer(L, buf, sz)->b;
}
