#include <u.h>
#include <libc.h>

#include <lua.h>
#include <lauxlib.h>

#include "../base/common.c"

/*
 * The following global state designates the Lua state
 * responsible for registering and running note handler
 * functions -- the one (and only) loading this module.
 * Additionally the note itself is communicated to the
 * postponed handler here due to most of Lua API not
 * being safe to use in a note handler context.
 * 
 * This global state and nondeterministic nature of
 * postponed handling of notes means this module should
 * be used with care and will likely need to be heavily
 * adapted for use in any but the simplest of hosts.
 * luix standalone interpreter is an example of a simple
 * program with a single Lua state and Lua code being
 * "the boss", that is, the note is very likely to
 * interrupt a Lua VM rather than host code, and if not
 * the VM will be entered shortly after. This lets
 * postponed note handlers run relatively close to
 * the actual note event.
 * In more involved programs, perhaps running multiple
 * separate Lua states, or spending more time in the
 * host, the postponed handlers may run only as soon
 * as the designated handler state gets a chance to
 * run, if at all.
 * 
 * In short, consider alternatives to Lua code doing
 * any direct note handling.
 * 
 * TODO: the note state, catcher and exit functions,
 * and notify registration should all be left for the
 * host to set up.
 */

typedef struct Note Note;

struct Note {
	lua_State *L;
	char note[ERRMAX+1];
};

static Note notestate;

/* 
 * Acks the note so it can be handled outside note context
 * but only after the possibly interrupted Lua state
 * stabilizes. This is done by registering an instruction
 * hook and running handler functions inside it.
 * Note that another note may come just as we are doing
 * this. We do nothing about it currently: the newer
 * notes simply interrupt currently executing handlers.
 * One solution is to queue incoming notes and handle
 * all of them in order.
 * Also note that this catcher always acknowledges the
 * note, preventing any other catchers registered after
 * it from ever seeing the note. Therefore you most
 * likely want it to be the last or only note handler.
 */
static int
notecatcher(void*, char *note)
{
	static void noterunner(lua_State*, lua_Debug*);
	
	lua_sethook(notestate.L, noterunner,
		LUA_MASKCALL|LUA_MASKRET|LUA_MASKCOUNT, 1);
	strncpy(notestate.note, note, ERRMAX);
	return 1;
}

static void
noterunner(lua_State *L, lua_Debug*)
{
	int n, i;
	
	lua_sethook(notestate.L, nil, 0, 0);
	if(lua_getfield(L, LUA_REGISTRYINDEX, "p9-note-handlers") != LUA_TTABLE)
		luaL_error(L, "missing note handlers table");
	if((n = lua_rawlen(L, -1)) == 0)
		return;
	for(i = 1; i <= n; i++){
		lua_rawgeti(L, -1, i); /* handler(note) */
		lua_pushstring(L, notestate.note);
		if(lua_pcall(L, 1, 1, 0) != LUA_OK)
			break;
		if(lua_toboolean(L, -1) == 1)
			return; /* to where we got interrupted */
		lua_pop(L, 1);
	}
	/* Emulate kernel handling of unacknowledged note. */
	if(strncmp(notestate.note, "sys:", 4) == 0)
		abort();
	exits(notestate.note); /* threadexitsall when using thread.h */
}

static int
noteset(lua_State *L)
{
	if(lua_getfield(L, LUA_REGISTRYINDEX, "p9-note-handlers") != LUA_TTABLE)
		return luaL_error(L, "missing note handlers table");
	lua_insert(L, -2);
	lua_rawseti(L, -2, lua_rawlen(L, -2) + 1);
	lua_pop(L, 1);
	return 0;
}

static int
noteunset(lua_State *L)
{
	int n, pos, fn, t;
	
	fn = lua_gettop(L);
	if(lua_getfield(L, LUA_REGISTRYINDEX, "p9-note-handlers") != LUA_TTABLE)
		return luaL_error(L, "missing note handlers table");
	t = fn + 1;
	n = lua_rawlen(L, t);
	for(pos = 1; pos <= n; pos++){
		lua_rawgeti(L, t, pos);
		if(lua_rawequal(L, fn, -1))
			goto remove;
		lua_pop(L, 1);
	}
	lua_pop(L, 2);
	return 0;
remove:
	lua_pop(L, 1);
  for ( ; pos < n; pos++) {
    lua_rawgeti(L, t, pos + 1);
    lua_rawseti(L, t, pos);
  }
  lua_pushnil(L);
  lua_rawseti(L, t, pos);
  lua_pop(L, 2);
  return 0;
}

static int
p9_note_catch(lua_State *L)
{
	int fn;
	const char *arg;
	
	fn = lua_gettop(L);
	luaL_argexpected(L, fn > 0, 1, "function expected");
	if(fn == 1)
		arg = "set";
	else
		arg = luaL_checkstring(L, 1);
	luaL_argexpected(L,
		lua_type(L, fn) == LUA_TFUNCTION, fn, "function");
	if(strcmp(arg, "set") == 0)
		return noteset(L);
	else if(strcmp(arg, "unset") == 0)
		return noteunset(L);
	return luaL_error(L, "'set' or 'unset' expected");
}

static int
p9_note_post(lua_State *L)
{
	int pid, w;
	const char *who, *note;
	
	who = luaL_checkstring(L, 1);
	if(strcmp(who, "proc") == 0)
		w = PNPROC;
	else if(strcmp(who, "group") == 0)
		w = PNGROUP;
	else
		return luaL_argerror(L, 1, "expected 'proc' or 'group'");
	pid = luaL_checkinteger(L, 2);
	note = luaL_checkstring(L, 3);
	if(postnote(w, pid, note) == -1)
		return error(L, "postnote: %r");
	lua_pushboolean(L, 1);
	return 1;
}

static luaL_Reg p9_note_module[] = {
	{"post", p9_note_post},
	{"catch", p9_note_catch},
	{nil, nil},
};

int
luaopen_p9_note(lua_State *L)
{
	/* Only one Lua state may work with notes */
	if(notestate.L != nil)
		return 0;
	notestate.L = L;
	
	lua_createtable(L, 1, 0);
	lua_setfield(L, LUA_REGISTRYINDEX, "p9-note-handlers");
	
	luaL_newlib(L, p9_note_module);
	
	atnotify(notecatcher, 1);
	return 1;
}
