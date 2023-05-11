CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -g
LDLIBS = -lpthread -lcrypto

all: dfs dfc

dfs: queue.o util.o dfs.o
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

dfs.o: dfs.c util.h queue.h
	$(CC) $(CFLAGS) -c $< -o $@

dfc: queue.o util.o dfc.o
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

dfc.o: dfc.c util.h queue.h
	$(CC) $(CFLAGS) -c $< -o $@

util.o: util.c util.h
	$(CC) $(CFLAGS) -c $< -o $@

queue.o: queue.c queue.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f dfs dfc *.o

clearcache:
	rm -rf ./cache/*

clear-dfs:
	rm -rf ./dfs1/*
	rm -rf ./dfs2/*
	rm -rf ./dfs3/*
	rm -rf ./dfs4/*
	
run-dfs:
	gnome-terminal -- bash -c "./dfs ./dfs1 10001; exec bash" &
	gnome-terminal -- bash -c "./dfs ./dfs2 10002; exec bash" &
	gnome-terminal -- bash -c "./dfs ./dfs3 10003; exec bash" &
	gnome-terminal -- bash -c "./dfs ./dfs4 10004; exec bash" &

get-dfc:
	gnome-terminal -- bash -c "./dfc put dfc.conf foo3; exec bash"

put-dfc:
	gnome-terminal -- bash -c "./dfc put dfc.conf foo3; exec bash"

run-all: all run-dfs run-dfc

debug-dfs: all
	gnome-terminal -- bash -c "gdb -ex run --args ./dfs ./dfs1 10001; exec bash" &
	gnome-terminal -- bash -c "gdb -ex run --args ./dfs ./dfs2 10002; exec bash" &
	gnome-terminal -- bash -c "gdb -ex run --args ./dfs ./dfs3 10003; exec bash" &
	gnome-terminal -- bash -c "gdb -ex run --args ./dfs ./dfs4 10004; exec bash" &

debug-dfc: all
	gnome-terminal -- bash -c "gdb -ex run --args ./dfc put dfc.conf foo3; exec bash"

debug-all: debug-dfs debug-dfc

# eventually change to do single line arguments, just doing this for ease of testing

push:
ifndef COMMIT_MSG
	$(error COMMIT_MSG is not set)
endif
	git add .
	git commit -m "$(COMMIT_MSG)"
	git push
