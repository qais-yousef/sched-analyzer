/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2024 Qais Yousef */
#ifndef __PARSE_KALLSYMS_H__
#define __PARSE_KALLSYMS_H__

void parse_kallsyms(void);
char *find_kallsyms(void *address);

#endif /* __PARSE_KALLSYMS_H__ */
