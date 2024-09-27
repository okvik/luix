#include <u.h>
#include <libc.h>
#include <ctype.h>
#include <tos.h>

#include <lua.h>
#include <lauxlib.h>

#include "../base/common.c"

#include "fs.c"
#include "walk.c"
#include "env.c"
#include "ns.c"
#include "proc.c"
#include "misc.c"

static luaL_Reg p9_module[] = {
	{"open", p9_open},
	{"create", p9_create},
	{"pipe", p9_pipe},
	{"remove", p9_remove},
	{"access", p9_access},
	{"fd", nil}, /* table placeholder */
	
	{"stat", p9_stat},
	{"wstat", p9_wstat},
	{"walk", p9_walk},
	
	{"bind", p9_bind},
	{"mount", p9_mount},
	{"unmount", p9_unmount},
	
	{"getenv", p9_getenv},
	{"setenv", p9_setenv},
	{"env", nil}, /* table placeholder */
	
	{"abort", p9_abort},
	{"exits", p9_exits},
	{"fatal", p9_fatal},
	{"sleep", p9_sleep},
	{"alarm", p9_alarm},
	{"rfork", p9_rfork},
	{"wait", p9_wait},
	{"exec", p9_exec},
	{"wdir", p9_wdir},
	{"pid", p9_pid},
	{"ppid", p9_ppid},
	{"user", p9_user},
	{"sysname", p9_sysname},
	
	{"nanosec", p9_nanosec},
	{"nsec", p9_nsec},
	{"cleanname", p9_cleanname},
	
	{"enc64", p9_enc64},
	{"dec64", p9_dec64},
	{"enc32", p9_enc32},
	{"dec32", p9_dec32},
	{"enc16", p9_enc16},
	{"dec16", p9_dec16},
	
	{nil, nil}
};

int
luaopen_p9(lua_State *L)
{
	int lib;
	Buf *buf;
	
	luaL_newlib(L, p9_module);
	lib = lua_gettop(L);
	
	buf = resizebuffer(L, nil, Iosize);
	lua_pushlightuserdata(L, buf);
	lua_setfield(L, LUA_REGISTRYINDEX, "p9-buffer");
	
	luaL_newmetatable(L, "p9-File");
	luaL_setfuncs(L, p9_file_proto, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	
	/* Populate p9.fd[] with standard descriptors */
	lua_createtable(L, 3, 0);
	for(int i = 0; i < 3; i++){
		filenew(L, i);
		lua_rawseti(L, -2, i);
	}
	lua_setfield(L, lib, "fd");
	
	static luaL_Reg walkmt[] = {
		{"__close", p9_walkclose},
		{nil, nil},
	};
	luaL_newmetatable(L, "p9-Walk");
	luaL_setfuncs(L, walkmt, 0);
	
	static luaL_Reg envmt[] = {
		{"__index", p9_getenv_index},
		{"__newindex", p9_setenv_newindex},
		{nil, nil},
	};
	lua_createtable(L, 0, 2);
	luaL_setfuncs(L, envmt, 0);
	lua_pushvalue(L, -1);
	lua_setmetatable(L, -2);
	lua_setfield(L, lib, "env");
	
	lua_pushvalue(L, lib);
	return 1;
}
