/* ------------------------------------------------------------------------- *
 * afhba-llcontrol-example.c
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2014 Peter Milne, D-TACQ Solutions Ltd
 *                      <peter dot milne at D hyphen TACQ dot com>
 *                         www.d-tacq.com
 *   Created on: 18 September 2014
 *    Author: pgm
 *                                                                           *
 *  This program is free software; you can redistribute it and/or modify     *
 *  it under the terms of Version 2 of the GNU General Public License        *
 *  as published by the Free Software Foundation;                            *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *  GNU General Public License for more details.                             *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  along with this program; if not, write to the Free Software              *
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/* ------------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#define HB_FILE "/dev/afhba.0.loc"
#define HB_LEN	0x1000

#define LOG_FILE	"afhba.log"

void* host_buffer;
int fd;
int nsamples = 10000000;		/* 10s at 1MSPS */
int sched_fifo_priority = 1;
int verbose;
FILE* fp_log;
char* action_name = "WriteAction";

/* ACQ425 */

#define NCHAN	16
#define NSHORTS	32
#define VI_LEN 	(NSHORTS*sizeof(short))
#define SPIX	(NCHAN*sizeof(short)/sizeof(unsigned))

#if 0
#warning MONITOR host_buffer
#define CH01 (((volatile short*)host_buffer)[0])
#define CH01 (((volatile short*)host_buffer)[0])
#define CH02 (((volatile short*)host_buffer)[1])
#define CH03 (((volatile short*)host_buffer)[2])
#define CH04 (((volatile short*)host_buffer)[3])
#define TLATCH (((volatile unsigned*)host_buffer)[SPIX])	/* actually, sample counter */
#define SPAD1	(((volatile unsigned*)host_buffer)[SPIX+1])   /* user signal from ACQ */
#else
#define CH02 (((volatile short*)local_buffer)[1])
#define CH03 (((volatile short*)local_buffer)[2])
#define CH04 (((volatile short*)local_buffer)[3])
#define TLATCH (((volatile unsigned*)local_buffer)[SPIX])	/* actually, sample counter */
#define SPAD1	(((volatile unsigned*)local_buffer)[SPIX+1])   /* user signal from ACQ */
#endif

class Action {
public:
	Action() {}
	virtual void onSample(void* sample) = 0;
	virtual ~Action() {}
};

void get_mapping() {
	fd = open(HB_FILE, O_RDWR);
	if (fd < 0){
		perror("HB_FILE");
		exit(errno);
	}
	host_buffer = mmap(0, HB_LEN, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (host_buffer == (caddr_t)-1 ){
		perror( "mmap" );
	        exit(errno);
	}
}

void goRealTime(void)
{
	struct sched_param p = {};
	p.sched_priority = sched_fifo_priority;



	int rc = sched_setscheduler(0, SCHED_FIFO, &p);

	if (rc){
		perror("failed to set RT priority");
	}
}

void write_action(void *data)
{
	fwrite(data, sizeof(short), NSHORTS, fp_log);
}

void check_tlatch_action(void *local_buffer)
{
	static unsigned tl0;

	unsigned tl1 = TLATCH;
	if (tl1 != tl0+1){
		printf("%d => %d\n", tl0, tl1);
	}
	tl0 = tl1;
}


void ui(int argc, char* argv[])
{
        if (getenv("RTPRIO")){
		sched_fifo_priority = atoi(getenv("RTPRIO"));
        }
	if (getenv("VERBOSE")){
		verbose = atoi(getenv("VERBOSE"));
	}
	if (argc > 1){
		nsamples = atoi(argv[1]);
	}
	if (getenv("ACTION")){
		action_name = getenv("ACTION");
	}
}

void setup()
{
	get_mapping();
	goRealTime();
	fp_log = fopen("llcontrol.log", "w");
	if (fp_log == 0){
		perror("llcontrol.log");
		exit(1);
	}
}

void run(Action* action)
{
	short* local_buffer = new short[NSHORTS];
	unsigned tl0 = 0xdeadbeef;	/* always run one dummy loop */
	unsigned spad1_0 = SPAD1;
	unsigned spad1_1;
	unsigned tl1;
	int sample;
	int println = 0;

	mlockall(MCL_CURRENT);
	memset(host_buffer, 0, VI_LEN);

	for (sample = 0; sample <= nsamples; ++sample, tl0 = tl1){
		memcpy(local_buffer, host_buffer, VI_LEN);
		while((tl1 = TLATCH) == tl0){
			sched_yield();
			memcpy(local_buffer, host_buffer, VI_LEN);
		}
		if (verbose){
			if (sample%10000 == 0){
				if (println == 0){
					printf("[%10u] ", sample);
					println = 1;
				}
				printf("%10u ", tl1);
			}
			if (spad1_1 != spad1_0){
				if (println == 0){
					printf("[%d] ", sample);
					println = 1;
				}
				printf("\t%u => %u ", sample, spad1_0, spad1_1);
				spad1_0 = spad1_1;
			}
			if (println){
				printf("\n");
				println = 0;
			}
		}
		action->onSample(local_buffer);
	}
}

void closedown() {
	munmap(host_buffer, HB_LEN);
	close(fd);
	fclose(fp_log);
}

class WriteAction: public Action {
public:
        virtual void onSample(void* sample) {};
        virtual ~WriteAction() {}
};

class CheckTlatchAction: public Action {
public:
        virtual void onSample(void* sample) {};
        virtual ~CheckTlatchAction() {}
};

class NullAction: public Action {
	int nsamples;
public:
	NullAction() : nsamples(0) {}
        virtual void onSample(void* sample) {

	};
        virtual ~NullAction() {
		printf("NullAction: nsamples:%d\n", nsamples);
	}
};

int main(int argc, char* argv[])
{
	ui(argc, argv);

	Action *action;
	if (strcmp(action_name, "WriteAction") == 0){
		action = new WriteAction;
	}else if (strcmp(action_name, "CheckTlatchAction") == 0){
		action = new CheckTlatchAction;
	}else{
		action = new NullAction;
	}
	setup();
	printf("ready for data\n");
	run(action);
	printf("finished\n");
	delete action;
	closedown();
}
