.PHONY: server client

COMPILER=clang++
CFLAGS=-std=c++11 -I./external

server:
	cd ./src && cmake . && cmake --build . && mv ./server .. && cd ..

up:
	docker-compose up --build

format:
	clang-format -i ./src/*.cpp src/*.hpp
