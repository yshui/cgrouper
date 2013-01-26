#include <proc/readproc.h>
#include <sys/queue.h>
#include "grouper.h"
#include "cgroup.h"
static inline void _freeproc (proc_t *p) {
	if(!p)
		return;
	if(p->environ)
		free((void*)*p->environ);
	if(p->cmdline)
		free((void*)*p->cmdline);
	if(p->cgroup)
		free((void*)*p->cgroup);
	if(p->supgid)
		free(p->supgid);
	if(p->supgrp)
		free(p->supgrp);
	free(p);
}
static inline int _group_process(proc_t *p){
	char *cgroup_name;
	asprintf(&cgroup_name, "grp_%d", p->tty);
	struct cgroup *cgr = cgroup_get("cpu", cgroup_name, true);
	int ret = cgroup_attach(cgr, p->tgid);
	free(cgroup_name);
	cgroup_put(cgr);
	free(cgr);
	return ret;
}
int regroup_process(int pid){
	pid_t pids[2]={pid, 0};
	PROCTAB *PT=openproc(PROC_FILLCOM|PROC_FILLMEM|PROC_FILLSTAT|PROC_FILLSTATUS|PROC_PID, pids);
	if(!PT)
		return 0;
	proc_t *pr=readproc(PT, NULL);
	if(!pr){
		free(PT);
		return 0;
	}
	int ret = _group_process(pr);
	_freeproc(pr);
	closeproc(PT);
	return ret;
}
int group_new_process(int pid){
	return regroup_process(pid);
}
int group_delete_process(int pid){
	return EXIT_SUCCESS;
}
int grouper_init(EV_P){
	proc_t **ptable = readproctab(PROC_FILLMEM|PROC_FILLCOM|PROC_FILLSTAT|PROC_FILLSTATUS);
	if(!ptable)
		return EXIT_FAILURE;

	int i;
	for(i=0; ptable[i]; i++){
		_group_process(ptable[i]);
		_freeproc(ptable[i]);
	}

	free(ptable);
	return EXIT_SUCCESS;
}
