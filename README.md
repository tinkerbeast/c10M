# c10M
Research into the c10M problem with respect to the Linux kernel.

# Making and running siege

### Step 1: Make and run

	make clean
	make

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
