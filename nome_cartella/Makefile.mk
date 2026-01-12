all: filter store watchdog

filter: filter.o rt-lib.o
	gcc -o filter filter.o rt-lib.o -lrt -pthread -lm

store: store.o rt-lib.o
	gcc -o store store.o rt-lib.o -lrt -pthread

watchdog: watch_dog.o rt-lib.o
	gcc -o watchdog watch_dog.o rt-lib.o -lrt

filter.o: filter.c rt-lib.h
	gcc -Wall -Wextra -c filter.c

store.o: store.c rt-lib.h
	gcc -Wall -Wextra -c store.c

watch_dog.o: watch_dog.c rt-lib.h
	gcc -Wall -Wextra -c watch_dog.c

rt-lib.o: rt-lib.c rt-lib.h
	gcc -Wall -Wextra -c rt-lib.c

clean:
	rm -f *.o filter store watchdog
