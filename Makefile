CC = gcc
CFLAGS = -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -O1 -g
CPPFLAGS = -IPistryak -IPrannychuk/include
LDLIBS = -lcrypto -lm -lcurl -ljson-c
CORE = Onishchenko/src/transaction_manager.c Pistryak/Search/search.c Pistryak/Statistics/statistics.c Pistryak/Budget/budget.c Prannychuk/src/crypto.c Prannychuk/src/storage.c Prannychuk/src/backup.c app/vault.c app/monobank.c app/profiles.c
HEADERS = $(wildcard app/*.h Onishchenko/include/*.h Pistryak/*.h Pistryak/*/*.h Prannychuk/include/*.h)
.PHONY: all run test valgrind
all: build/finance_tracker
build:
	mkdir -p build
build/finance_tracker: app/main.c $(CORE) $(HEADERS) Makefile | build
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ app/main.c $(CORE) $(LDLIBS)
build/test_integration: tests/test_integration.c $(CORE) $(HEADERS) Makefile | build
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ tests/test_integration.c $(CORE) $(LDLIBS)
build/test_monobank: tests/test_monobank.c $(CORE) $(HEADERS) Makefile | build
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ tests/test_monobank.c $(CORE) $(LDLIBS)
build/test_profiles: tests/test_profiles.c $(CORE) $(HEADERS) Makefile | build
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ tests/test_profiles.c $(CORE) $(LDLIBS)
run: all
	./build/finance_tracker
test: all build/test_integration build/test_monobank build/test_profiles
	$(MAKE) -C Prannychuk test
	./build/test_integration
	./build/test_monobank
	./build/test_profiles
valgrind: all build/test_integration build/test_monobank build/test_profiles
	$(MAKE) -C Prannychuk valgrind
	valgrind --leak-check=full --error-exitcode=1 ./build/test_integration
	valgrind --leak-check=full --error-exitcode=1 ./build/test_monobank
	valgrind --leak-check=full --error-exitcode=1 ./build/test_profiles
