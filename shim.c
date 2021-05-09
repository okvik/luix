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

void
exit(int code)
{
	char *status;
	
	switch(code){
	case EXIT_SUCCESS:
		status = nil; break;
	case EXIT_FAILURE:
	default:
		status = "failure"; break;
	}
	exits(status);
}

#define MAXFORKS	30
#define NSYSFILE	3
#define	tst(a, b)	(*mode == 'r' ? (b) : (a))
#define	RDR	0
#define	WTR	1

struct a_fork {
	char done;
	int	fd;
	int	pid;
	char status[255+1];
};
static struct a_fork the_fork[MAXFORKS];

FILE*
popen(char *cmd, char *mode)
{
	int p[2];
	int us, them, pid;
	int i, ind;

	for(ind = 0; ind < MAXFORKS; ind++)
		if(the_fork[ind].pid == 0)
			break;
	if(ind == MAXFORKS)
		return NULL;
	if(pipe(p) == -1)
		return NULL;
	us = tst(p[WTR], p[RDR]);
	them = tst(p[RDR], p[WTR]);
	switch(pid = fork()){
	case -1:
		return NULL;
	case 0:
		/* us and them reverse roles in child */
		close(us);
		dup(them, tst(0, 1));
		for(i = NSYSFILE; i < FOPEN_MAX; i++)
			close(i);
		execl("/bin/rc", "rc", "-c", cmd, NULL);
		rfork(RFNAMEG);
		bind("/bin/ape", "/bin", MBEFORE);
		execl("/bin/rc", "rc", "-c", cmd, NULL);
		exits("exec failed");
	default:
		the_fork[ind].pid = pid;
		the_fork[ind].fd = us;
		the_fork[ind].done = 0;
		close(them);
		return fdopen(us, mode);
	}
	return NULL;
}

int
pclose(FILE *file)
{
	int f, r, ind;
	Waitmsg *status;

	f = fileno(file);
	fclose(file);
	for(ind = 0; ind < MAXFORKS; ind++)
		if(the_fork[ind].fd == f && the_fork[ind].pid != 0)
			break;
	if(ind == MAXFORKS)
		return 0;
	if(!the_fork[ind].done){
		do{
			if((status = wait()) == nil)
				r = -1;
			else
				r = status->pid;
			for(f = 0; f < MAXFORKS; f++){
				if(r == the_fork[f].pid){
					the_fork[f].done = 1;
					strncpy(the_fork[f].status, status->msg, sizeof(the_fork[f].status));
					break;
				}
			}
			free(status);
		} while(r != the_fork[ind].pid && r != -1);
		if(r == -1)
			strcpy(the_fork[ind].status, "No loved ones to wait for");
	}
	the_fork[ind].pid = 0;
	if(the_fork[ind].status[0] != '\0')
		return 1;
	return 0;
}
