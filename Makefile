.PHONY: server client

server:
	clang++ -std=c++11 -o mirlin --include-directory ./include src/main.cpp -pthread

client:
	clang++ -std=c++11 -o mirlinclient --include-directory ./include client/main.cpp -pthread

up:
	docker-compose up --build

format:
	clang-format -i src/*.cpp
