# multi-PSI From Mutual Dynamic OPRF
This is the implementation of our paper **Multiple Private Set Intersection From Mutual Dynamic Oblivious PRF** [[eprint](https://eprint.iacr.org/2020/729.pdf)].

## Installation
The implementation has been tested on Linux. To download the code and compile, run the following commands. It will also download and compile the libraries of [`libOTe`](https://github.com/osu-crypto/libOTe), [`Boost`](https://sourceforge.net/projects/boost/), and [`Miracl`](https://github.com/miracl/MIRACL).
```
$ git clone --recursive https://github.com/GreenEli/Multiple-PSI.git
$ cd multiple-PSI
$ bash compile
```

## Running the Code
### Parameters:
```
-r 0/1   to run a sender/receiver.
-ss      log of the set size on sender side (initial PSI).
-rs      log of the set size on receiver side (initial PSI).
-ss1      log of the set size on sender side (second PSI).
-rs1      log of the set size on receiver side (second PSI).
-ssi      log of the set size on sender side ((i+1)-th PSI).
-rsi      log of the set size on receiver side ((i+1)-th PSI).
-w       width of the matrix (initial PSI).
-h       log of the height of the matrix (initial PSI).
-hash    hash output length in bytes (initial PSI).
-w1       width of the matrix (second PSI).
-h1       log of the height of the matrix (second PSI).
-hash1    hash output length in bytes (second PSI).
-wi       width of the matrix ((i+1)-th PSI).
-hi       log of the height of the matrix ((i+1)-th PSI).
-hashi    hash output length in bytes ((i+1)-th PSI).
-ip      ip address (and port).
```
### Examples:
```
$ ./bin/PSI_test -r 0 -ss 20 -rs 20 -ss1 16 -rs1 16 -w 624 -h 20 -hash 11 -w1 609 -h1 16 -hash1 9 -ip 127.0.0.1  & ./bin/PSI_test -r 1 -ss 20 -rs 20 -ss1 16 -rs1 16 -w 624 -h 20 -hash 11 -w1 609 -h1 16 -hash1 9 -ip 127.0.0.1

$ ./bin/PSI_test -r 0 -ss 24 -rs 24 -ss1 16 -rs1 16 -w 636 -h 24 -hash 12 -w1 609 -h1 16 -hash1 9 -ip 127.0.0.1 & ./bin/PSI_test -r 1 -ss 24 -rs 24 -ss1 16 -rs1 16 -w 636 -h 24 -hash 12 -w1 609 -h1 16 -hash1 9 -ip 127.0.0.1

```
## To Do
Note that we have completed the implementation in the main part of our protocol, we will update the rest in future. And the current version in our code has low readability, further updates will be made to enhance the readability.

## Help
For any questions on building or running the library, please create git issues, or contact [`Qiang Liu`] liuqiang2022 at iie do ac do cn.
