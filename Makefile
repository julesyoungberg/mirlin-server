.PHONY: build

build:
	clang++ -std=c++11 -o mirlin --include-directory ./include src/main.cpp -pthread
