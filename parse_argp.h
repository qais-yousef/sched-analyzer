/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2023 Qais Yousef */
#ifndef __PARSE_ARGS_H__
#define __PARSE_ARGS_H__
#include <argp.h>
#include <stdbool.h>

struct sa_opts {
	bool perfetto;
	bool csv;
};

extern struct sa_opts sa_opts;
extern const struct argp argp;

#endif /* __PARSE_ARGS_H__ */
