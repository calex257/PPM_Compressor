all: build

build: quadtree.c
	gcc -g -Wall -std=c99 -o quadtree quadtree.c

clean: build
	rm quadtree