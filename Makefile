
all:
	gcc   -Wall -Werror -std=gnu99  -g *.c -o lan

clean:
	rm -f  *.o lan

