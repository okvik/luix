#include <u.h>
#include <libc.h>
#include <tos.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

static void
dumpstack(lua_State *L)
{
	for(int i = 1; i <= lua_gettop(L); i++){
		print("%d: %s\n", i, luaL_tolstring(L, i, nil));
		lua_pop(L, 1);
	}
}

static int
os_exit(lua_State *L){
	const char *status;
	
	status = nil;
	switch(lua_type(L, 1)){
	case LUA_TSTRING:
		status = lua_tostring(L, 1);
		break;
	case LUA_TBOOLEAN:
		status = lua_toboolean(L, 1) ? nil : "failure";
		break;
	case LUA_TNUMBER:
		status = smprint("failure %lld", lua_tointeger(L, 1));
		break;
	}
	if(lua_toboolean(L, 2))
		lua_close(L);
	exits(status);
	return 0;
}

static int
os_execute(lua_State *L)
{
	const char *cmd;
	Waitmsg *w;
	
	cmd = luaL_optstring(L, 1, nil);
	if(cmd == nil){
		lua_pushboolean(L, 1);
		return 1;
	}
	switch(rfork(RFPROC|RFFDG|RFREND)){
	case -1:
		luaL_pushfail(L);
		lua_pushstring(L, "exit");
		lua_pushinteger(L, 999);
		return 3;
	case 0:
		execl("/bin/rc", "rc", "-c", cmd, nil);
		sysfatal("exec: %r");
	}
	w = wait();
	if(w && w->msg[0] != 0){
		luaL_pushfail(L);
		if(strncmp(w->msg, "interrupt", 9) == 0
		|| strncmp(w->msg, "alarm", 5) == 0
		|| strncmp(w->msg, "hangup", 6) == 0)
			lua_pushstring(L, "signal");
		else
			lua_pushstring(L, "exit");
		lua_pushinteger(L, 999);
	}else{
		lua_pushboolean(L, 1);
		lua_pushstring(L, "exit");
		lua_pushinteger(L, 0);
	}
	return 3;
}

static int
os_remove(lua_State *L){
	char err[ERRMAX];
	const char *file;
	
	file = luaL_checkstring(L, 1);
	if(remove(file) == -1){
		rerrstr(err, sizeof err);
		luaL_pushfail(L);
		lua_pushfstring(L, "%s", err);
		lua_pushinteger(L, 999);
		return 3;
	}
	lua_pushboolean(L, 1);
	return 1;
}

static int
os_rename(lua_State *L)
{
	char *from = luaL_checkstring(L, 1);
	char *to = luaL_checkstring(L, 2);
	char *cmd;
	int ret;
	
	/* This will have to do until mv(1) is
	 * basically reimplemented here.
	 * Diagnostic information is particularly
	 * lacking and could be improved by using
	 * the stderr output of mv(1) as an error
	 * message. */
	cmd = smprint("return os.execute('/bin/mv %q %q >[2]/dev/null')", from, to);
	ret = luaL_dostring(L, cmd);
	free(cmd);
	if(ret != LUA_OK){
		luaL_pushfail(L);
		lua_pushstring(L, "should never happen");
		lua_pushinteger(L, 999);
		return 3;
	}
	if(lua_toboolean(L, -3) == 0)
		return 3;
	lua_pushboolean(L, 1);
	return 1;
}

static int
os_tmpname(lua_State *L)
{
	int i, fd;
	char buf[32];
	
	for(i = 0; i < 3; i++){
		snprint(buf, sizeof buf, "/tmp/lua.%.8lx", lrand());
		if(access(buf, AEXIST) != 0)
			goto OK;
	}
Error:
	return luaL_error(L, "unable to generate a unique filename");
OK:
	if((fd = create(buf, OWRITE, 0600)) == -1)
		goto Error;
	close(fd);
	lua_pushstring(L, buf);
	return 1;
}

static int
os_getenv(lua_State *L)
{
	char *v;
	
	v = getenv(luaL_checkstring(L, 1));
	lua_pushstring(L, v);
	free(v);
	return 1;
}

