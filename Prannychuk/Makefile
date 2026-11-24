CC = gcc
CFLAGS = -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -O1 -g
CPPFLAGS = -Iinclude
LDLIBS = -lcrypto
CORE = src/crypto.c src/storage.c src/backup.c
HEADERS = include/storage.h include/crypto.h include/finance_types.h include/backup.h
OUT = build

.PHONY: all demo test valgrind clean
all: $(OUT)/demo $(OUT)/test_crypto $(OUT)/test_storage $(OUT)/test_backup

$(OUT):
	mkdir -p $(OUT)

$(OUT)/demo: demo/main.c $(CORE) $(HEADERS) | $(OUT)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ demo/main.c $(CORE) $(LDLIBS)

$(OUT)/test_crypto: tests/test_crypto.c $(CORE) $(HEADERS) | $(OUT)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ tests/test_crypto.c $(CORE) $(LDLIBS)

$(OUT)/test_storage: tests/test_storage.c $(CORE) $(HEADERS) | $(OUT)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ tests/test_storage.c $(CORE) $(LDLIBS)

$(OUT)/test_backup: tests/test_backup.c $(CORE) $(HEADERS) | $(OUT)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ tests/test_backup.c $(CORE) $(LDLIBS)

demo: $(OUT)/demo
	./$(OUT)/demo

test: $(OUT)/test_crypto $(OUT)/test_storage $(OUT)/test_backup
	./$(OUT)/test_crypto
	./$(OUT)/test_storage
	./$(OUT)/test_backup

valgrind: $(OUT)/test_crypto $(OUT)/test_storage $(OUT)/test_backup
	valgrind --leak-check=full --error-exitcode=1 ./$(OUT)/test_crypto
	valgrind --leak-check=full --error-exitcode=1 ./$(OUT)/test_storage
	valgrind --leak-check=full --error-exitcode=1 ./$(OUT)/test_backup

clean:
	rm -rf $(OUT)
