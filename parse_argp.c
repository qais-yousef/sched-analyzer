/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2023 Qais Yousef */
#include "parse_argp.h"

const char *argp_program_version = "sched-analyzer 0.1";
const char *argp_program_bug_address = "<qyousef@layalina.io>";

static char doc[] =
"Extract scheduer data using BPF and emit them into csv file or perfetto events";

struct sa_opts sa_opts = {
	.perfetto = true,
	.csv = false,
};

static const struct argp_option options[] = {
	{ "perfetto", 'p', 0, 0, "Emit perfetto events and collect a trace. Requires traced and traced_probes to be running. (default)" },
	{ "csv", 'c', 0, 0, "Produce CSV files of collected data." },
	{ 0 },
};

static error_t parse_arg(int key, char *arg, struct argp_state *state)
{
	switch (key) {
	case 'p':
		sa_opts.perfetto = true;
		sa_opts.csv = false;
		break;
	case 'c':
		sa_opts.perfetto = false;
		sa_opts.csv = true;
		break;
	case ARGP_KEY_ARG:
		argp_usage(state);
		break;
	case ARGP_KEY_END:
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

const struct argp argp = {
	.options = options,
	.parser = parse_arg,
	.doc = doc,
};
