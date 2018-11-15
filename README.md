# cpsc3220Project4
Implements a highly concurrent, multi-threaded file buffer cache.

The cache supports two functions, blockread and blockwrite.

blockread(char \*x, int blocknum) takes in a block to read from, blocknum, and
a char array in which to store the read in data, x. It first checks to see if
the data is already in the file buffer cache, if it is then the data is read
from the cache. However, if the data is not in the cache then it is placed into
either an empty position in the cache, or into a spot chosen using the LRU
replacement policy. If the LRU policy is used, the replaced block is written to
memory if it has been edited.


blockwrite(char\*x, int blocknum) takes in a block to be written to, blocknum,
and a char array to be written to it.
