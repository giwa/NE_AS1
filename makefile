simsubnet: main.o config.o cmd_send.o search.o
	gcc -o simsubnet main.o config.o cmd_send.o search.o

main.o: main.c simsubnet.h
	gcc -c main.c

config.o: config.c simsubnet.h
	gcc -c config.c

cmd_send.o: cmd_send.c simsubnet.h
	gcc -c cmd_send.c

search.o: search.c simsubnet.h
	gcc -c search.c

clean:
	\rm simsubnet *.o
