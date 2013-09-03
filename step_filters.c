/*
 * step_filters.c
 *
 *  Created on: Sep 2, 2013
 *      Author: heavey
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "step_filters.h"
#include "list.h"

struct stepfilter_list {
	struct list_node list;
	char *msg;
};

static struct stepfilter_list *sil; /* step into list */
static struct stepfilter_list *sol; /* step over list */
static struct stepfilter_list *srl; /* step return list */

static struct stepfilter_list *stepfilter_list_malloc(void) {
	struct stepfilter_list *p = NULL;
	p = (struct stepfilter_list *)malloc(sizeof(struct stepfilter_list));
	if(p == NULL)
		return NULL;

	list_init(&p->list);
	p->msg = NULL;
	return p;
}

static void stepfilter_list_free(struct stepfilter_list *p) {
	if (p != NULL) {
		if(p->msg != NULL)
			free(p->msg);
		free(p);
	}
}

static void stepfilter_list_destroy(struct stepfilter_list **head) {
	struct list_node *item = &(*head)->list;
	struct list_node *p = NULL;
	if(item == NULL)
		return;

	for(p = item->next; p != item; p = item->next) {
		list_remove(p);
		stepfilter_list_free((struct stepfilter_list *)p);
	}
	stepfilter_list_free((struct stepfilter_list *)item);
	*head = NULL;
}

static void stepfiler_list_print_callback(struct list_node *item) {
	struct stepfilter_list *p = NULL;
	p = (struct stepfilter_list *)item;
	fprintf(stderr, "msg = %s\n", p->msg);
}

static void stepfiler_list_print(struct stepfilter_list *head) {
	list_travel(&head->list, stepfiler_list_print_callback);
}

static int stepfilter_list_add(struct stepfilter_list **head, char *msg) {
	struct stepfilter_list *list = NULL;
	//new node
	list = stepfilter_list_malloc();
	list->msg = (char *)malloc(strlen(msg) + 1);
	if(list->msg == NULL) {
		stepfilter_list_free(list);
		return -1;
	}
	strcpy(list->msg, msg);

	if(*head == NULL)
		*head = list;
	else
		list_insert(&list->list, &(*head)->list);

	return 0;
}

int filters_init(char *pathname) {
	FILE *fp = NULL;
	struct stepfilter_list **clist = NULL;

	fp = fopen(pathname, "r");
	if(fp == NULL)
		return -1;
	while (1) {
		int len = 0;
		char *p = NULL;
		char line[1024];

		if (fgets(line, sizeof(line), fp) == NULL)
			break;

		//ead /n
		p = &line[strlen(line) - 1];
		if(*p == '\n')
			*p = '\0';

		if(line[0] == '[') {
			const char si_header[] = "[step-into]";
			const char so_header[] = "[step-over]";
			const char sr_header[] = "[step-return]";
			if (strncmp(line, si_header, sizeof(si_header) - 1) == 0) {
				clist = &sil;
			} else if (strncmp(line, so_header, sizeof(so_header) - 1) == 0) {
				clist = &sol;
			} else if (strncmp(line, sr_header, sizeof(sr_header) - 1) == 0) {
				clist = &srl;
			}
			continue;
		}

		if (clist == NULL)
			continue;

		//eat space
		for(p = line; *p == ' ' && p < &line[sizeof(line)]; p++) {
			;
		}

		len = strlen(p);
		if (len < 3)
			continue;

		stepfilter_list_add(clist, p);
	}
	fclose(fp);
	fp = NULL;

#if 1
	fprintf(stderr, "[step-into]\n");
	stepfiler_list_print(sil);
	fprintf(stderr, "[step-over]\n");
	stepfiler_list_print(sol);
	fprintf(stderr, "[step-return]\n");
	stepfiler_list_print(srl);
#endif
	return 0;
}

void filters_destroy(void) {
	stepfilter_list_destroy(&sil);
	stepfilter_list_destroy(&sol);
	stepfilter_list_destroy(&srl);
}

static int check_filter(struct stepfilter_list *head, char *file) {
	struct list_node *item = NULL;
	struct list_node *p = NULL;
	if(head == NULL)
		return -1;

	item = &head->list;
	if (strstr(file, head->msg) != NULL)
		return 0;
	for(p = item->next; p != item; p = p->next) {
		struct stepfilter_list *t = NULL;
		t = (struct stepfilter_list *)p;
		if (strstr(file, t->msg) != NULL)
			return 0;
	}

	return -1;
}

stepfilter_t step_filter(char *file, char *func, int line) {
	if (check_filter(sil, file) == 0)
		return SF_INTO;
	else if (check_filter(sol, file) == 0)
		return SF_OVER;
	else if (check_filter(srl, file) == 0)
		return SF_RETURN;
	return SF_NONE;
}
