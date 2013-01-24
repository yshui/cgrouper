#pragma once
#include "common.h"

struct cgroup;

struct cgroup *cgroup_get(const char *, const char *, bool);
int cgroup_attach(struct cgroup *, int);
