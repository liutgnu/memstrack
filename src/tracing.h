#include "utils.h"
#ifndef _MEMORY_TRACER_TRACING_LIB

#define is_task(tracenode_p) (tracenode_p->parent == NULL)
#define is_stacktop(tracenode_p) (tracenode_p->children == NULL)
// TODO: Remove redundant record, and when alloc happened extending a stacktop, remove old record and inherit.

extern struct HashMap TaskMap;

extern unsigned long trace_count;

struct Record {
	unsigned long addr;

	long pages_alloc;
	long pages_alloc_peak;
};

struct Tracenode {
	/* Tree node linking all neibors */
	struct TreeNode node;
	struct Tracenode *parent;
	struct Tracenode *children;

	union {
		unsigned long addr;
		char *symbol;
	};

	struct Record *record;
};

struct Task {
	struct HashNode node;
	struct Tracenode tracenode;

	int pid;
	char *task_name;
};

struct PageRecord {
	struct Tracenode *tracenode;
};

struct AllocRecord {
	struct TreeNode node;
	struct Tracenode *tracenode;

	unsigned long long addr;
	unsigned long size;
};

struct AllocEvent {
	unsigned long kvaddr;
	long bytes_req;
	long bytes_alloc;
};

struct PageEvent {
	unsigned long pfn;
	long pages_alloc;
};

void mem_tracing_init();
void update_record(struct Tracenode *record, struct PageEvent *pe);
void try_update_record(struct Tracenode *record, struct PageEvent *pe);
void load_kallsyms();
char* kaddr_to_sym(unsigned long long addr);

struct Tracenode* get_child_tracenode(struct Tracenode *root, char *symbol, unsigned long addr);
struct Tracenode* insert_child_tracenode(struct Tracenode *root, struct Tracenode *src);
struct Tracenode* get_or_new_child_tracenode(struct Tracenode *root, char *callsite, unsigned long addr);

struct Task* get_task(struct HashMap *map, char* task_name, int pid);
struct Task* insert_task(struct HashMap *map, struct Task* task);
struct Task* get_or_new_task(struct HashMap *map, char* task_name, int pid);

void final_report(struct HashMap *map, int task_limit);

#define _MEMORY_TRACER_TRACING_LIB 1

#endif /* ifndef _MEMORY_TRACER_TRACING_LIB */
