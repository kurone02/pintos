#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "synch.h"
#include "filesys/filesys.h"

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING,       /* About to be destroyed. */
    #ifdef USERPROG    
      THREAD_EXITING    /* About to be freed by parent. */
    #endif
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */
#define NICE_DEFAULT 0                  /* Default nice value. */
#define RECENT_CPU_DEFAULT 0            /* Default recent_cpu value. */
#define LOAD_AVG_DEFAULT 0              /* Default load_avg value. */
#define FIXED_POINT_1 (1 << 14)         /* The number 1.0 in fix-point format */

/* A struct to represent real numbers
   using 17.14 fixed-point format
*/
typedef int fp;

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    struct list_elem allelem;           /* List element for all threads list. */

    int64_t wakeup_tick;                /* Tick till wake up */

    int nice;                           /* The nice value of the thread */
    fp recent_cpu;                      /* The recent cpu usage in fixed-point format */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */
    int initial_priority;
    struct lock *wait_on_lock;          /* The lock that the thread is waiting on */
    struct list donations;              /* The threads to donate the priority to */
    struct list_elem d_elem;            /* List element on donation list */

    /* Shared between thread.c and userprog/syscall.c. */
    struct lock exit_lock;              /* The lock for exit and wait */
    int exit_status;                    /* The thread exit code */
    tid_t parent_tid;                   /* The identifier of the parent */
    struct list_elem siblings;          /* The pointer to the next and previous siblings */
    struct list children;               /* The list of children */
    struct list_elem child_elem;        /* The pointer to the next and previous siblings */
    struct condition exit_signal;       /* The exit signal */

    /* For file descriptors */
    struct file *fdt[MAX_FILE];         /* The file descriptor table*/
    int next_fd;

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };

struct tstart_properties {
   tid_t parent_tid;                   /* Parent's thread identifier. */
   char *file_name;                    /* The file name */
   char *args;                         /* The arguments to be parsed */
   struct semaphore wait;              /* The semaphore to wait for starting */
   struct thread *this_thread;         /* The pointer to this thread */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

/* Load average for MLFQS */
fp load_avg;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

int64_t get_min_sleep_tick(void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);
void thread_sleep (int64_t);
void thread_wakeup(int64_t);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

bool thread_cmp_priority(const struct list_elem*, const struct list_elem*, void*);
void reset_priority(void);
void thread_yield_if_not_max(void);
struct thread* thread_highest_priority(void);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

/* Fixed-point arithmetics */
fp int_to_fp(int);
int fp_to_int_0(fp);
int fp_to_int_nearest(fp);

fp add_fp(fp, fp);
fp sub_fp(fp, fp);

fp add_fp_int(fp, int);
fp sub_fp_int(fp, int);

fp mul_fp(fp, fp);
fp div_fp(fp, fp);

fp mul_fp_int(fp, int);
fp div_fp_int(fp, int);


/* MLFQS utilities */
int mlfqs_get_priority(fp, int);
fp mlfqs_new_recent_cpu(fp, int);
fp mlfqs_new_load_avg(fp);
void mlfqs_increase_recent_cpu(void);
void mlfqs_recalculate_all_priority(void);
void mlfqs_recalculate_all_recent_cpu(void);

#endif /* threads/thread.h */