static int
os_clock(lua_State *L)
{
	lua_pushnumber(L, (lua_Number)_tos->pcycles / _tos->cyclefreq);
	return 1;
}

static void
setfield(lua_State *L, const char *key, int value, int delta)
{
	lua_pushinteger(L, (lua_Integer)value + delta);
	lua_setfield(L, -2, key);
}

static void
setboolfield(lua_State *L, const char *key, int value)
{
	if(value < 0)
		return;
	lua_pushboolean(L, value);
	lua_setfield(L, -2, key);
}

static void
setallfields(lua_State *L, Tm *tm)
{
	setfield(L, "year", tm->year, 1900);
	setfield(L, "month", tm->mon, 1);
	setfield(L, "day", tm->mday, 0);
	setfield(L, "hour", tm->hour, 0);
	setfield(L, "min", tm->min, 0);
	setfield(L, "sec", tm->sec, 0);
	setfield(L, "yday", tm->yday, 1);
	setfield(L, "wday", tm->wday, 1);
	setboolfield(L, "isdst", 0);
}

static int
getfield(lua_State *L, const char *key, int d, int delta)
{
	int isnum;
	int t = lua_getfield(L, -1, key);
	lua_Integer res = lua_tointegerx(L, -1, &isnum);

	if(!isnum){
		if (t != LUA_TNIL)
			return luaL_error(L, "field '%s' is not an integer", key);
		else if (d < 0)
			return luaL_error(L, "field '%s' missing in date table", key);
		res = d;
	}else{
		/* unsigned avoids overflow when lua_Integer has 32 bits */
		if (!(res >= 0 ? (lua_Unsigned)res <= (lua_Unsigned)INT_MAX + delta
				           : (lua_Integer)INT_MIN + delta <= res))
			return luaL_error(L, "field '%s' is out-of-bound", key);
		res -= delta;
	}
	lua_pop(L, 1);
	return res;
}

static int
os_time(lua_State *L)
{
	vlong t;
	Tm tm;

	if (lua_isnoneornil(L, 1))
		t = time(nil);
	else {
		luaL_checktype(L, 1, LUA_TTABLE);
		lua_settop(L, 1);
		tm.tz = tzload("local");
		tm.year = getfield(L, "year", -1, 1900);
		tm.mon = getfield(L, "month", -1, 1);
		tm.mday = getfield(L, "day", -1, 0);
		tm.hour = getfield(L, "hour", 12, 0);
		tm.min = getfield(L, "min", 0, 0);
		tm.sec = getfield(L, "sec", 0, 0);
		t = tmnorm(&tm);
		setallfields(L, &tm);
	}
	lua_pushinteger(L, t);
	return 1;
}

static int
os_difftime(lua_State *L)
{
	vlong t1 = luaL_checkinteger(L, 1);
	vlong t2 = luaL_checkinteger(L, 2);
	lua_pushinteger(L, t1 - t2);
	return 1;
}

