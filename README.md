# p2p_file_transfer
Peer-to-peer file transfer program for Unix systems using SHA-256 hashing and 
merkle tree structure for file validation. 

# Progrma information
The program relies on package files (.bpkg), which outline the metadata of the
 specific file it refers to. 
This includes an identifier, filename, size, nhashes (number of SHA-256 hashes),
hashes, nchunks (number of chunks of information: the leaves of the merkle tree)
, chunks (the actual information having been hashed).

These .bpkg files can be created using the pkgmake program, and should be 
distributed to allow file transfer.

# How to run
Compile the program using make btide.
```bash
make btide
```
Program is run using './btide \<config file name\>'
```bash
./btide config.cfg
```

The configuration file will use the following information: 
* directory: path local to the system that store .bpkg files and the files that 
are mapped in there. If the directory does not exist, the program should will 
create it.
* max_peers: The number of peers the program can be connected to.
* port: The port the client will be listening on.

# Commands
```bash
CONNECT <ip:port>
```
* Connect to a peer

```bash
DISCONNECT <ip:port>
```
* Disconnect from a peer
  
```bash
ADDPACKAGE <filename>
```
* Adds a package to the program's list of managed packages. This is a .bpkg file.

```bash
REMPACKAGE <ident, 20 char matching>
```
* Removes a package based on the provided identifier
  
```bash
PACKAGES
```
* Views all managed packages.
  
```bash
PEERS
```
* View all connected peers.

  
```bash
FETCH <ip:port> <identifier> <hash> (<offset>)
```
* Requests chunks related to the hash given from the provided peer. If offset is
 specified, it will use this additional info to narrow down a hash at a 
 particular offset of the file.

```bash
QUIT
```
* Closes the program

# Organisation of software

Header files with structs and function declarations are found in the /include 
directory. The corresponding source code is found in the /src directory, which 
is liked to the binary at compilation.

**Part 1**
The source code for part 1 is found in src/chk/pkgchk.c, which uses functions 
and declarations from sha256.h and merkle_tree.h. The functions defined in 
pkgchk.c are responsible for loading a bpkg file, constructing a merkle tree, 
as well the as the auxillary operations involved in extracting information from 
the file and the tree. pkgmain.c is the entry point of the program and uses 
command line arguments to decide the output.

**Part 2**
Part 2 involves multiple source files. btide.c is the entry point of the program
and is where the main method is found. It uses an infinite input loop to get
user input and perform the corresponding tasks. 

The function used to parse input from stdin is found in cli.c. 

The functions used to parse input from the config file is found in config.c. 

dyn_arry.c and thread_array.c are responsible for functions used to create and
maintain the dynamic arrays used in the program for threads, packages and 
connection. 

packet.c creates packets to be sent across the network, and also has a specific 
function that sends RES packets (generate_and_send_RES)

The peer management code is found in peer.c which includes functions to create
and start server/client threads and also how these threads accept and respond to
packets send across the network.

packet_dump.c is used to create the packet_dump binary for testing purposes.

# Testing

**Part 1**

To run the tests for part 1, use make p1tests.

```bash
make p1tests
```

The testing data is located in the p1tests directory. This testing suite 
reflects the tests in the testing plan of A3Tests, as well as an additional test
for an empty bpkg file. 

Brief outline of tests
1. Loading of a valid bpkg file
2. Verification of a complete data file
3. Verification of an incomplete data file
4. Loading an erroneous bpkg file
5. Verification of an empty data file
6. Correct data file with extra chunks
7. Compatibility with different file types
8. Creating a file/handling non-existent files
9. Checking for an existing file
10. Constructing/operating a large merkle tree
11. Loading a bpkg file with one hash/chunk
12. Empty bpkg file

The directory contains .args files and .out files, as well as .bpkg and .dat 
files for testing. This includes some binary files (test10.dat) necessary for 
testing. The script iterates through all the tests and compares the output to 
the expected output.

```bash
cat $test | xargs ./pkgmain | diff - $expected
```

The test script tests 88.89% of pgkchk.c. 

```bash
File 'src/chk/pkgchk.c'
Lines executed:88.89% of 324
Creating 'pkgchk.c.gcov'

Lines executed:88.89% of 324
```

See the full gcov report in /gcov_reports/pkgchk.c.gcov.

**Part 2**
The testing for part 2 is split into 3 different parts: package/config testing, 
packet testing and network testing. To run the package and packet testing, 
run the script p2test.sh.

```bash
make p2tests
```

*Package/Config*
The package tests can be found in the /p2tests directory, which tests the 
loading of valid and invalid configuration files and packages.

*Packets*
The packet tests can be found in the /p2_packet_tests directory. The packet_dump
binary creates packets using the functions in packet.c and writes the binary
data to .pkt files during the test script. These .pkt files are compared against 
the output of verified packets, using the pktval program found in the resources 
directory.

*Network*
The network tests can be found in the /p2_network_test directory. To run 
networking tests, each network test is separated into seperate folders. Each 
folder has two config files and two .in files, one for each peer. 
Move into the folder, with the binary program.

```bash
cd p2_network_tests/test_<insert test number>
```

The tests are configured such that the one peer should execute all their 
commands before the other. 

Open two seperate terminals. Start peer 2 using its config file (config.cfg2) 
and enter its input (in test_x.in2). Note that peer 2 will need to remain 
running until the end of the test, and therefore ‘QUIT’ is not included and 
requires manual input after the test.

```bash
./btide config.cfg2
```

Then, run peer 1 using the input from the .in files and pipe the output of peer 
1 into .out files. 

```bash
./btide config.cfg > test_x.out
```
You cannot pipe the input directly due to time taken to read the files for 
various functions such as checking for completion status of packages.

Then compare these outputs (.out) with the expected output (.expected). 

Test 1 tests basic connection, disconnection and the ‘PEERS’ command.
- Make sure it can connect to and disconnect from another peer
- Make sure it cannot try and connect with someone already connected
- Make sure it cannot disconnect from someone not connected

Tests 2-5 test the FETCH functionality.
ii) Validating FETCH input: incorrect ip/port, package, hash
iii) Transferring data normally from complete file to incomplete file
iv) Transferring from incomplete to incomplete (setting error byte)
v) Transferring data to a peer that does not have the file (tests file creation:
Observe new data file in /data)

Note: After running FETCH cases, files/data must be reset to their original 
state, as the program will have edited them.

The testing suite tests 84.54% of source code. Due to code sections that are 
repeated in the client and server threads, parts that are not covered in 
the client version are covered in the server version and vice versa. See the 
full gcov reports in the /gcov_reports directory.

```bash
File 'src/btide.c'
Lines executed:93.72% of 191
Creating 'btide.c.gcov'

File 'src/config.c'
Lines executed:74.42% of 43
Creating 'config.c.gcov'

File 'src/cli.c'
Lines executed:83.72% of 43
Creating 'cli.c.gcov'

File 'src/dyn_array.c'
Lines executed:84.92% of 126
Creating 'dyn_array.c.gcov'

File 'src/packet.c'
Lines executed:100.00% of 57
Creating 'packet.c.gcov'

File 'src/peer.c'
Lines executed:75.36% of 207
Creating 'peer.c.gcov'

File 'src/thread_array.c'
Lines executed:72.00% of 25
Creating 'thread_array.c.gcov'

Lines executed:84.54% of 692
```



