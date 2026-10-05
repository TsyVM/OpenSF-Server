// Builds one pack from its source folder (Docs/UniversalServerDeploy.md §11.2): lsfpack's work, in a
// header so the pack tests build theirs the same way.
//
//   weapons/<pack>/pack.cfg   maps/<pack>/pack.cfg   chars/<pack>/pack.cfg
//
// The files beside pack.cfg keep Soldier Front's own layout and go into the pack under
// x/<pack id>/ as they lie: a gun in bhw/sf_a_<name>/, a map in ground/sf_m_<name>/ with its
// world/sf_m_<name>.wld, a character in sf_c_<name>/. A weapons folder fills the weapon library, a
// maps folder the area library, a chars folder the force library; a top folder named after a
// library (area, weapon, force, sound, effect, lobby, menu) puts what is under it in that one.
//
// pack.cfg:
//
//   id = mypack                 [a-z0-9_], 3 to 24 (NM-9)
//   version = 1
//   author = "Who made it"
//
//   [weapon]
//   name  = longrifle           [a-z0-9_-]: its code is x:mypack:longrifle (NM-7)
//   title = "Long Rifle"
//   like  = ak74                the base gun it starts from: every number not given here is that
//                               gun's, and its sounds, pictures and kill mark stand in (PK-8)
//   model = bhw/sf_a_longrifle  its own model's folder; left out: the `like` gun's model
//   price = 12000               SP (SH-8); and any of: class damage head leg range falloff_start
//                               falloff_min rpm automatic magazine reserve reload draw spread_stand
//                               spread_crouch spread_move spread_move_rate spread_per_shot spread_max
//                               spread_recover spread_run spread_air spread_walk recoil_up
//                               recoil_up_max recoil_side recoil_side_max recoil_recover move_speed
//                               grip carried zoom scope_image scope_move pellets grenade fuse
//                               blast_radius melee_range
//
//   [character]
//   name = ranger   title = "Ranger"   nation = "USA"
//   model = sf_c_ranger         its folder: the rig sf_c_ranger_bone and its pieces
//   art   = delta               the base force whose HUD art and voice stand in
//   price = 30000   speed = 1.0   upper_defense = 0   lower_defense = 0   avoid_headshot = 0
//
//   [part]
//   name = ranger_cap   title = "Ranger Cap"   tab = Head   slot = 3
//   character = ranger          one of this pack's characters (PK-6a) -- or  force = delta  for a
//                               new part of a base force (PK-6b)
//   model = sf_o_ranger_cap     the piece's file name, no folder, no extension
//   price = 2000
//
//   [map]
//   name = harbour              ground/sf_m_harbour/sf_m_harbour.map; its id is x-mypack-harbour
//   title = "Harbour"   night = false
//
// What a definition says of a model is measured from the model (a character's bones, size and
// clips), never typed; and the pack is checked exactly as the server and the game will check it
// (PK-4), then each map as sfcheck proves the client's own (PK-7).
#pragma once

#include "Engine/Core/Config.hpp"
#include "Engine/Core/FileSystem.hpp"
#include "Engine/Core/Strings.hpp"
#include "Engine/Physics/CollisionMesh.hpp"
#include "Game/Modes.hpp"
#include "Game/Movement.hpp"
#include "Game/Navigation.hpp"
#include "Game/Pack.hpp"
#include "Game/PackMount.hpp"
#include "SF/Data.hpp"
#include "SF/Level.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace lsf::packbuild {

struct Built {
    std::string id;
    u32 version = 1;
    std::vector<u8> bytes;
    std::string sha256;
    size_t files = 0;
    size_t weapons = 0, characters = 0, parts = 0, maps = 0;
    std::vector<std::string> notes;   // what an author should know: nothing here stops the pack
};

inline bool fail(std::string* why, std::string text) {
    if (why) *why = std::move(text);
    return false;
}

inline std::filesystem::path temp_layer(const char* tag) {
    u8 salt[4];
    eng::crypto::random_bytes(salt);
    std::error_code ec;
    return std::filesystem::temp_directory_path(ec) / eng::str::format("lsf-%s-%02x%02x%02x%02x", tag, salt[0], salt[1], salt[2], salt[3]);
}

