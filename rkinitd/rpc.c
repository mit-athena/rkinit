/* 
 * $Id: rpc.c,v 1.4 1997-12-03 22:05:07 ghudson Exp $
 * $Source: /afs/dev.mit.edu/source/repository/athena/bin/rkinit/rkinitd/rpc.c,v $
 * $Author: ghudson $
 *
 * This file contains the network parts of the rkinit server.
 */

#if !defined(lint) && !defined(SABER) && !defined(LOCORE) && defined(RCS_HDRS)
static char *rcsid = "$Id: rpc.c,v 1.4 1997-12-03 22:05:07 ghudson Exp $";
#endif /* lint || SABER || LOCORE || RCS_HDRS */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>

#include <rkinit.h>
#include <rkinit_err.h>
#include <rkinit_private.h>

#include "rkinitd.h"

#define RKINITD_TIMEOUT 60

static int in;			/* sockets */
static int out;

static char errbuf[BUFSIZ];

void error();

#ifdef __STDC__
static int timeout(void)
#else
static int timeout()
#endif /* __STDC__ */
{
    syslog(LOG_WARNING, "rkinitd timed out.\n");
    exit(1);

    return(0);			/* To make the compiler happy... */
}

/*
 * This function does all the network setup for rkinitd.
 * It returns true if we were started from inetd, or false if 
 * we were started from the commandline.
 * It causes the program to exit if there is an error. 
 */
#ifdef __STDC__
int setup_rpc(int notimeout)		
#else
int setup_rpc(notimeout)
  int notimeout; /* True if we should not timeout */
#endif /* __STDC__ */
{
    struct itimerval timer;	/* Time structure for timeout */
#ifdef POSIX
   struct sigaction act, oact;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
#endif


    /* For now, support only inetd. */
    in = 0;
    out = 1;

    if (! notimeout) {
	SBCLEAR(timer);

	/* Set up an itimer structure to send an alarm signal after timeout
	   seconds. */
	timer.it_interval.tv_sec = RKINITD_TIMEOUT;
	timer.it_interval.tv_usec = 0;
	timer.it_value = timer.it_interval;
	
	/* Start the timer. */
	if (setitimer (ITIMER_REAL, &timer, (struct itimerval *)0) < 0) {
	    sprintf(errbuf, "setitimer: %s", strerror(errno));
	    rkinit_errmsg(errbuf);
	    error();
	    exit(1);
	}

#ifdef POSIX
      act.sa_handler= (void (*)()) timeout;
      (void) sigaction (SIGALRM, &act, NULL);
#else
	signal(SIGALRM, timeout);
#endif
    }

    return(TRUE);
}

#ifdef __STDC__
void rpc_exchange_version_info(int *c_lversion, int *c_hversion, 
			       int s_lversion, int s_hversion)
#else
void rpc_exchange_version_info(c_lversion, c_hversion, s_lversion, s_hversion)
  int *c_lversion;
  int *c_hversion;
  int s_lversion;
  int s_hversion;
#endif /* __STDC__ */
{
    u_char version_info[VERSION_INFO_SIZE];
    u_long length = sizeof(version_info);
    
    if (rki_get_packet(in, MT_CVERSION, &length, (char *)version_info) !=
	RKINIT_SUCCESS) {
	error();
	exit(1);
    }

    *c_lversion = version_info[0];
    *c_hversion = version_info[1];

    version_info[0] = s_lversion;
    version_info[1] = s_hversion;

    if (rki_send_packet(out, MT_SVERSION, length, (char *)version_info) != 
	RKINIT_SUCCESS) {
	error();
	exit(1);
    }
}
    
#ifdef __STDC__
void rpc_get_rkinit_info(rkinit_info *info)
#else
void rpc_get_rkinit_info(info)
  rkinit_info *info;
#endif /* __STDC__ */
{
    u_long length = sizeof(rkinit_info);
    
    if (rki_get_packet(in, MT_RKINIT_INFO, &length, (char *)info)) {
	error();
	exit(1);
    }
    
    info->lifetime = ntohl(info->lifetime);
}

#ifdef __STDC__
void rpc_send_error(char *errmsg)
#else
void rpc_send_error(errmsg)
  char *errmsg;
#endif /* __STDC__ */
{
    if (rki_send_packet(out, MT_STATUS, strlen(errmsg), errmsg)) {
	error();
	exit(1);
    }
}

#ifdef __STDC__
void rpc_send_success(void)
#else
void rpc_send_success()
#endif /* __STDC__ */
{
    if (rki_send_packet(out, MT_STATUS, 0, "")) {
	error();
	exit(1);
    }
}

#ifdef __STDC__
void rpc_exchange_tkt(KTEXT cip, MSG_DAT *scip)
#else
void rpc_exchange_tkt(cip, scip)
  KTEXT cip;
  MSG_DAT *scip;
#endif /* __STDC__ */
{
    u_long length = MAX_KTXT_LEN;

    if (rki_send_packet(out, MT_SKDC, cip->length, (char *)cip->dat)) {
	error();
	exit(1);
    }
    
    if (rki_get_packet(in, MT_CKDC, &length, (char *)scip->app_data)) {
	error();
	exit(1);
    }
    scip->app_length = length;
}

#ifdef __STDC__
void rpc_getauth(KTEXT auth, struct sockaddr_in *caddr, 
		 struct sockaddr_in *saddr)
#else
void rpc_getauth(auth, caddr, saddr)
  KTEXT auth;
  struct sockaddr_in *caddr;
  struct sockaddr_in *saddr;
#endif /* __STDC__ */
{
    int addrlen = sizeof(struct sockaddr_in);

    if (rki_rpc_get_ktext(in, auth, MT_AUTH)) {
	error();
	exit(1);
    }

    if (getpeername(in, caddr, &addrlen) < 0) {
	sprintf(errbuf, "getpeername: %s", strerror(errno));
	rkinit_errmsg(errbuf);
	error();
	exit(1);
    }

    if (getsockname(out, saddr, &addrlen) < 0) {
	sprintf(errbuf, "getsockname: %s", strerror(errno));
	rkinit_errmsg(errbuf);
	error();
	exit(1);
    }
}
