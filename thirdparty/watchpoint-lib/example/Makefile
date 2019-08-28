

test: test.c ../src/libwatchpoint.a
	gcc -o $@ $^ -I../include -lwatchpoint -L../src -lpthread
clean:
	rm -f test
