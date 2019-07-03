# c10M
Research into the c10M problem with respect to the Linux kernel.

# Making and running siege

### Step 1: Make and run

    mkdir build
    cd build
    cmake ../
	make clean 
	make clean && make VERBOSE=1
    gcc  -g   CMakeFiles/httpio-static.dir/src/main.c.o  -o httpio-static -pthread libhttpiolib-static.a

#### Note on build failure

Cmake uses the gcc command with -lpthread even for static build, which is causing the error. See https://stackoverflow.com/questions/23250863/difference-between-pthread-and-lpthread-while-compiling for differences.

The problem with dynamic build is probably the function pointer in the structure being not initalised in the dynamic library. 

TODO fix dynamic build.


#### TODO: building with paramters

Make can specify parameters. By default the parameters are taken from `conf.h`.

	make CFLAGS=-D TUPLE_TYPE={} -D IOLOOP_TYPE={} -D HANDLER_LIFECYCLE={}

The following parameters are possible:

    TUPLE_TYPE is one of ['TUPLE_INET']
    IOLOOP_TYPE is one of ['IOLOOP_ACCEPT', 'IOLOOP_SELECT']
    HANDLER_LIFECYCLE is one of ['PROCESS_UNIPROCESS', 'PROCESS_FORK']

Invoke run via.

	./httpio

### Run siege or ab on the server

	siege -c64 -t10s -b http://localhost:8888/

    ab -c64 -t10 -r http://localhost:8888/


Above command runs HTTP siege or Apache ab on the default bind port of the `./httpio` process. *64 concurrent* connections are used at max to run for *10 seconds* in *benchmarking* mode. Note that using the `-b` benchmarking flag is similar to doing `-d0` which sets the delay between two users to 0. The `-r` flag in `ab` says don't close socket on receive errors. 
