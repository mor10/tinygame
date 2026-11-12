#pragma once

namespace game_store::transaction {

template <typename Store>
bool recoverBackup(Store& store, const char* active, const char* backup) {
    return !store.exists(backup) || store.exists(active) || store.rename(backup, active);
}

template <typename Store>
bool commit(Store& store, const char* active, const char* stage, const char* backup) {
    const bool had_active = store.exists(active);
    if (had_active && !store.rename(active, backup)) {
        store.remove(stage);
        return false;
    }
    if (!store.rename(stage, active)) {
        if (had_active && !store.rename(backup, active)) return false;
        store.remove(stage);
        return false;
    }
    return true;
}

}  // namespace game_store::transaction
