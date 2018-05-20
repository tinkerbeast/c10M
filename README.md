# c10M
Research into the c10M problem with respect to the Linux kernel.

# Making and running siege

### Step 1: Make and run

	make clean
	make

Make can specify parameters. By default the parameters are taken from `conf.h`.

	make CFLAGS=-D TUPLE_TYPE={} -D HANDLER_LIFECYCLE={}

TUPLE_TYPE is one of ['TUPLE_INET']
HANDLER_LIFECYCLE is one of ['PROCESS_UNIPROCESS', 'PROCESS_FORK']

Invoke running via.

	./httpio

### Run siege on the server

	siege -c64 -t10s -b http://localhost:8888/

Above command runs a HTTP siege of the default bind and port of the `./httpio` process. *64 concurrent* connections are used at max to run for *10 seconds* in *benchmarking* mode. Note that using the `-b` benchmarking flag is similar to doing `-d0` which sets the delay between two users to 0.