static char*
datefmt1(lua_State *, char **sp, Tm *tm)
{
	char *s, mod;
	Fmt f;
	
	fmtstrinit(&f);
	s = *sp;
	mod = 0;
	if(s[0] == 'E' || s[0] == '0'){
		mod = s[0];
		s++;
	}
	switch(s[0]){
	case '%': fmtprint(&f, "%%"); break;
	case 'n': fmtprint(&f, "\n"); break;
	case 't': fmtprint(&f, "\t"); break;
	
	case 'y': fmtprint(&f, "%τ", tmfmt(tm, "YY")); break;
	case 'Y': fmtprint(&f, "%τ", tmfmt(tm, "YYYY")); break;
	
	case 'm': fmtprint(&f, "%τ", tmfmt(tm, "MM")); break;
	case 'b': fmtprint(&f, "%τ", tmfmt(tm, "MMM")); break;
	case 'B': fmtprint(&f, "%τ", tmfmt(tm, "MMMM")); break;
	
	/* TODO: week of the year calculation */
	case 'U': fmtprint(&f, "00"); break;
	case 'W': fmtprint(&f, "01"); break;
	
	case 'j': fmtprint(&f, "%.3d", tm->yday + 1); break;
	case 'd': fmtprint(&f, "%.2d", tm->mday); break;

	case 'w': fmtprint(&f, "%τ", tmfmt(tm, "W")); break;
	case 'a': fmtprint(&f, "%τ", tmfmt(tm, "WW")); break;
	case 'A': fmtprint(&f, "%τ", tmfmt(tm, "WWW")); break;
	
	case 'H': fmtprint(&f, "%τ", tmfmt(tm, "hh")); break;
	case 'I': fmtprint(&f, "%d", tm->hour % 12); break;
	
	case 'M': fmtprint(&f, "%τ", tmfmt(tm, "mm")); break;
	
	case 'S': fmtprint(&f, "%τ", tmfmt(tm, "ss")); break;

	case 'c': case 'x': fmtprint(&f, "%τ", tmfmt(tm, nil)); break;
	case 'X': fmtprint(&f, "%τ", tmfmt(tm, "hh[:]mm[:]ss")); break;
	
	case 'p': fmtprint(&f, "%τ", tmfmt(tm, "a")); break;
	
	case 'Z': fmtprint(&f, "%τ", tmfmt(tm, "Z")); break;
	
	default:
		fmtprint(&f, "%%");
		if(mod)
			fmtprint(&f, "%c", mod);
		fmtprint(&f, "%c", s[0]);
		break;
	}
	*sp = ++s;
	return fmtstrflush(&f);
}

static void
datefmt(lua_State *L, char *fmt, long fmtlen, Tm *tm)
{
	char *s, *e, *p;
	luaL_Buffer b;
	
	luaL_buffinit(L, &b);
	s = fmt;
	e = fmt + fmtlen;
	while(s < e){
		if(s[0] != '%')
			luaL_addchar(&b, *s++);
		else {
			s++;
			p = datefmt1(L, &s, tm);
			luaL_addstring(&b, p);
			free(p);
		}
	}
	luaL_pushresult(&b);
}

static int
os_date(lua_State *L)
{
	const char *fmt;
	size_t fmtlen;
	vlong t;
	Tm tm, *tp;
	
	fmt = luaL_optlstring(L, 1, "%c", &fmtlen);
	t = luaL_opt(L, luaL_checkinteger, 2, time(nil));
	if(fmt[0] == '!'){
		fmt++; fmtlen--;
		tp = tmtime(&tm, t, tzload("GMT"));
	}else
		tp = tmtime(&tm, t, tzload("local"));
	if(tp == nil)
		return luaL_error(L, "date result cannot be represented in this installation");
	if(strcmp(fmt, "*t") == 0){
		lua_createtable(L, 0, 9);
		setallfields(L, tp);
	}else
		datefmt(L, fmt, fmtlen, tp);
	return 1;
}

static int
os_setlocale(lua_State *L)
{
	if(lua_gettop(L) == 0 || lua_isnil(L, 1)
	|| strcmp(luaL_checkstring(L, 1), "C") == 0)
		lua_pushstring(L, "C");
	else
		luaL_pushfail(L);
	return 1;
}

static const luaL_Reg syslib[] = {
	{"execute",   os_execute},
	{"exit",      os_exit},
	{"getenv",    os_getenv},
	{"remove",    os_remove},
	{"rename",    os_rename},
	{"tmpname",   os_tmpname},
	{"clock",     os_clock},
	{"date",      os_date},
	{"time",      os_time},
	{"difftime",  os_difftime},
	{"setlocale",	os_setlocale},
	{nil, nil}
};

LUAMOD_API int luaopen_os (lua_State *L) {
	quotefmtinstall();
	tmfmtinstall();
	
	srand(time(0));

	luaL_newlib(L, syslib);
	return 1;
}
