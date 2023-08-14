/* usemem.c
**
** Force well-defined utilization of memory
**
** Usage: usemem [-m|-s|-S] [-t|-n] [-M] [-hl] [-r seconds] virtsz [physsz [alivesz]]
**
** Flags:
**   -m		use mmap to allocate (default: malloc)
**   -s		create as Posix shared memory
**   -S		create as System V shared memory
**
**   -t		advise to use transparent huge pages
**   -n		advise not to use transparent huge pages
**   -M		advise to use KSM (same page merging)
**   -C		advise to deactivate pages (cold)
**   -P		advise to pageout (reclaim) pages
**   -R		advise to populate (prefault) page tables readable
**   -W		advise to populate (prefault) page tables writable
**
**   -h		use huge pages (not for malloc or Posix IPC)
**   -l		lock memory
**
**   virtsz 	requested memory
**   physsz 	referenced memory (once)
**   alivesz	referenced memory (each second)
**
** All sizes can be extended with [KMGT]
** ==========================================================================
** Author:       JC van Winkel		original version based on malloc
**
**               Gerlof Langeveld	extended version:
**					- with memory types mmap, static huge pages,
**					  POSIX shared memory and SystemV shared memory
**					- advises with madvise()
**					- memory locking
**					- repetition to simulate memory leakage
**
** Copyright (C) AT Computing	2008
** Extended:     AT Computing	2017/2023 
** ==========================================================================
** This file is free software.  You can redistribute it and/or modify
** it under the terms of the GNU General Public License (GPL); either
** version 3, or (at your option) any later version.
*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <ctype.h>

#ifndef	MADV_HUGEPAGE
#define	MADV_HUGEPAGE	0	// ignore if not supported
#endif

#ifndef	MADV_NOHUGEPAGE
#define	MADV_NOHUGEPAGE	0	// ignore if not supported
#endif

#ifndef	MADV_COLD
#define	MADV_COLD	0	// ignore if not supported
#endif

#ifndef	MADV_PAGEOUT
#define	MADV_PAGEOUT	0	// ignore if not supported
#endif

#ifndef	MADV_POPULATE_READ
#define	MADV_POPULATE_READ	0	// ignore if not supported
#endif

#ifndef	MADV_POPULATE_WRITE
#define	MADV_POPULATE_WRITE	0	// ignore if not supported
#endif

static long long	getnum(const char *);
static void		do_advise(char *, int, void *, size_t);

void
conflict(char f1, char f2)
{
	fprintf(stderr, "conflicting flags: %c and %c\n", f1, f2);
	exit(2);
}

int
main(int argc, char *argv[])
{
	char 		*p, *msg;
	char		alloctype = 'a';
	char		tflag = 0, nflag = 0, hflag = 0, lflag = 0, Mflag = 0,
			Cflag = 0, Pflag = 0, Rflag = 0, Wflag = 0;
	int		i, c, opts, fd, flags=0;
	long		pagesize = sysconf(_SC_PAGESIZE), repeatinterval = -1;
	long long 	j;
	long long 	virtual = 0;
	long long 	physical = 0;
	long long 	keepalive = 0;

	// correct number of arguments?
	//
	if (argc < 2) {
		fprintf(stderr,
		        "Usage: usemem [-m|-s|-S] [-t|-n] [-MCPRW] [-hl] "
			"[-r sec] virtsize [physsize [alivesize]]\n");
		fprintf(stderr, "\tflags:\n");
		fprintf(stderr, "\t\t-m\tuse mmap to allocate (default: malloc)\n");
		fprintf(stderr, "\t\t-s\tcreate as Posix shared memory\n");
		fprintf(stderr, "\t\t-S\tcreate as System V shared memory\n\n");

		fprintf(stderr, "\t\t-t\tadvise to use transparent huge pages\n");
		fprintf(stderr, "\t\t-n\tadvise not to use transparent huge pages\n");
		fprintf(stderr, "\t\t-M\tadvise to use KSM (same page merging)\n");
		fprintf(stderr, "\t\t-C\tadvise to deactivate pages (cold)\n");
		fprintf(stderr, "\t\t-P\tadvise to pageout (reclaim) pages\n");
		fprintf(stderr, "\t\t-R\tadvise to populate (prefault) page tables readable\n");
		fprintf(stderr, "\t\t-W\tadvise to populate (prefault) page tables writable\n\n");

		fprintf(stderr, "\t\t-h\tuse huge pages (not for malloc or Posix IPC)\n");
		fprintf(stderr, "\t\t-l\tlock memory\n\n");
		fprintf(stderr, "\t\t-r sec\trepeat allocation every <sec> seconds\n\n");

		fprintf(stderr, "\tvirtsize \trequested memory\n");
		fprintf(stderr, "\tphyssize \treferenced memory (once)\n");
		fprintf(stderr, "\talivesize\treferenced memory (each second)\n");
		fprintf(stderr, "\tall sizes can be extended with [KMGT]\n");
		exit(1);
	}

	// verify flags
	// 
	while ((c=getopt(argc, argv, "msStnMCPRWhlr:")) != EOF) {
		switch (c) {
		   case 'm':
			if (alloctype != 'a') 
				conflict(alloctype, c);
			else
				alloctype = 'm';
			break;

		   case 's':
			if (alloctype != 'a') 
				conflict(alloctype, c);
			else
				alloctype = 's';
			break;

		   case 'S':
			if (alloctype != 'a') 
				conflict(alloctype, c);
			else
				alloctype = 'S';
			break;

		   case 't':
			tflag = 1;
			break;

		   case 'n':
			nflag = 1;
			break;

		   case 'M':
			Mflag = 1;
			break;

		   case 'C':
			Cflag = 1;
			break;

		   case 'P':
			Pflag = 1;
			break;

		   case 'R':
			Rflag = 1;
			break;

		   case 'W':
			Wflag = 1;
			break;

		   case 'h':
			hflag = 1;
			break;

		   case 'l':
			lflag = 1;
			break;

		   case 'r':
			repeatinterval = strtol(optarg, &p, 10);

			if (*p) {
 				fprintf(stderr, "wrong repeat interval: %s\n", optarg);
				exit(1);
			}
			break;

		   default:
 			fprintf(stderr, "wrong flag: %c\n", c);
			exit(1);
		}
	}

	// gather memory size
	//
	if (optind < argc)
		virtual = getnum(argv[optind++]);

	if (optind < argc)
		physical = getnum(argv[optind++]);

	if (optind < argc) {
		if (repeatinterval == -1) {
			keepalive = getnum(argv[optind]);
		} else {
 			fprintf(stderr,
				"alivesize can't be combined with repeat\n");
			exit(1);
		}
	}

	// verify consistency of specified memory sizes
	//
	if (virtual == 0) {
		fprintf(stderr, "virtsize must be defined and larger than 0\n");
		exit(1);
	}

	if (physical > virtual) {
		fprintf(stderr, "physsize cannot be larger than virtsize\n");
		exit(1);
	}

	if (keepalive > physical) {
	 	fprintf(stderr, "alivesize cannot be larger than physsize\n");
		exit(1);
	}

	// potential allocation loop
	// (just once in case no repetition is required)
	//
	while (1) {
		// allocate memory virtually
		//
		switch (alloctype) {

	   	   // conventional malloc
	   	   //
		   case 'a':
			if (hflag)
				fprintf(stderr, "warning: -h flag ignored for malloc\n");

			msg = "malloc";

			if (tflag+nflag+Mflag+Cflag+Pflag+Rflag+Wflag+lflag) {
				// start address must be page-aligned
				p = malloc(virtual+pagesize);
				if (!p)
					break;

				if ((unsigned long long)p % pagesize)
					p = (char *)(((unsigned long long)p / pagesize + 1) * pagesize);
			} else {
				p = malloc(virtual);
			}
			
			break;

		   // mmap anonymous
		   //
		   case 'm':
			opts = MAP_PRIVATE|MAP_ANONYMOUS;

			if (hflag)
				opts |= MAP_HUGETLB;
		
			msg = "mmap";
			p = mmap(NULL, virtual, PROT_READ|PROT_WRITE, opts, -1, 0);
			if (p == MAP_FAILED)
				p = 0;

			break;

		   // Posix IPC with mmap shared
		   //
		   case 's':
			opts = MAP_SHARED;
	
			if (hflag)
				fprintf(stderr, "warning: -h flag ignored for Posix IPC\n");
		
			msg = "shm_open";
			fd = shm_open("/shmtmp", O_RDWR|O_CREAT, 0600);
		
			if (fd == -1) {
				p = 0;
				break;
			}
		
       		 	shm_unlink("/shmtmp");	// destroy when detached

			msg = "ftruncate for Posix IPC";
			if ( ftruncate(fd, virtual) == -1 ) {
				p = 0;
				break;
			}

			msg = "mmap for Posix IPC";
			p = mmap(NULL, virtual, PROT_READ|PROT_WRITE, opts, fd, 0);
			if (p == MAP_FAILED)
				p = 0;

			close(fd);

			break;

		   // System V IPC 
		   //
		   case 'S':
			msg = "shmget";
       			i = shmget(IPC_PRIVATE, virtual, IPC_CREAT |
				(hflag ? SHM_HUGETLB : 0) | 0600);
		
			if (i == -1) {
				p = 0;
				break;
			}
		
			msg = "shmat";
			p = shmat(i, NULL, 0);

			(void) shmctl(i, IPC_RMID, 0);	// destroy when detached

			break;
		}

		// verify success of previous allocation
		//
		if (!p) {
			perror(msg);
			exit(1);
		}

		// handle advises before referencing memory
		//
		if (tflag)
			do_advise("-t", MADV_HUGEPAGE, p, virtual);

		if (nflag)
			do_advise("-n", MADV_NOHUGEPAGE, p, virtual);
	
		if (Mflag)
			do_advise("-M", MADV_MERGEABLE, p, virtual);
 

		// mlock memory area
		//
		if (lflag) {
			if ( mlock(p, virtual) == -1 )
				perror("warning: mlock failed");
			else
				printf("memory locked\n");
		}

		printf("%lld KiB allocated (%s) at address %p", virtual/1024, msg, p);
		fflush(stdout);

		// reference memory physically
		//
		if (physical) {
			memset(p, 'X', physical);
			printf(" / %lld KiB referenced", physical/1024);
			fflush(stdout);
		}

		// handle advises after referencing memory
		//
		if (Rflag)
			do_advise("-R", MADV_POPULATE_READ, p, virtual);
 
		if (Wflag)
			do_advise("-W", MADV_POPULATE_WRITE, p, virtual);
 
		if (Cflag)
			do_advise("-C", MADV_COLD, p, virtual);
 
		if (Pflag)
			do_advise("-P", MADV_PAGEOUT, p, virtual);
 
		//
		// verify if repetition is required (simulating memory leakage)
		//
		if (repeatinterval == -1) {
			break;
		} else {
			printf("\n");
			fflush(stdout);
			sleep(repeatinterval);
		}
	} 

	// keep referencing memory physically
	//
	if (keepalive) {
		printf(" / %lld KiB kept alive...\n", keepalive/1024);
		fflush(stdout);

	 	for (;;) {
	   		sleep(1);
			memset(p, 'X', keepalive);
		}
	} else {
		printf("\n");
		fflush(stdout);
		pause();
	}
}

static void do_advise(char *flag, int advice, void *start, size_t length)
{
	if (advice == 0) {
		fprintf(stderr, "warning: advise %s not supported (ignored)\n",
			              flag);
		return;
	}

	if (madvise(start, length, advice) == -1) {
		fprintf(stderr, "warning: advise %s", flag);
		perror(" advise failed (ignored)");
	}
}


/*
** convert requested memory size to number of bytes
*/
static long long getnum(const char *s)
{
	long long 	n;
	char 		*endptr;

	n = strtoll(s, &endptr, 10);

	if (*endptr && toupper(*endptr)=='K')
 		n*=1024;

	else if (*endptr && toupper(*endptr)=='M')
 		n*=1024*1024;

	else if (*endptr && toupper(*endptr)=='G')
 		n*=1024*1024*1024;

	else if (*endptr && toupper(*endptr)=='T')
	 	n*=(long long)1024*1024*1024*1024;

	else if (*endptr) {
		fprintf(stderr, "memory sizes must end in [KMGT]\n");
		exit(1);
	}

	if (!n) {
		fprintf(stderr, "sizes must be larger than zero\n");
		exit(1);
	}

	return n;
}
