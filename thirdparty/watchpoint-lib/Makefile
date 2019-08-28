.PHONY: all lib test install clean


lib:
	cd src && make

install: lib
	mkdir -p ${PREFIX}/include && cp include/*.h ${PREFIX}/include/
	mkdir -p ${PREFIX}/lib && cp src/libwatchpoint.a ${PREFIX}/lib/
clean:
	make -C src clean
