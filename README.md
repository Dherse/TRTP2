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