# Lua for Plan 9 space

luix is a native Plan 9 port of the Lua 5.4 library featuring a fairly
complete Plan 9 system library, a simplified standalone interpreter,
and a few useful extras such as LPeg.

The project contains several components, with the port of Lua being
the base for other components. It is fully usable on its own; and is
expected to be copied and compiled with your own projects that wish to
embed Lua.

The luix standalone interpreter `luix.c` along with the mkfile and
other files in the root of this repo serves primarily as an example of
how a possible embedding may be done.

The p9 module provides a Plan 9 system interface to Lua programs. It
provides a range of functionality from basic system calls to a
higher-level convenience functionality for accomplishing the most
common system interaction tasks. It is supposed to allow writing
pure Lua scripts in lieu of classic shelling out approach.

Finally, lpeg is a simple port of the LPeg pattern matching module.
  
## luix(1) standalone interpreter

### Building and installing

Building and installing luix is simple: 

	; git/clone https://github.com/okvik/luix
	; mk install

Only the luix binary and Lua modules get installed into the system.
The binary is statically linked so no C libraries have to be installed.

### Usage

Check the [luix(1) manual page](http://a-b.xyz/man/1/luix) for more
information.

	; man luix

## Legal

All of my original work is released under the MIT license.
