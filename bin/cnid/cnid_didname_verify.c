/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * Modified to check the consistency of didname.db by
 * Joe Clarke <marcus@marcuscom.com>
 *
 * $Id: cnid_didname_verify.c,v 1.1 2001-12-10 07:04:27 jmarcus Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <db.h>

#ifndef MIN
#define MIN(a, b)  ((a) < (b) ? (a) : (b))
#endif /* ! MIN */

int	main __P((int, char *[]));
void	usage __P((void));
void	version_check __P((void));

const char
	*progname = "db_verify";			/* Program name. */

static __inline__ int compare_did(const DBT *a, const DBT *b) {
	u_int32_t dida, didb;

	memcpy(&dida, a->data, sizeof(dida));
	memcpy(&didb, b->data, sizeof(didb));
	return dida - didb;
}

#if DB_VERSION_MINOR > 1
static int compare_unix(DB *db, const DBT *a, const DBT *b)
#else
static int compare_unix(const DBT *a, const DBT *b)
#endif
{
	u_int8_t *sa, *sb;
	int len, ret;

	if ((ret = compare_did(a, b)))
		return ret;
	
	sa = (u_int8_t *) a->data + 4;
	sb = (u_int8_t *) b->data + 4;
	for (len = MIN(a->size, b->size); len-- > 4; sa++, sb++)
		if ((ret = (*sa - *sb)))
			return ret;
	return a->size - b->size;
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	DB *dbp;
	DB_ENV *dbenv;
	int ch, e_close, exitval, nflag, quiet, ret, t_ret;
	char *home;
	char *dbname = "didname.db";

	version_check();

	dbenv = NULL;
	e_close = exitval = nflag = quiet = 0;
	home = NULL;
	while ((ch = getopt(argc, argv, "h:NqV")) != EOF)
		switch (ch) {
		case 'h':
			home = optarg;
			break;
		case 'N':
			nflag = 1;
			if ((ret = db_env_set_panicstate(0)) != 0) {
				fprintf(stderr,
				    "%s: db_env_set_panicstate: %s\n",
				    progname, db_strerror(ret));
				exit (1);
			}
			break;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'V':
			printf("%s\n", db_version(NULL, NULL, NULL));
			exit(0);
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/*
	 * Create an environment object and initialize it for error
	 * reporting.
	 */
	if ((ret = db_env_create(&dbenv, 0)) != 0) {
		fprintf(stderr, "%s: db_env_create: %s\n",
		    progname, db_strerror(ret));
		goto shutdown;
	}
	e_close = 1;

	/*
	 * XXX
	 * We'd prefer to have error output configured while calling
	 * db_env_create, but there's no way to turn it off once it's
	 * turned on.
	 */
	if (!quiet) {
		dbenv->set_errfile(dbenv, stderr);
		dbenv->set_errpfx(dbenv, progname);
	}

	if (nflag && (ret = dbenv->set_mutexlocks(dbenv, 0)) != 0) {
		dbenv->err(dbenv, ret, "set_mutexlocks");
		goto shutdown;
	}

	/*
	 * Attach to an mpool if it exists, but if that fails, attach
	 * to a private region.
	 */
	if ((ret = dbenv->open(dbenv,
	    home, DB_INIT_MPOOL | DB_USE_ENVIRON, 0)) != 0 &&
	    (ret = dbenv->open(dbenv, home,
	    DB_CREATE | DB_INIT_MPOOL | DB_PRIVATE | DB_USE_ENVIRON, 0)) != 0) {
		dbenv->err(dbenv, ret, "open");
		goto shutdown;
	}

	if ((ret = db_create(&dbp, dbenv, 0)) != 0) {
		fprintf(stderr,
		    "%s: db_create: %s\n", progname, db_strerror(ret));
		goto shutdown;
	}

	/* This is the crux of the program.  We need to make sure we verify
	 * using the correct sort routine.  Else, the database will report
	 * corruption. */
	dbp->set_bt_compare(dbp, &compare_unix);

	if (!quiet) {
		dbp->set_errfile(dbp, stderr);
		dbp->set_errpfx(dbp, progname);
	}
	if ((ret = dbp->verify(dbp, dbname, NULL, NULL, 0)) != 0)
		dbp->err(dbp, ret, "DB->verify: %s", dbname);
	if ((t_ret = dbp->close(dbp, 0)) != 0 && ret == 0) {
		dbp->err(dbp, ret, "DB->close: %s", dbname);
		ret = t_ret;
	}
	if (ret != 0)
		goto shutdown;

	if (0) {
shutdown:	exitval = 1;
	}
	if (e_close && (ret = dbenv->close(dbenv, 0)) != 0) {
		exitval = 1;
		fprintf(stderr,
		    "%s: dbenv->close: %s\n", progname, db_strerror(ret));
	}

	return (exitval);
}

void
usage()
{
	fprintf(stderr, "usage: db_verify [-NqV] [-h home] ...\n");
	exit (1);
}

void
version_check()
{
	int v_major, v_minor, v_patch;

	/* Make sure we're loaded with the right version of the DB library. */
	(void)db_version(&v_major, &v_minor, &v_patch);
	if (v_major != DB_VERSION_MAJOR ||
	    v_minor != DB_VERSION_MINOR || v_patch != DB_VERSION_PATCH) {
		fprintf(stderr,
	"%s: version %d.%d.%d doesn't match library version %d.%d.%d\n",
		    progname, DB_VERSION_MAJOR, DB_VERSION_MINOR,
		    DB_VERSION_PATCH, v_major, v_minor, v_patch);
		exit (1);
	}
}