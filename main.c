#define EV_COMPAT3 0
#include <stdio.h>
#include "nl_proc.h"
int main (void){
	ev_loop *loop = EV_DEFAULT;

	cgroup_init();

	grouper_init(EV_A);

	nl_proc_init(EV_A);

	ev_run (loop, 0);

	return 0;
}

