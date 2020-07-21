.PHONY: thirdparty clean

CURRENT_DIR=$(shell pwd)

thirdparty:
	#git submodule update
	cd thirdparty/watchpoint-lib && make install PREFIX=$(CURRENT_DIR)/build/thirdparty
	cd thirdparty/xed && ./mfile.py --debug --shared --prefix=$(CURRENT_DIR)/build/thirdparty install
	cd thirdparty/libpfm-4.10.1 &&  make PREFIX=$(CURRENT_DIR)/build/thirdparty install
	cd thirdparty/boost_1_71_0 && sh ./bootstrap.sh --prefix=$(CURRENT_DIR)/build/thirdparty --with-libraries="filesystem"  cxxflags="-std=c++11" && ./b2 -j 4 && ./b2 filesystem install 
	cd thirdparty/bintrees-2.0.7 &&  python setup.py install --user
	cd thirdparty/allocation-instrumenter && mvn package -DskipTests
	cd preload && make
	cd src && make 

clean:
	make -C src clean
	make -C thirdparty/watchpoint-lib clean
	make -C thirdparty/libpfm-4.10.1 clean
	cd thirdparty/xed && ./mfile.py clean 
	cd preload && rm libpreload.so
	mvn -f thirdparty/allocation-instrumenter/pom.xml clean
	rm -rf build
