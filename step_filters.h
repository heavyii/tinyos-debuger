/*
 * step_filters.h
 *
 *  Created on: Sep 2, 2013
 *      Author: heavey
 */

#ifndef STEP_FILTERS_H_
#define STEP_FILTERS_H_

typedef enum step_filter_result {SF_NONE, SF_INTO, SF_OVER, SF_RETURN} stepfilter_t;

int filters_init(char *pathname);
stepfilter_t step_filter(char *file, char *func, int line);

#endif /* STEP_FILTERS_H_ */
