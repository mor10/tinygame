#include <cassert>
#include <cstring>
#include <cstdio>
#include <string_view>

#include "manifest_view.h"

int main() {
    constexpr std::string_view valid =
        "api = 1\n"
        "id = \"cave_hop\"\n"
        "name = \"CAVE HOP\"\n"
        "starter = \"platformer\"\n"
        "view = \"scroll\"\n"
        "\n"
        "[tuning]\n"
        "gravity = 30\n";
    tg::engine::ManifestView manifest;
    assert(tg::engine::parseManifest(valid, manifest));
    assert(std::strcmp(manifest.id, "cave_hop") == 0);
    assert(std::strcmp(manifest.name, "CAVE HOP") == 0);
    assert(manifest.scroll);

    constexpr std::string_view escaped =
        "api = 1\nid = \"cave_hop\"\nname = \"A\\u0042\"\n"
        "starter = \"platformer\"\nview = \"fixed\" # comment\n";
    assert(tg::engine::parseManifest(escaped, manifest));
    assert(std::strcmp(manifest.name, "AB") == 0);
    assert(!manifest.scroll);

    for (const std::string_view text : {
             "\"api\" = +1\n'id' = 'cave_hop'\n'name' = 'GAME'\n"
             "'starter' = 'platformer'\n'view' = 'fixed'\n",
             "api = 0x0_1\nid = 'cave_hop'\nname = 'GAME'\n"
             "starter = 'x'\nview = 'fixed'\n",
             "api = 0o1\nid = 'cave_hop'\nname = 'GAME'\n"
             "starter = 'x'\nview = 'fixed'\n",
             "api = 0b1\nid = 'cave_hop'\nname = 'GAME'\n"
             "starter = 'x'\nview = 'fixed'\n",
             "api = 1\nid = 'cave_hop'\nname = '   '\n"
             "starter = '\t'\nview = 'fixed'\n",
         }) {
        assert(tg::engine::parseManifest(text, manifest));
        assert(std::strcmp(manifest.id, "cave_hop") == 0);
    }

    const std::string_view invalid[] = {
        "api = 1\nid = \"Other\"\nname = \"GAME\"\nstarter = \"x\"\nview = \"fixed\"\n",
        "api = 1\nid = \"cave_hop\"\nid = \"other\"\nname = \"GAME\"\n"
        "starter = \"x\"\nview = \"fixed\"\n",
        "api = 2\nid = \"cave_hop\"\nname = \"GAME\"\nstarter = \"x\"\nview = \"fixed\"\n",
        "api = 0x_1\nid = \"cave_hop\"\nname = \"GAME\"\nstarter = \"x\"\nview = \"fixed\"\n",
        "api = 0b10\nid = \"cave_hop\"\nname = \"GAME\"\nstarter = \"x\"\nview = \"fixed\"\n",
        "api = 1\nid = \"cave_hop\\u0000bad\"\nname = \"GAME\"\nstarter = \"x\"\nview = \"fixed\"\n",
        "api = 1\nid = \"cave_hop\"\nname = \"GAME\"\nstarter = \"x\"\nview = \"fixed\\u0000x\"\n",
        "api = 1\nid = \"cave_hop\"\nname = \"\"\nstarter = \"x\"\nview = \"fixed\"\n",
        "api = 1\nid = \"cave_hop\"\nname = \"GAME\"\nstarter = \"x\"\nview = \"other\"\n",
        "api = 1\nid = \"cave_hop\"\nname = \"GAME\"\nstarter = \"x\"\n",
    };
    for (const std::string_view text : invalid) {
        assert(!tg::engine::parseManifest(text, manifest));
    }
    std::puts("manifest view tests passed");
}
