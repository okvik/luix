#include <u.h>
#include <libc.h>

#include "shim.h"

/*
 * This pointer MUST be initialized by
 * a call to privalloc(2) prior to any
 * use of the errno.
 *   if(priv_errno == nil) priv_errno = privalloc();
 */
errno_t *priv_errno;

char*
strerror(int)
{
	static char err[ERRMAX];
	
	rerrstr(err, sizeof err);
	return err;
}
