#include "sched.h"
#include "irq.h"
#include "printf.h"
#include "buddy_system.h"
#include "stdlib.h"

static struct task_struct init_task = INIT_TASK;
struct task_struct *current = &(init_task);
struct task_struct *task[NR_TASKS] = {
    &(init_task),
};
int nr_tasks = 1;

void sched_init() {
    printf("init_task.kernel_sp = %x\n", init_task.kernel_sp);
    tpidr_el1_init(&init_task);
}

void preempt_disable(void) {
    current->preempt_count++;
}

void preempt_enable(void) {
    current->preempt_count--;
}

void _schedule(void) {
    preempt_disable();
    int next, c;
    struct task_struct *p;
    while (1) {
        c = -1;
        next = 0;
        for (int i = 0; i < NR_TASKS; i++) {
            p = task[i];
            if (p && p->state == TASK_RUNNING && p->counter > c) {
                c = p->counter;
                next = i;
            }
        }
        if (c) {
            break;
        }
        for (int i = 0; i < NR_TASKS; i++) {
            p = task[i];
            if (p) {
                p->counter = (p->counter >> 1) + p->priority;
            }
        }
    }
    switch_to(task[next]);
    preempt_enable();
}

void schedule(void) {
    current->counter = 0;
    _schedule();
}

void switch_to(struct task_struct *next) {
    if (current == next)
        return;
    struct task_struct *prev = current;
    current = next;
    cpu_switch_to(prev, next);
}

void schedule_tail(void) {
    preempt_enable();
}

void timer_tick() {
    --current->counter;
    if (current->counter > 0 || current->preempt_count > 0) {
        return;
    }
    current->counter = 0;
    enable_irq();
    _schedule();
    disable_irq();
}

struct task_struct *current_thread() {
    return (struct task_struct *)get_current();
}

int get_new_pid() {
    struct task_struct* p;
	for (int i = 0; i < NR_TASKS; i++) {
		p = task[i];
		if (p == NULL) {
			return i;
		}	
	}
    return -1;
}

void kill_zombies() {
    struct task_struct* p;
    for (int i = 1; i < NR_TASKS; i++) {
        p = task[i];
        if (p && p->state == ZOMBIE) {
            buddy_system_free((unsigned long)(p->kernel_sp & ~(4096 - 1)));
            buddy_system_free((unsigned long)(p->user_sp & ~(4096 - 1)));
            free(p);
            task[i] = NULL;
        }
    }

    return;
}

void idle() {
    while (1) {
        // printf("idle\n");
        kill_zombies(); // eclaim threads marked as DEAD
        schedule();     // switch to any other runnable thread
    }
}