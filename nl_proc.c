#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/connector.h>
#include <linux/cn_proc.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "nl_proc.h"

/*
 * connect to netlink
 * returns netlink socket, or -1 on error
 */
static int nl_connect()
{
	int rc;
	int nl_sock;
	struct sockaddr_nl sa_nl;

	nl_sock = socket(AF_NETLINK, SOCK_DGRAM|SOCK_NONBLOCK, NETLINK_CONNECTOR);
	if (nl_sock == -1) {
		perror("socket");
		return -1;
	}

	sa_nl.nl_family = AF_NETLINK;
	sa_nl.nl_groups = CN_IDX_PROC;
	sa_nl.nl_pid = getpid();

	rc = bind(nl_sock, (struct sockaddr *)&sa_nl, sizeof(sa_nl));
	if (rc == -1) {
		perror("bind");
		close(nl_sock);
		return -1;
	}

	return nl_sock;
}

/*
 * subscribe on proc events (process notifications)
 */
static int set_proc_ev_listen(int nl_sock, bool enable)
{
	int rc;
	struct __attribute__ ((aligned(NLMSG_ALIGNTO))) {
		struct nlmsghdr nl_hdr;
		struct __attribute__ ((__packed__)) {
			struct cn_msg cn_msg;
			enum proc_cn_mcast_op cn_mcast;
		};
	} nlcn_msg;

	memset(&nlcn_msg, 0, sizeof(nlcn_msg));
	nlcn_msg.nl_hdr.nlmsg_len = sizeof(nlcn_msg);
	nlcn_msg.nl_hdr.nlmsg_pid = getpid();
	nlcn_msg.nl_hdr.nlmsg_type = NLMSG_DONE;

	nlcn_msg.cn_msg.id.idx = CN_IDX_PROC;
	nlcn_msg.cn_msg.id.val = CN_VAL_PROC;
	nlcn_msg.cn_msg.len = sizeof(enum proc_cn_mcast_op);

	nlcn_msg.cn_mcast = enable ? PROC_CN_MCAST_LISTEN : PROC_CN_MCAST_IGNORE;

	rc = send(nl_sock, &nlcn_msg, sizeof(nlcn_msg), 0);
	if (rc == -1) {
		perror("netlink send");
		return -1;
	}

	return 0;
}

/*
 * handle a single process event
 */
void handle_proc_ev(EV_P_ ev_io *w, int revents){
	int rc;
	int nl_sock = w->fd;
	struct __attribute__ ((aligned(NLMSG_ALIGNTO))) {
		struct nlmsghdr nl_hdr;
		struct __attribute__ ((__packed__)) {
			struct cn_msg cn_msg;
			struct proc_event proc_ev;
		};
	} nlcn_msg;

	while (1) {
		rc = recv(nl_sock, &nlcn_msg, sizeof(nlcn_msg), 0);

		if (rc == 0) {
			/* shutdown? */
			ev_break(EV_A_ EVBREAK_ALL);
			return;
		} else if (rc == -1) {
			if(errno == EINTR)
				continue;
			if(errno == EAGAIN)
				break;
			perror("netlink recv");
			ev_break(EV_A_ EVBREAK_ALL);
			return;
		}
		switch (nlcn_msg.proc_ev.what) {
			case PROC_EVENT_NONE:
				printf("set mcast listen ok\n");
				break;
			case PROC_EVENT_FORK:
				group_new_process(nlcn_msg.proc_ev.event_data.fork.child_pid);
				break;
			case PROC_EVENT_EXEC:
				regroup_process(nlcn_msg.proc_ev.event_data.exec.process_pid);
				break;
			case PROC_EVENT_UID:
			case PROC_EVENT_GID:
				regroup_process(nlcn_msg.proc_ev.event_data.id.process_pid);
				break;
			case PROC_EVENT_EXIT:
				group_delete_process(nlcn_msg.proc_ev.event_data.exit.process_pid);
				break;
			default:
				printf("unhandled proc event\n");
				break;
		}
	}
}

static ev_io nl_watcher;
int nl_proc_init(EV_P){
	int nl_sock;

	nl_sock = nl_connect();
	if (nl_sock == -1)
		return EXIT_FAILURE;

	int rc = set_proc_ev_listen(nl_sock, true);
	if(rc == -1){
		rc = EXIT_FAILURE;
		goto out;
	}
	ev_io_init(&nl_watcher, handle_proc_ev, nl_sock, EV_READ);
	ev_io_start(EV_A_ &nl_watcher);

	return EXIT_SUCCESS;

out:
	close(nl_sock);
	return rc;
}
