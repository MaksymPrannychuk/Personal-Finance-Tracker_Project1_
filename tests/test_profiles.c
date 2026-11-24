#define _POSIX_C_SOURCE 200809L
#include "../app/profiles.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void)
{
    char temp[] = "/tmp/pft-profiles-XXXXXX";
    assert(mkdtemp(temp) != NULL);
    assert(chdir(temp) == 0);
    char a[PROFILE_PATH_CAP], b[PROFILE_PATH_CAP], path[PROFILE_PATH_CAP];
    assert(profile_name_valid("student_1"));
    assert(!profile_name_valid("../other") && !profile_name_valid("A/B") && !profile_name_valid("") && !profile_name_valid("Student"));
    assert(!profile_name_valid("123456789012345678901234567890123"));
    assert(!profile_path("student", path, 2));
    puts("[PASS] Profiles: safe names and bounded paths");

    assert(!profile_open("alice", a, sizeof a));
    assert(!profile_create("alice", a, sizeof a, "short"));
    assert(profile_create("alice", a, sizeof a, "alice-pass"));
    assert(profile_create("bob", b, sizeof b, "bob-password"));
    assert(strcmp(a,b) != 0 && profile_open("alice",path,sizeof path));
    assert(!profile_create("alice", path, sizeof path, "other-password"));
    TransactionManager alice = {0}, bob = {0};
    assert(vault_load(a,&alice,"alice-pass") == PFT_OK);
    assert(vault_load(b,&bob,"bob-password") == PFT_OK);
    assert(alice.count == 0 && bob.count == 0);
    puts("[PASS] Profiles: creation, reopening and duplicate protection");

    Transaction t = {.id=1,.date="2026-11-24",.amount=75.25,.category="Food",.is_income=0,.description="Private sample"};
    assert(add_transaction(&alice,t));
    assert(vault_save(a,&alice,"alice-pass") == PFT_OK);
    assert(vault_load(a,&alice,"bob-password") != PFT_OK && alice.count == 1);
    assert(vault_load(b,&bob,"bob-password") == PFT_OK && bob.count == 0);
    assert(vault_load(a,&alice,"alice-pass") == PFT_OK && alice.transactions[0].amount == 75.25);
    puts("[PASS] Profiles: independent records and passwords");

    assert(symlink("p-alice","profiles/p-fake") == 0);
    assert(!profile_open("fake",path,sizeof path));
    assert(!profile_create("fake",path,sizeof path,"fake-password"));
    assert(unlink("profiles/p-fake") == 0);
    puts("[PASS] Profiles: reject profile directory symlinks");

    TransactionManager demo = {0};
    assert(profile_demo(&demo) && demo.count == 6 && demo.next_id == 7);
    assert(delete_transaction(&demo,1));
    assert(profile_demo(&demo) && demo.count == 6);
    assert(vault_load(a,&alice,"alice-pass") == PFT_OK && alice.count == 1);
    assert(vault_load(b,&bob,"bob-password") == PFT_OK && bob.count == 0);
    puts("[PASS] Demo: fictional transactions, reset and no profile changes");
    assert(unlink(a) == 0 && unlink(b) == 0);
    assert(rmdir("profiles/p-alice") == 0 && rmdir("profiles/p-bob") == 0 && rmdir("profiles") == 0);
    assert(chdir("/") == 0 && rmdir(temp) == 0);
    puts("All 5 profile tests passed.");
    return 0;
}
