/* 
 * $Header: /afs/dev.mit.edu/source/repository/athena/bin/rkinit/rkinit/rkinit.c,v 1.1 1989-11-12 19:34:03 qjb Exp $
 * $Source: /afs/dev.mit.edu/source/repository/athena/bin/rkinit/rkinit/rkinit.c,v $
 * $Author: qjb $
 *
 */

#if !defined(lint) && !defined(SABER)
static char *rcsid_rkinit_c = "$Header: /afs/dev.mit.edu/source/repository/athena/bin/rkinit/rkinit/rkinit.c,v 1.1 1989-11-12 19:34:03 qjb Exp $";
#endif lint || SABER

#include <stdio.h>
#include <strings.h>
#include <netdb.h>
#include <pwd.h>
#include <krb.h>
#include <des.h>

#include <rkinit.h>
#include <rkinit_err.h>

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

main(argc, argv)
  int argc;
  char *argv[];
{
    char *whoami;		/* Name of this program */

    char principal[MAX_K_NAME_SZ]; /* Principal for which to get tickets */
    char *host;			/* Remote host */
    char *username = 0;	/* Username of owner of ticket */
    char r_krealm[REALM_SZ];	/* Kerberos realm of remote host */
    char aname[ANAME_SZ];	/* Aname of remote ticket file */
    char inst[INST_SZ];		/* Instance of remote ticket file */
    char realm[REALM_SZ];	/* Realm of remote ticket file */
    char *tktfilename = NULL;	/* Name of ticket file on remote host */
    u_long lifetime = DEFAULT_TKT_LIFE;	/* Lifetime of remote tickets */
    int timeout = TRUE;		/* Should we time out? */
    rkinit_info info;		/* Information needed by rkinit */

    struct passwd *localid;	/* To determine local id */

    int status = 0;		/* general error number */

    int i;

    bzero(principal, sizeof(principal));
    bzero(aname, sizeof(aname));
    bzero(inst, sizeof(inst));
    bzero(realm, sizeof(realm));

    /* Parse commandline arguements. */
    if ((whoami = rindex(argv[0], '/')) == 0)
	whoami = argv[0];
    else
	whoami++;

    if (argc < 2) usage();

    host = argv[1];
    for (i = 2; i < argc; i++) {
	if (strcmp(argv[i], "-l") == NULL) {
	    if (++i >= argc)
		usage();
	    else 
		username = argv[i];
	}
	else if (strcmp(argv[i], "-k") == NULL) {
	    if (++i >= argc)
		usage();
	    else
		strncpy(r_krealm, argv[i], sizeof(r_krealm) - 1);
	}
	else if (strcmp(argv[i], "-p") == NULL) {
	    if (++i >= argc)
		usage();
	    else
		strncpy(principal, argv[i], sizeof(principal) - 1);
	}
	else if (strcmp(argv[i], "-f") == NULL) {
	    if (++i >= argc)
		usage();
	    else
		tktfilename = argv[i];
	}
	else if (strcmp(argv[i], "-t") == NULL) {
	    if (++i >= argc)
		usage();
	    else {
		lifetime = atoi(argv[i])/5;
		if (lifetime == 0)
		    lifetime = 1;
	    }
	}
	else if (strcmp(argv[i], "-notimeout") == NULL)
	    timeout = FALSE;
	else
	    usage();
    }

    /* Initialize the realm of the remote host if necessary */
    if (r_krealm[0] == 0) {
	/* 
	 * Try to figure out the realm of the remote host.  If the
	 * remote host is unknown, don't worry about it; the library
	 * will handle the error better and print a good error message.
	 */
	struct hostent *hp;
	if (hp = gethostbyname(host))
	    strcpy(r_krealm, krb_realmofhost(hp->h_name));
    }

    /* If no username was specified, use local id on client host */
    if (username == 0) {
	if ((localid = getpwuid(getuid())) == 0) {
	    fprintf(stderr, "You can not be found in the password file.\n");
	    exit(1);
	}
	username = localid->pw_name;
    }

    /* Find out who will go in the ticket file */
    if (! principal[0]) {
	if ((status = krb_get_tf_fullname(TKT_FILE, aname, inst, realm)) 
	    != KSUCCESS) {
	    /* 
	     * If user has no ticket file and principal was not specified, 
	     * we will try to get tickets for username@remote_realm
	     */
	    strcpy(aname, username);
	    strcpy(realm, r_krealm);
	}
    }
    else {
	if ((status = kname_parse(aname, inst, realm, principal))
	    != KSUCCESS) {
	    fprintf(stderr, "%s\n", krb_err_txt[status]);
	    exit(1);
	}
	if (strlen(realm) == 0) {
	    if (krb_get_lrealm(realm, 1) != KSUCCESS)
		strcpy(realm, KRB_REALM);
	}
    }

    bzero((char *)&info, sizeof(info));
    
    strcpy(info.aname, aname);
    strcpy(info.inst, inst);
    strcpy(info.realm, realm);
    strcpy(info.sname, "krbtgt");
    strcpy(info.sinst, realm);
    strncpy(info.username, username, sizeof(info.username) - 1);
    if (tktfilename)
	strncpy(info.tktfilename, tktfilename, sizeof(info.tktfilename) - 1);
    info.lifetime = lifetime;

    if (status = rkinit(host, r_krealm, &info, timeout)) {
	com_err(whoami, status, "while obtaining remote tickets:");
	fprintf(stderr, "%s\n", rkinit_errmsg(0));
	exit(1);
    }

    exit(0);
}
	
usage() 
{
    fprintf(stderr,"Usage: rkinit host options\n");
    fprintf(stderr,
      "Options: [-l username] [-k krb_realm] [-p principal] [-f tktfile]\n");
    fprintf(stderr, "         [-t lifetime] [-notimeout]\n");
	    
    exit(1);
}
