#include <stdio.h>
#include <stdlib.h>

#include "lua.h"
#include "lauxlib.h"

#define SOURCE_LIMIT (64U * 1024U)
#define HEAP_LIMIT (1024U * 1024U)

static unsigned char source[SOURCE_LIMIT + 1];
static size_t used;

static void* bounded_alloc(void* context, void* pointer, size_t old_size, size_t new_size) {
    (void)context;
    if (pointer == NULL) old_size = 0;
    if (new_size == 0) {
        free(pointer);
        used -= old_size;
        return NULL;
    }
    if (new_size > old_size && new_size - old_size > HEAP_LIMIT - used) return NULL;
    void* result = realloc(pointer, new_size);
    if (result != NULL) used = used - old_size + new_size;
    return result;
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const size_t size = fread(source, 1, sizeof(source), stdin);
    if (ferror(stdin) || size > SOURCE_LIMIT) {
        fprintf(stderr, "%s: could not read bounded Lua source\n", argv[1]);
        return 2;
    }
    lua_State* state = lua_newstate(bounded_alloc, NULL);
    if (state == NULL) {
        fprintf(stderr, "%s: could not create Lua parser within heap limit\n", argv[1]);
        return 2;
    }
    const int status = luaL_loadbufferx(state, (const char*)source, size, argv[1], "t");
    if (status != LUA_OK) {
        const char* message = lua_tostring(state, -1);
        fprintf(stderr, "%s\n", message != NULL ? message : "Lua parser failed");
    }
    lua_close(state);
    return status == LUA_OK ? 0 : 1;
}
