#include "game_store_transaction.h"

#include <cassert>
#include <map>
#include <string>

class MemoryStore {
public:
    bool exists(const char* path) const {
        return files.count(path) != 0;
    }

    bool rename(const char* from, const char* to) {
        ++rename_count;
        if (rename_count == fail_rename || rename_count == fail_rename_again) return false;
        const auto source = files.find(from);
        if (source == files.end() || exists(to)) return false;
        files.emplace(to, source->second);
        files.erase(source);
        return true;
    }

    bool remove(const char* path) {
        files.erase(path);
        return true;
    }

    std::map<std::string, std::string> files;
    int rename_count = 0;
    int fail_rename = 0;
    int fail_rename_again = 0;
};

using game_store::transaction::commit;
using game_store::transaction::recoverBackup;

void replacementCommitsAndKeepsTheOldFileUntilSwap() {
    MemoryStore store;
    store.files["/active"] = "old";
    store.files["/stage"] = "new";

    assert(commit(store, "/active", "/stage", "/backup"));
    assert(store.files.size() == 2);
    assert(store.files.at("/active") == "new");
    assert(store.files.at("/backup") == "old");
    assert(!store.exists("/stage"));
}

void failedFirstRenamePreservesActiveAndRemovesStage() {
    MemoryStore store;
    store.files["/active"] = "old";
    store.files["/stage"] = "new";
    store.fail_rename = 1;

    assert(!commit(store, "/active", "/stage", "/backup"));
    assert(store.files.size() == 1);
    assert(store.files.at("/active") == "old");
}

void failedSwapRollsBackThePreviousFile() {
    MemoryStore store;
    store.files["/active"] = "old";
    store.files["/stage"] = "new";
    store.fail_rename = 2;

    assert(!commit(store, "/active", "/stage", "/backup"));
    assert(store.files.size() == 1);
    assert(store.files.at("/active") == "old");
}

void failedRollbackLeavesBackupForBootRecovery() {
    MemoryStore store;
    store.files["/active"] = "old";
    store.files["/stage"] = "new";
    store.fail_rename = 2;
    store.fail_rename_again = 3;
    assert(!commit(store, "/active", "/stage", "/backup"));

    assert(store.exists("/backup"));
    assert(recoverBackup(store, "/active", "/backup"));
    assert(store.files.at("/active") == "old");
    assert(!store.exists("/backup"));
}

void recoveryKeepsACommittedActiveFile() {
    MemoryStore store;
    store.files["/active"] = "new";
    store.files["/backup"] = "old";

    assert(recoverBackup(store, "/active", "/backup"));
    assert(store.files.at("/active") == "new");
    assert(store.files.at("/backup") == "old");
}

int main() {
    replacementCommitsAndKeepsTheOldFileUntilSwap();
    failedFirstRenamePreservesActiveAndRemovesStage();
    failedSwapRollsBackThePreviousFile();
    failedRollbackLeavesBackupForBootRecovery();
    recoveryKeepsACommittedActiveFile();
}
