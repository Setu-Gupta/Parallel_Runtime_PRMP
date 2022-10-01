#ifndef _WORK_STEALING_H
#define _WORK_STEALING_H

#include <abt.h>

static void create_pools(int num, ABT_pool *pools);
static void create_scheds(int num, ABT_pool *pools, ABT_sched *scheds);

#endif