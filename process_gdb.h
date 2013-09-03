/*
 * process_gdb.h
 *
 *  Created on: Sep 2, 2013
 *      Author: heavey
 */

#ifndef PROCESS_GDB_H_
#define PROCESS_GDB_H_

/**
 * start gdb
 * @return: Return 0 on success, nonzero on error
 */
int gdb_start(int argc, char **argv);

/**
 * gdb_send - Send data, exit if error occur
 */
void gdb_send(const char *data, int len);

/**
 * read one line
 */
int gdb_readline(char *data, int size);
int gdb_get_fdin(void);
int gdb_get_fdout(void);

#endif /* PROCESS_GDB_H_ */
