# TRTP2

## Authors

 - Sébastien d'Herbais de Thun - 2875-16-00
 - Thomas Heuschling - 2887-16-00

## Makefile
 
 - all: builds the release version of the code
 - clean: deletes all build artifacts
 - run: run the release version
 - debug: builds a version with debug symbols
 - test: builds & tests the code
 - stat: generates gitlog.stat
 - install_mdbook: installs the report builder, requires [cargo/rust](https://rust-lang.org)
 - report: uses mdbook & mdbook-latex to build a PDF version (can take a **loooong** time)