#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>

#include "../memstrack.h"
#include "../tracing.h"
#include "ftrace.h"

#define MAX_LINE 4096
#define MAX_SYMBOL 4096

static FILE* ftrace_file;
// For ftrace
char ftrace_line[MAX_LINE];
unsigned int lookahead;

static struct Task *task;
static struct PageEvent pevent;
static char *event;

static void __ignore_stacktrace() {
	// Igore possible following stacktrace
	do {
		lookahead = !!ftrace_read_next_valid_line(ftrace_line, MAX_LINE, ftrace_file);
	} while (strncmp(ftrace_line, FTRACE_STACK_TRACE_SIGN, strlen(FTRACE_STACK_TRACE_SIGN) - 1) == 0);

}

static struct Tracenode* __process_stacktrace() {
	lookahead = !!ftrace_read_next_valid_line(ftrace_line, MAX_LINE, ftrace_file);
	if (!lookahead) {
		// EOF
		return NULL;
	}
	if (strncmp(ftrace_line, FTRACE_STACK_TRACE_SIGN, strlen(FTRACE_STACK_TRACE_SIGN) - 1) != 0) {
		// Read ahead end of trace
		return NULL;
	}

	struct Tracenode *tp = NULL;
	char callsite[MAX_SYMBOL], *callsite_arg = NULL;
	int callsite_len = 0;
	callsite_arg = ftrace_line + strlen(FTRACE_STACK_TRACE_SIGN);
	callsite_len = strlen(callsite_arg);
	strcpy(callsite, callsite_arg);
	callsite[callsite_len - 1] = '\0';

	// Process next traceline
	tp = __process_stacktrace();

	if (tp == NULL) {
		tp = get_or_new_child_tracenode(
					to_tracenode(task),
					callsite, 0);
	} else {
		tp = get_or_new_child_tracenode(tp, callsite, 0);
	}

	return tp;
}

int ftrace_handling_init() {
	char setup_events[1024], *print_header;
	print_header = setup_events;

	if (m_slab) {
		print_header += sprintf(print_header, "kmem:kmem_cache_alloc ");
	}
	if (m_page) {
		print_header += sprintf(print_header, "kmem:mm_page_alloc ");
	}
	return ftrace_setup(&ftrace_file, setup_events);
}

int ftrace_apply_fds(struct pollfd *fds) {
	fds[0].fd = fileno(ftrace_file);
	fds[0].events = POLLIN;
	return 0;
}

int ftrace_handling_clean() {
	return ftrace_cleanup(&ftrace_file);
}

int ftrace_handle_mm_page_alloc() {
	char *page_arg, *pfn_arg, *order_arg, *migratetype_arg, *gfp_flags_arg;
	page_arg = strtok(NULL, " ") + sizeof("page=") - 1;
	pfn_arg = strtok(NULL, " ") + sizeof("pfn=") - 1;
	order_arg = strtok(NULL, " ") + sizeof("order=") - 1;
	migratetype_arg = strtok(NULL, " ") + sizeof("migratetype=") - 1;
	gfp_flags_arg = strtok(NULL, " ") + sizeof("gfp_flags=") - 1;

	int order, pfn;
	sscanf(order_arg, "%u", &order);
	sscanf(pfn_arg, "%u", &pfn);

	pevent.pfn = pfn;
	pevent.pages_alloc = 1;

	for (int i = 0; i < order; i++) {
		pevent.pages_alloc *= 2;
	}

	update_record(to_tracenode(task), &pevent);
	return 0;
}

// int ftrace_handle_kmem_cache_alloc() {
// 	unsigned long long ptr;
// 	char *ptr_arg, *bytes_req_arg, *bytes_alloc_arg;
// 	ptr_arg = strtok(NULL, " ") + sizeof("ptr=") - 1;
// 	bytes_req_arg = strtok(NULL, " ") + sizeof("bytes_req=") - 1;
// 	bytes_alloc_arg = strtok(NULL, " ") + sizeof("bytes_alloc=") - 1;
//
// 	sscanf(bytes_req_arg, "%u", &aevent.bytes_req);
// 	sscanf(bytes_alloc_arg, "%u", &aevent.bytes_alloc);
// 	sscanf(ptr_arg, "%llx", &aevent.kvaddr);
//
// 	update_record(to_tracenode(task), NULL, &aevent);
// 	return 0;
// }

static void do_ftrace_process() {
	struct Tracenode *tn;

	char *pre_last = strstr(ftrace_line, ": ");
	if (pre_last == NULL) {
		log_error("Invalid Line: %s\n", ftrace_line);
		__ignore_stacktrace();
		return;
	}

	char *last = strstr(pre_last + 2, ": "), *tmp;
	if (last) {
		tmp = strstr(last + 2, ": ");
		while (tmp) {
			pre_last = last;
			last = tmp;
			tmp = strstr(last + 2, ": ");
		}
	} else {
		last = pre_last;
		pre_last = NULL;
	}
	if (strncmp(last + 2, FTRACE_STACK_TRACE_EVENT, strlen(FTRACE_STACK_TRACE_EVENT) - 1) == 0) {
		if (!task) {
			log_debug("Got unexpected stacktrace!\n");
			__ignore_stacktrace();
		} else {
			tn = __process_stacktrace();
			update_record(tn, &pevent);
			task = NULL;
		}
	} else {
		char *task_info = NULL, *event_info = NULL, *pid_arg = NULL;
		unsigned int pid = 0;

		task_info = ftrace_line;
		event_info = pre_last;
		pid_arg = pre_last;

		while(pid_arg[0] != '-') {
			pid_arg --;
			if (pid_arg <= ftrace_line) {
				pid_arg = NULL;
				break;
			}
		}

		if (pid_arg) {
			pid_arg[0] = '\0';
			pid_arg++;
			sscanf(pid_arg, "%u", &pid);
		}

		event_info[0] = '\0';
		event_info += 2;

		while(task_info[0] == ' ' || task_info[0] == '\t') {
			task_info++;
		}

		if (!task_info || !event_info || !pid_arg) {
			log_warn("Invalid line:\n%s\n", ftrace_line);
			return;
		}

		// char *event, *callsite;
		event = strtok(event_info, " ");
		// callsite = strtok(NULL, " ");
		task = get_or_new_task(&TaskMap, task_info, pid);

		// if (strncmp(event, "kmem_cache_alloc:", sizeof("kmem_cache_alloc:") - 2) == 0) {
		// 	ftrace_handle_kmem_cache_alloc();
		// }
		if (strncmp(event, "mm_page_alloc:", sizeof("kmem_page_alloc:") - 2) == 0) {
			ftrace_handle_mm_page_alloc();
		} else {
			log_warn("Unexpected event %s\n", event_info);
		}
	}
}

int ftrace_handling_process() {
	while (lookahead) {
		lookahead = 0;
		do_ftrace_process();
	}

	ftrace_read_next_valid_line(ftrace_line, MAX_LINE, ftrace_file);
	do_ftrace_process();
	return 0;
}