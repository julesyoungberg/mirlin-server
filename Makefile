.PHONY: build

build:
	clang++ -std=c++11 -o mirlin --include-directory ./include src/main.cpp -pthread

up:
	docker-compose up --build

format:
	clang-format -i src/*.cpp
