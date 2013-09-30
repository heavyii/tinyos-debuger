# file: $(top_dir)/debug/Makefile



CC=gcc

CFLAGS = -O1 -g -Wall -pipe 
INCLUDE = -I.

%.o:%.c
	$(CC)  -c $^ ${INCLUDE} ${CFLAGS} -o $@	
	
tinyos-debuger_obj = main.o step_filters.o process_gdb.o list.o tinyos_gdb_packet.o socket.o
tinyos-debuger: $(tinyos-debuger_obj)
	@$(CC) $(INCLUDES)  ${CFLAGS} $^ -o $@

.PYTHON: clean
clean:
	rm *.o
	rm tinyos-debuger