// PK-7: a map as sfcheck proves the client's own: it loads whole, every texture resolves, both
// sides have somewhere to stand, the ground can be walked, and a game can be played on it.
inline bool check_map(const sf::Data& data, const std::string& id, std::vector<std::string>& notes, std::string* why) {
    std::string err;
    auto level = sf::load_level(data, id, sf::kLevelAll, &err);
    if (!level) return fail(why, id + ": it does not load: " + err);
    if (level->stats.missing_textures) return fail(why, id + eng::str::format(": %zu of its textures are in neither the pack nor the base game", level->stats.missing_textures));
    if (level->stats.missing_lightmaps) notes.push_back(id + eng::str::format(": %zu lightmaps are missing (those sectors are lit flat)", level->stats.missing_lightmaps));
    if (level->collision_indices.empty()) return fail(why, id + ": it has no collision (sf_m_<name>_c.cft): nobody could stand on it");
    size_t red = 0, blue = 0;
    for (const sf::LevelSpawn& s : level->spawns) red += s.team != sf::Team::Blue, blue += s.team != sf::Team::Red;
    if (!red || !blue) return fail(why, id + ": its world script gives one side no spawn");
    const MapRules rules = map_rules(*level);
    std::string modes;
    for (int m = 0; m < int(Mode::Count); ++m)
        if (mode_offers(Mode(m), rules)) modes += std::string(modes.empty() ? "" : ", ") + mode_name(Mode(m));
    if (modes.empty()) return fail(why, id + ": its world script offers no game");
    eng::CollisionMesh collision;
    std::vector<u8> surfaces(level->collision_surfaces.size());
    for (size_t i = 0; i < surfaces.size(); ++i) surfaces[i] = u8(level->collision_surfaces[i]);
    collision.build(level->collision_vertices, level->collision_indices, surfaces);
    NavGraph nav;
    nav.build(collision, MovementDef{}, collision.bounds());
    if (nav.stats().nodes == 0) return fail(why, id + ": nowhere on it can be walked (its navigation is empty)");
    notes.push_back(id + eng::str::format(": %zu triangles, %zu + %zu spawns, %zu cells to walk; plays ", level->stats.triangles, red, blue, nav.stats().nodes) + modes);
    return true;
}

