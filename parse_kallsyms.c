/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2024 Qais Yousef */
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NUM_SYMBOLS		500000
#define LINE_SIZE		256
#define SYMBOL_LEN		128

struct symbol {
	void *address;
	char symbol[SYMBOL_LEN];
};

static struct symbol symbols[MAX_NUM_SYMBOLS];
static bool ready;


static int cmp(const void *a, const void *b)
{
	struct symbol *i = (struct symbol *)a;
	struct symbol *j = (struct symbol *)b;

	return i->address - j->address;
}

void parse_kallsyms(void)
{
	char line[LINE_SIZE] = { 0 };
	unsigned int i = 0;
	FILE *fp;

	fp = fopen("/proc/sys/kernel/kptr_restrict", "r");
	if (fp) {
		if (fgets(line, LINE_SIZE, fp)) {
			char *end_ptr;
			int val = strtol(line, &end_ptr, 0);
			if (val >= 2) {
				fprintf(stderr, "/proc/sys/kernel/kptr_restrict is %d, won't be able to parse kallsyms\n", val);
				fclose(fp);
				return;
			}
		}
		fclose(fp);
	} else {
		fprintf(stderr, "Failed to open /proc/sys/kernel/kptr_restrict, might fail to parse kallsyms\n");
	}

	fp = fopen("/proc/kallsyms", "r");
	if (!fp) {
		fprintf(stderr, "Failed to open /proc/kallsyms\n");
		return;
	}

	while (fgets(line, LINE_SIZE, fp) && i < MAX_NUM_SYMBOLS) {
		char *end_ptr;
		char *token;

		/* Get address first */
		token = strtok(line, " ");
		if (!token) {
			fprintf(stderr, "Error parsing kallsyms\n");
			break;
		}

		errno = 0;
		symbols[i].address = (void *)strtoul(line, &end_ptr, 16);
		if (errno != 0) {
			perror("Unsupported address value\n");
			break;
		}
		if (end_ptr == line) {
			fprintf(stderr, "address: no digits were found\n");
			break;
		}

		/* Skip 'type' field */
		token = strtok(NULL, " ");
		if (!token) {
			fprintf(stderr, "Error parsing kallsyms\n");
			break;
		}

		/* Get symbol name */
		token = strtok(NULL, " ");
		if (!token) {
			fprintf(stderr, "Error parsing kallsyms\n");
			break;
		}

		strncpy(symbols[i].symbol, token, SYMBOL_LEN-1);
		symbols[i].symbol[SYMBOL_LEN-1] = '0';
		i++;
	}

	fclose(fp);

	qsort(symbols, MAX_NUM_SYMBOLS, sizeof(struct symbol), cmp);

	ready = true;
}

static char *____find_kallsyms(unsigned int start, unsigned int end, void *address)
{
	while (start <= end-1) {
		if (symbols[start].address <= address &&
		    symbols[start+1].address >= address)
			return symbols[start].symbol;
		start++;
	}

	fprintf(stderr, "Couldn't find_kallsyms() for address: %p\n", address);

	return NULL;
}

static char *__find_kallsyms(unsigned int start, unsigned int pivot,
			     unsigned int end, void *address)
{
	if (end - start <= 100)
		return ____find_kallsyms(start, end, address);

	if (address == symbols[pivot].address)
		return symbols[pivot].symbol;

	if (address > symbols[pivot].address)
		return __find_kallsyms(pivot, pivot+(end-pivot)/2, end, address);

	if (address < symbols[pivot].address)
		return __find_kallsyms(start, start+(pivot-start)/2, pivot, address);

	fprintf(stderr, "Reached unexpeted path while searching for address: %p\n", address);

	return NULL;
}

char *find_kallsyms(void *address)
{
	if (!address || !ready)
		return NULL;

	return __find_kallsyms(0, MAX_NUM_SYMBOLS/2, MAX_NUM_SYMBOLS-1, address);
}
