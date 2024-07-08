# p2p_file_transfer
Peer-to-peer file transfer system using SHA-256 hashing and merkle tree structure for file validation. 

Program structure
The program relies on package files (.bpkg), which outline the metadata of the specific file it refers to. 
This includes an identifier, filename, size, nhashes (number of SHA-256 hashes), hashes, nchunks 
(number of chunks of information: the leaves of the merkle tree), chunks (the actual information 
having been hashed).

These .bpkg files can be created using the pkgmake program, and should be distributed to allow file transfer.

Program is run using './btide \<config file name\>'

The configuration file will use the following information: 
* directory: path local to the system that store .bpkg files and the files that are mapped in there.
  If the directory does not exist, the program should will create it.
* max_peers: The number of peers the program can be connected to.
* port: The port the client will be listening on. Acceptable port range (1024, 6553).

Commands:

CONNECT \<ip:port\>
* Connect to a peer

DISCONNECT \<ip:port\>
* Disconnect from a peer
  
ADDPACKAGE \<file\>
* Adds a package to the program's list of managed packages. This should be a .bpkg file.
  
REMPACKAGE \<ident, 20 char matching\>
* Removes a package based on the provided identifier
  
PACKAGES
* Views all managed packages.
  
PEERS
* View all connected peers.
  
FETCH \<ip:port\> \<identifie\r> \<hash\> (\<offset\>)
* Requests chunks related to the hash given from the provided peer. If offset is specified, it will use t
his additional info to narrow down a hash at a particular offset of the file.

QUIT
* Closes the program