// `source` holds pack.cfg; `fills` is the library its files go to unless a top folder says another.
// `data` is the Soldier Front data folder, with nothing mounted; `others` the server's other packs
// (their names are taken: NM-2).
inline bool build(const std::filesystem::path& source, pack::Library fills, sf::Data& data, const pack::BaseIndex& index, const std::vector<pack::Loaded>& others,
                  Built& out, std::string* why) {
    using eng::json::Value;
    out = Built{};
    const auto cfg_bytes = eng::fs::read_file(source / "pack.cfg");
    if (!cfg_bytes) return fail(why, "there is no pack.cfg in " + eng::str::narrow(source.wstring()));
    eng::ConfigFile cfg;
    std::string bad;
    if (!eng::ConfigFile::parse(std::string_view(reinterpret_cast<const char*>(cfg_bytes->data()), cfg_bytes->size()), cfg, &bad)) return fail(why, "pack.cfg: " + bad);
    const eng::ConfigSection& root = cfg.root();
    const std::string id = root.get_string("id");
    if (!pack::id_ok(id)) return fail(why, "pack.cfg: id is lower case letters, digits and _, 3 to 24 of them (NM-9)");
    out.id = id;
    out.version = u32(std::max(1, root.get_int("version", 1)));
    const std::string mine = "x/" + id + "/";

    // ── The files ──────────────────────────────────────────────────────────────
    pack::Pack p;
    p.id = id;
    p.version = out.version;
    std::vector<std::vector<u8>> files;
    {
        struct Found {
            pack::Entry entry;
            std::filesystem::path file;
        };
        std::vector<Found> found;
        std::error_code ec;
        for (std::filesystem::recursive_directory_iterator it(source, ec), end; !ec && it != end; it.increment(ec)) {
            if (!it->is_regular_file(ec)) continue;
            std::string rel = sf::lower(eng::str::narrow(std::filesystem::relative(it->path(), source, ec).generic_wstring()));
            if (rel == "pack.cfg" || rel.starts_with(".") || rel.find("/.") != std::string::npos) continue;
            Found f;
            f.entry.library = fills;
            // A top folder named after a library puts what is under it there.
            if (const size_t slash = rel.find('/'); slash != std::string::npos)
                for (u8 l = 0; l < u8(pack::Library::Count); ++l)
                    if (rel.compare(0, slash, pack::library_name(pack::Library(l))) == 0) {
                        f.entry.library = pack::Library(l);
                        rel.erase(0, slash + 1);
                        break;
                    }
            f.entry.key = mine + rel;
            if (!pack::key_ok(f.entry.key, id, &bad)) return fail(why, rel + ": " + bad + " -- a file's name is lower case letters, digits, _ - . only, and at most 120 with its folders");
            f.file = it->path();
            found.push_back(std::move(f));
        }
        if (found.size() > pack::kMaxEntries) return fail(why, "more files than one pack may hold (4,096)");
        std::sort(found.begin(), found.end(), [](const Found& a, const Found& b) {
            return a.entry.library != b.entry.library ? a.entry.library < b.entry.library : a.entry.key < b.entry.key;
        });
        u64 total = 0;
        for (Found& f : found) {
            auto bytes = eng::fs::read_file(f.file);
            if (!bytes) return fail(why, f.entry.key + ": it cannot be read");
            total += bytes->size();
            if (total > pack::kMaxPackBytes) return fail(why, "the pack is over 50 MB (DL-6): a server's packs may be 50 MB together");
            f.entry.size = u32(bytes->size());
            p.entries.push_back(std::move(f.entry));
            files.push_back(std::move(*bytes));
        }
    }
    out.files = files.size();

    // ── The definitions ────────────────────────────────────────────────────────
    Value defs = Value::object();
    defs["author"] = eng::str::sanitize_line(root.get_string("author"), 64);
    Value weapons = Value::array(), forces = Value::array(), parts = Value::array(), maps = Value::array();
    auto folder_key = [&](std::string text) {
        text = sf::lower(text);
        std::replace(text.begin(), text.end(), '\\', '/');
        while (!text.empty() && text.back() == '/') text.pop_back();
        return mine + text;
    };
    static const char* const kClasses[] = {"rifle", "smg", "sniper", "machinegun", "shotgun", "pistol", "knife", "grenade"};
    static const char* const kGrenades[] = {"none", "frag", "flash", "smoke", "gas"};
    struct Field {
        const char* cfg;
        const char* group;   // "" or the object it sits in
        const char* key;
        char kind;           // f float, i whole, b yes/no, s text
    };
    static const Field kFields[] = {
        {"price", "", "price", 'i'}, {"damage", "", "damage", 'f'}, {"head", "", "head", 'f'}, {"leg", "", "leg", 'f'}, {"range", "", "range", 'f'},
        {"falloff_start", "", "falloff_start", 'f'}, {"falloff_min", "", "falloff_min", 'f'}, {"rpm", "", "rpm", 'f'}, {"automatic", "", "automatic", 'b'},
        {"magazine", "", "magazine", 'i'}, {"reserve", "", "reserve", 'i'}, {"reload", "", "reload", 'f'}, {"draw", "", "draw", 'f'},
        {"spread_stand", "spread", "stand", 'f'}, {"spread_crouch", "spread", "crouch", 'f'}, {"spread_move", "spread", "move", 'f'},
        {"spread_move_rate", "spread", "move_rate", 'f'}, {"spread_per_shot", "spread", "per_shot", 'f'}, {"spread_max", "spread", "max", 'f'},
        {"spread_recover", "spread", "recover", 'f'}, {"spread_run", "spread", "run", 'f'}, {"spread_air", "spread", "air", 'f'}, {"spread_walk", "spread", "walk", 'f'},
        {"recoil_up", "recoil", "up", 'f'}, {"recoil_up_max", "recoil", "up_max", 'f'}, {"recoil_side", "recoil", "side", 'f'},
        {"recoil_side_max", "recoil", "side_max", 'f'}, {"recoil_recover", "recoil", "recover", 'f'}, {"move_speed", "", "move_speed", 'f'},
        {"grip", "", "grip", 'i'}, {"carried", "", "carried", 's'}, {"zoom", "scope", "zoom", 'f'}, {"scope_image", "scope", "image", 's'},
        {"scope_move", "scope", "move", 'f'}, {"pellets", "", "pellets", 'i'}, {"fuse", "", "fuse", 'f'}, {"blast_radius", "", "blast_radius", 'f'},
        {"melee_range", "", "melee_range", 'f'},
    };
    for (const eng::ConfigSection* sec : cfg.all("weapon")) {
        const std::string name = sec->get_string("name");
        const WeaponDef* like = weapon_by_model(sec->get_string("like"));
        if (!like) return fail(why, "pack.cfg line " + std::to_string(sec->line) + ": weapon " + name + ": `like` names the base gun it starts from (ak74, m4a1 ...)");
        Value w = pack::weapon_json(*like);
        w["name"] = name;
        w["title"] = sec->get_string("title", name);
        w["like"] = like->model;
        w["model"] = sec->has("model") ? folder_key(sec->get_string("model")) : "base:" + like->model;
        if (sec->has("class")) {
            const std::string c = sf::lower(sec->get_string("class"));
            int klass = -1;
            for (int k = 0; k < 8; ++k)
                if (c == kClasses[k]) klass = k;
            if (klass < 0) return fail(why, name + ": class is one of rifle, smg, sniper, machinegun, shotgun, pistol, knife, grenade");
            w["class"] = klass;
        }
        if (sec->has("grenade")) {
            const std::string g = sf::lower(sec->get_string("grenade"));
            int kind = -1;
            for (int k = 0; k < 5; ++k)
                if (g == kGrenades[k]) kind = k;
            if (kind < 0) return fail(why, name + ": grenade is one of none, frag, flash, smoke, gas");
            w["grenade"] = kind;
        }
        for (const Field& f : kFields) {
            if (!sec->has(f.cfg)) continue;
            Value v;
            if (f.kind == 'f') v = double(sec->get_float(f.cfg));
            else if (f.kind == 'i') v = sec->get_int(f.cfg);
            else if (f.kind == 'b') v = sec->get_bool(f.cfg);
            else v = sec->get_string(f.cfg);
            if (*f.group) w[f.group][f.key] = std::move(v);
            else w[f.key] = std::move(v);
        }
        if (!sec->has("price")) return fail(why, name + ": it needs a price in SP (SH-8)");
        weapons.push(std::move(w));
    }
    for (const eng::ConfigSection* sec : cfg.all("character")) {
        Value f;
        const std::string name = sec->get_string("name");
        f["name"] = name;
        f["title"] = sec->get_string("title", name);
        f["nation"] = sec->get_string("nation");
        f["model"] = folder_key(sec->get_string("model", "sf_c_" + name));
        f["art"] = sf::lower(sec->get_string("art"));
        f["price"] = sec->get_int("price", 0);
        f["speed"] = double(sec->get_float("speed", 1.0f));
        f["upper_defense"] = double(sec->get_float("upper_defense", 0));
        f["lower_defense"] = double(sec->get_float("lower_defense", 0));
        f["avoid_headshot"] = double(sec->get_float("avoid_headshot", 0));
        forces.push(std::move(f));
    }
    for (const eng::ConfigSection* sec : cfg.all("part")) {
        Value part;
        const std::string name = sec->get_string("name");
        part["name"] = name;
        part["title"] = sec->get_string("title", name);
        part["info"] = sec->get_string("info");
        part["tab"] = sec->get_string("tab", "Accessory");
        if (sec->has("character")) {
            part["force"] = "x:" + id + ":" + sec->get_string("character");
        } else {
            const ForceDef* base = force_by_model(sec->get_string("force"));
            if (!base) return fail(why, name + ": a part names `character` (one of this pack's) or `force` (a base force: delta, sas ...)");
            part["force"] = int(base->id);
        }
        part["slot"] = sec->get_int("slot", 0);
        part["model"] = sf::lower(sec->get_string("model"));
        part["picture"] = sf::lower(sec->get_string("picture"));
        part["price"] = sec->get_int("price", 0);
        part["speed"] = double(sec->get_float("speed", 0));
        parts.push(std::move(part));
    }
    for (const eng::ConfigSection* sec : cfg.all("map")) {
        Value m;
        const std::string name = sf::lower(sec->get_string("name"));
        m["name"] = name;
        m["title"] = sec->get_string("title", name);
        m["night"] = sec->get_bool("night", false);
        m["folder"] = mine + "ground/sf_m_" + name;
        maps.push(std::move(m));
    }
    out.weapons = weapons.size(), out.characters = forces.size(), out.parts = parts.size(), out.maps = maps.size();
    defs["weapons"] = std::move(weapons);
    defs["parts"] = std::move(parts);
    defs["maps"] = std::move(maps);

    // ── What the models say for themselves ─────────────────────────────────────
    // The files are laid over the data as the server and the game will lay them, and each
    // character is measured there.
    const std::filesystem::path layer = temp_layer("build");
    struct Unmount {
        sf::Data& data;
        std::filesystem::path layer;
        ~Unmount() {
            data.set_session({});
            pack::remove_layer(layer);
        }
    } unmount{data, layer};
    auto mounted = [&](const Value& with, pack::Loaded& l) {
        Value d = with;
        p.definitions = d.dump();
        if (p.definitions.size() > pack::kMaxDefinitions) return fail(why, "pack.cfg defines more than a pack can carry");
        if (!pack::load_bytes(pack::write(p, files), l, &bad)) return fail(why, bad);
        std::vector<pack::Loaded> set = others;
        set.push_back(l);
        data.set_session({});
        if (!pack::write_layer(layer, set, &bad)) return fail(why, bad);
        data.set_session(layer);
        return true;
    };
    pack::Loaded first;
    {
        Value d = defs;
        d["forces"] = Value::array();
        if (!mounted(d, first)) return false;
    }
    for (Value& f : forces.elements_mut()) {
        pack::ForceMeasure m;
        if (!pack::measure_force(data, f["model"].str(), m, &bad)) return fail(why, f["name"].str() + ": " + bad);
        f["bones"] = m.bones;
        f["height"] = double(m.height);
        f["width"] = double(m.width);
        Value clips = Value::array();
        for (const std::string& c : m.clips) clips.push(c);
        f["clips"] = std::move(clips);
    }
    defs["forces"] = std::move(forces);

    // ── The checks the server and the game make (PK-4), and a map's own (PK-7) ──
    pack::Loaded built;
    if (!mounted(defs, built)) return false;
    std::vector<pack::Loaded> set = others;
    set.push_back(built);
    if (!pack::check_set(set, index.lookup(), &bad)) return fail(why, bad);
    std::vector<std::pair<pack::Pack, std::string>> named;
    std::vector<u32> sizes;
    for (const pack::Loaded& l : set) named.emplace_back(l.pack, l.sha256), sizes.push_back(u32(l.bytes.size()));
    SessionContent content;
    if (!pack::registry_from_manifest(pack::manifest(named, sizes), content, &bad)) return fail(why, bad);
    if (!pack::check_mounted(data, content, &bad)) return fail(why, bad);
    for (const PackMap& m : content.maps)
        if (m.pack == id && !check_map(data, m.id, out.notes, &bad)) return fail(why, bad);
    // PF-4: a phone's renderer takes pictures of 2,048 a side at most.
    for (const pack::Entry& e : built.pack.entries) {
        const std::string_view l = pack::leaf(e.key);
        if (!(l.ends_with(".dds") || l.ends_with(".tga") || l.ends_with(".jpg") || l.ends_with(".png") || l.ends_with(".bmp"))) continue;
        pack::Pack one;
        one.id = id;
        one.entries.push_back(e);
        if (!pack::check_files(one, built.bytes, nullptr, pack::kMaxTextureAndroid)) {
            out.notes.push_back(std::string(l) + " is over 2,048 a side: players on Android cannot join a server with this pack (§11.12)");
            break;
        }
    }
    out.sha256 = built.sha256;
    out.bytes = std::move(built.bytes);
    return true;
}

}  // namespace lsf::packbuild
