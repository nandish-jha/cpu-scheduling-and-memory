/*
AAKASH JANA AAJ284 11297343
NANDISH JHA NAJ474 11282001
*/



// Simple EDF scheduler demo for CMPT 332 A3
// Spawns several "jobs" with different deadlines, priorities,
// and start times, then prints the order in which they finish.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define TASK_COUNT 4

// Small arithmetic loop to burn CPU time.
static void
do_heavy_math(int loops) {
  int acc = 0;
  for (int i = 0; i < loops; i++) {
    acc = acc + (i * i);
  }
  // Prevent compiler from optimizing this out
  if (acc == -1) {
    printf("impossible value: %d\n", acc);
  }
}

static void
run_child_task(int task_id, int task_deadline, int task_priority,
               int start_delay_ticks, int work_units) {
  struct sched_attr attrs;
  struct sched_attr check_attrs;

  // Convert relative delay into absolute start_time based on current ticks.
  int now = uptime();
  attrs.deadline   = task_deadline;
  attrs.priority   = task_priority;
  attrs.start_time = now + start_delay_ticks;

  printf("job %d: set deadline=%d, prio=%d, start_time=%d (now=%d, delay=%d)\n",
         task_id, attrs.deadline, attrs.priority,
         attrs.start_time, now, start_delay_ticks);

  if (setSchedAttr(&attrs) < 0) {
    printf("job %d: setSchedAttr failed\n", task_id);
    exit(1);
  }

  // Sanity check that kernel stored what we asked for.
  if (getSchedAttr(&check_attrs) < 0) {
    printf("job %d: getSchedAttr failed\n", task_id);
    exit(1);
  }

  printf("job %d: kernel reports deadline=%d, prio=%d, start_time=%d\n",
         task_id,
         check_attrs.deadline,
         check_attrs.priority,
         check_attrs.start_time);

  // Simulate some computation.
  printf("job %d: starting work (%d units)\n", task_id, work_units);
  do_heavy_math(work_units);
  printf("job %d: finished work (deadline=%d, prio=%d, start_time=%d)\n",
         task_id, task_deadline, task_priority, attrs.start_time);

  exit(0);
}

static void
edf_demo(void)
{
  int child_pids[TASK_COUNT];
  int status;

  printf("\n--- EDF scheduler demonstration ---\n");
  printf("Spawning %d jobs with different deadlines, priorities, "
         "and start times.\n", TASK_COUNT);
  printf("Earlier deadlines (and already-released start times) "
         "should generally finish first.\n\n");

  // Each "job" configuration:
  // deadline, priority, start_delay_ticks, work_units
  struct job_cfg {
    int d;
    int prio;
    int start_delay;
    int work;
  } jobs[TASK_COUNT] = {
    // lower deadline => higher EDF urgency
    // higher priority => tie-breaker when deadlines equal
    { 80,  1, 10,  60000000 },  // job 0: later release, medium deadline
    { 40,  2,  0,  50000000 },  // job 1: earliest deadline, starts immediately
    { 60,  0,  5,  55000000 },  // job 2: middle deadline, small delay
    { 80,  0,  0,  65000000 },  // job 3: same deadline as 0,
                                 // lower priority, more work
  };

  // Create child jobs.
  for (int i = 0; i < TASK_COUNT; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("parent: fork failed for job %d\n", i);
      exit(1);
    }

    if (pid == 0) {
      // Child path.
      run_child_task(i,
                     jobs[i].d,
                     jobs[i].prio,
                     jobs[i].start_delay,
                     jobs[i].work);
      // not reached
      exit(0);
    } else {
      // Parent path.
      child_pids[i] = pid;
      printf("parent: created job %d (pid=%d): d=%d, prio=%d, "
             "start_delay=%d, work=%d\n",
             i, pid,
             jobs[i].d,
             jobs[i].prio,
             jobs[i].start_delay,
             jobs[i].work);
    }
  }

  printf("\nparent: waiting for children to finish...\n");
  printf("hint: jobs with earlier deadlines and earlier start times "
         "should tend to exit first.\n\n");

  int finished_count = 0;
  for (int i = 0; i < TASK_COUNT; i++) {
    int done_pid = wait(&status);
    if (done_pid < 0) {
      printf("parent: wait returned error\n");
      break;
    }

    // Map pid back to job index.
    int which = -1;
    for (int j = 0; j < TASK_COUNT; j++) {
      if (child_pids[j] == done_pid) {
        which = j;
        break;
      }
    }

    finished_count++;
    if (which >= 0) {
      printf("parent: job %d (pid=%d) completed as #%d (exit status=%d)\n",
             which, done_pid, finished_count, status);
    } else {
      printf("parent: unknown child pid=%d completed as #%d (status=%d)\n",
             done_pid, finished_count, status);
    }
  }

  printf("\n--- EDF demo done ---\n");
  printf("Inspect the completion order above vs. the configured "
         "deadlines/start times.\n");
}

int
main(void)
{
  edf_demo();
  exit(0);
}
