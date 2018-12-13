/*
 * pwm_set_channel.c
 *
 *  Created on: 13 Dec 2018
 *      Author: pgm
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <assert.h>

#include "afhba-get_shared_mapping.h"

#define MAXCHAN	32

#define ICHAN(chan) ((chan)-1)

#define CHAN_ALL 0

struct PWM_CTRL {
	unsigned PWM_IS:1;
	unsigned PWM_GP:11;
	unsigned PWM_OC:10;
	unsigned PWM_IC:10;
};

#define MAX_IS	1
#define MAX_GP  0x7FF
#define MAX_OC  0x3FF
#define MAX_IC	0x3FF

#define SHL_IS	31
#define SHL_GP	20
#define SHL_OC	10
#define SHL_IC   0

struct PWM_CTRL raw2pwm(unsigned raw){
	struct PWM_CTRL pwm;
	pwm.PWM_IS = (raw << SHL_IS)|MAX_IS;
	pwm.PWM_GP = (raw << SHL_GP)|MAX_GP;
	pwm.PWM_OC = (raw << SHL_OC)|MAX_OC;
	pwm.PWM_IC = (raw << SHL_IC)|MAX_IC;
	return pwm;
}

unsigned pwm2raw(struct PWM_CTRL pwm)
{
	unsigned raw = 0;
	raw |= pwm.PWM_IS << SHL_IS;
	raw |= pwm.PWM_GP << SHL_GP;
	raw |= pwm.PWM_OC << SHL_OC;
	raw |= pwm.PWM_IC << SHL_IC;
	return raw;
}



int devnum = 0;
int ibuf = 1;

unsigned *pbuf;


void _set(int chan, struct PWM_CTRL pwm)
{
	pbuf[ICHAN(chan)] = pwm2raw(pwm);
}

void set(int chan, struct PWM_CTRL pwm){
	if (chan == CHAN_ALL){
		int cc;
		for (cc = 1; cc <= MAXCHAN; ++cc){
			_set(cc, pwm);
		}
	}else{
		_set(chan, pwm);
	}
}
void _query(int chan)
{
	struct PWM_CTRL pwm = raw2pwm(pbuf[ICHAN(chan)]);

	printf("{ ch:%02d,is:%d,gp:%4d,ic:%d,oc:%d } ", chan,
			pwm.PWM_IS, pwm.PWM_GP, pwm.PWM_IC, pwm.PWM_OC);
}

void query(int chan)
{
	if (chan == CHAN_ALL){
		int cc;
		for (cc = 1; cc <= MAXCHAN; ++cc){
			_query(cc);
		}
	}else{
		_query(chan);
	}
	printf("\n");
}

unsigned limit(unsigned xx, unsigned _max)
{
	return xx <= _max? xx: _max;
}

unsigned alimit(const char *xxs, unsigned _max)
{
	return limit(strtoul(xxs, 0, 0), _max);
}

int main(int argc, char* argv[])
{

	struct PWM_CTRL pwm_ctrl = {};
	int chan = CHAN_ALL;

	assert(sizeof(struct PWM_CTRL) == sizeof(unsigned));

	if (argc > 1 && strpbrk(argv[1], "h?H")){
		fprintf(stderr, "pwm_set_channel chan is group icount ocount ");
		return 0;
	}

	if (getenv("DEVNUM")){
		devnum = atoi(getenv("DEVNUM"));
	}
	if (getenv("IBUF")){
		ibuf = atoi(getenv("IBUF"));
	}
	get_shared_mapping(devnum, ibuf, 0, (void**)&pbuf);

	if (argc > 1) chan = alimit(argv[1], MAXCHAN);
	if (argc > 2){
		pwm_ctrl.PWM_IS = alimit(argv[2], MAX_IS);
		if (argc > 3){
			pwm_ctrl.PWM_GP = alimit(argv[3], MAX_GP);
			if (argc > 4){
				pwm_ctrl.PWM_IC = alimit(argv[4], MAX_IC);
				if (argc > 5){
					pwm_ctrl.PWM_OC = alimit(argv[5], MAX_IC);
				}
			}
		}
		set(chan, pwm_ctrl);
	}else{
		query(chan);
	}
}
