#include <poll.h>

extern int perf_events_num;
extern struct PerfEvent *perf_events;

#define perf_fd_num perf_events_num

int perf_handling_init();
int perf_handling_clean();
int perf_handling_start();
int perf_handling_process();

inline static int perf_count_fds(void) { return perf_events_num; }
int perf_apply_fds(struct pollfd *fds);