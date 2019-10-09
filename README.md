# TRTP2

## Authors

- Sébastien d'Herbais de Thun - 2875-16-00
- Thomas Heuschling - 2887-16-00

## Project structure

The project is structure as follows :

- `bin/` - the binary output files, normally empty, cleaned using `make clean`
- `headers/` - header definitions for the project
- `report/` - contains the LateX source code and resources for the report
- `src/` - contains the C source code of the project, `src/main.c` is the main function
- `tests/` - contains test definitions, best ran using `make clean && make test`
- `gitlog.stat` - required `git log --stat` output, generated using `make stat`
- `Makefile` - the make file
- `Readme.md` - informations about the project for the code review
- `statement.pdf` - the statement of the project
- `report.pdf` - the required report in PDF format, generated using `make report`

## Makefile

- `all`: builds the release version of the code
- `clean`: deletes all build artifacts
- `run`: run the release version
- `debug`: builds a version with debug symbols
- `test`: builds & tests the code
- `stat`: generates gitlog.stat
- `install_mdbook`: installs the report builder, requires [cargo/rust](https://rust-lang.org)
- `report`: uses mdbook & mdbook-latex to build a PDF version (can take a **loooong** time)

## Objectives

As we already passed the project last year, we decided to restart from scratch but use our
previous knowledge to create a better and simpler implementation. In order to do this,
we decided to go with a better project structure made of simple, composable components
and assembled into larger functional units to reach the complete functionality required.

We also set some objectives for ourselves including, better performance, cleaner and
simpler code and no on-the-go allocations. These objectives are achieved in different
ways explained below.

### Better performance

We designed our architecture to have as high a performance as we can have while using
techniques available in the course and with limited dependencies. For this reason,
we implemented a near allocation free code that uses multithreading and SIMD to
dramatically improve the performance. Every buffer allocated is reused. We also try and
use limited mutexes and locks as those have a high latency associated with them.

We also used more advanced data structures such as hash tables to improve performance
by removing the need of iterative lookup by having an average execution speed of O(1).
We also use a stream to communicate allowing both the head and the tail to be manipulated
without having interlocks. The data structure used by the stream is a double-ended linked-list
removing the need for iterative operations once again.

### Cleaner & simpler code

The code is split into numerous small files containing single function units such as
the definition of a hash-table, packet related functions, etc. This makes reading,
debugging and working as a group easier. In addition, we have commented the entire
codebase with in-depth comments explaining he inner workings, inputs and outputs
of every function we defined.

## Architecture

TODO

## SuperFastHashing

This piece of code is designed to make the hashing of the IP and port combo easier and simpler.
We still wanted a high performance implementation and looked on the internet for a fast
hashing algorithm suitable for hashing IPv6 addresses and ports. Fortunately we found a
so-called SuperFastHashing algorithm that is much faster than CRC32 and is according to
users on StackOverflow suitable for IP address hashing. All credits for this implementation
go to the [original author](http://www.azillionmonkeys.com/qed/hash.html).
