// lsfpack: makes and checks a server's packs (Docs/UniversalServerDeploy.md §11).
//
//   lsfpack build [<server folder>] --data <Soldier Front data folder> [--out <packs folder>]
//       Every weapons/<pack>, maps/<pack> and chars/<pack> of the server's folder (default: the
//       folder it is run in) into packs/<pack>.lsfpack, each checked on the way exactly as the server and
//       the game will check it (PK-4), and all of them together (NM-2, DL-6).
//   lsfpack build <one source folder> --kind weapons|maps|chars --data <...> [--out <folder>]
//       Just that one.
//   lsfpack check <packs folder> --data <...>
//       What the server does when it starts, and each map as sfcheck proves the client's own.
//   lsfpack list <file.lsfpack>
//       What a pack holds.
//
// Tools/common/PackBuild.hpp says how a source folder and its pack.cfg are laid out. The exit code
// is 0 when everything asked for was made or passed.
#include "Engine/Core/FileSystem.hpp"
#include "Engine/Core/Log.hpp"
#include "Engine/Core/Strings.hpp"
#include "Game/Pack.hpp"
#include "Game/PackMount.hpp"
#include "SF/Data.hpp"

#include "../common/PackBuild.hpp"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {

namespace stdfs = std::filesystem;
using lsf::pack::Library;

std::string narrow(const stdfs::path& p) { return eng::str::narrow(p.wstring()); }

int usage() {
    std::fprintf(stderr,
                 "lsfpack build [<server folder>] --data <Soldier Front data folder> [--out <packs folder>]\n"
                 "lsfpack build <source folder> --kind weapons|maps|chars --data <...> [--out <folder>]\n"
                 "lsfpack check <packs folder> --data <...>\n"
                 "lsfpack list <file.lsfpack>\n");
    return 2;
}

bool open_data(sf::Data& data, const stdfs::path& folder) {
    if (folder.empty()) {
        std::fprintf(stderr, "lsfpack: name the Soldier Front data folder with --data (the game's own data/, which the server's client_data names too)\n");
        return false;
    }
    if (!data.open(folder)) {
        std::fprintf(stderr, "lsfpack: %s is not a Soldier Front data folder\n", narrow(folder).c_str());
        return false;
    }
    return true;
}

int list(const stdfs::path& file) {
    lsf::pack::Loaded l;
    std::string why;
    if (!lsf::pack::load_file(file, l, &why)) {
        std::fprintf(stderr, "%s: %s\n", narrow(file.filename()).c_str(), why.c_str());
        return 1;
    }
    std::printf("%s version %u: %zu files, %.2f MB, SHA-256 %s\n", l.pack.id.c_str(), l.pack.version, l.pack.entries.size(), double(l.bytes.size()) / 1048576.0, l.sha256.c_str());
    eng::json::Value defs;
    if (eng::json::parse(l.pack.definitions, defs)) {
        for (const auto& w : defs["weapons"].elements()) std::printf("  weapon     x:%s:%-20s %s (like %s, %lld SP)\n", l.pack.id.c_str(), w["name"].str().c_str(), w["title"].str().c_str(), w["like"].str().c_str(), (long long)w["price"].as_int());
        for (const auto& f : defs["forces"].elements()) std::printf("  character  x:%s:%-20s %s (%lld bones, %lld SP)\n", l.pack.id.c_str(), f["name"].str().c_str(), f["title"].str().c_str(), (long long)f["bones"].as_int(), (long long)f["price"].as_int());
        for (const auto& p : defs["parts"].elements()) std::printf("  part       x:%s:%-20s %s (%lld SP)\n", l.pack.id.c_str(), p["name"].str().c_str(), p["title"].str().c_str(), (long long)p["price"].as_int());
        for (const auto& m : defs["maps"].elements()) std::printf("  map        x-%s-%-20s %s%s\n", l.pack.id.c_str(), m["name"].str().c_str(), m["title"].str().c_str(), m["night"].as_bool() ? " (night)" : "");
    }
    for (const lsf::pack::Entry& e : l.pack.entries) std::printf("  %-7s %9u  %s\n", lsf::pack::library_name(e.library), e.size, e.key.c_str());
    return 0;
}

int check(const stdfs::path& dir, const stdfs::path& data_dir) {
    sf::Data data;
    if (!open_data(data, data_dir)) return 2;
    std::vector<stdfs::path> files;
    std::error_code ec;
    for (const auto& e : stdfs::directory_iterator(dir, ec))
        if (e.path().extension() == ".lsfpack") files.push_back(e.path());
    std::sort(files.begin(), files.end());
    if (files.empty()) {
        std::printf("No packs in %s.\n", narrow(dir).c_str());
        return 0;
    }
    std::vector<lsf::pack::Loaded> set;
    std::string why;
    lsf::u64 total = 0;
    for (const stdfs::path& f : files) {
        lsf::pack::Loaded l;
        if (!lsf::pack::load_file(f, l, &why)) {
            std::fprintf(stderr, "%s: %s\n", narrow(f.filename()).c_str(), why.c_str());
            return 1;
        }
        std::printf("%-24s version %u, %zu files, %.2f MB\n", l.pack.id.c_str(), l.pack.version, l.pack.entries.size(), double(l.bytes.size()) / 1048576.0);
        total += l.bytes.size();
        set.push_back(std::move(l));
    }
    lsf::pack::BaseIndex index;
    index.build(data);
    if (!lsf::pack::check_set(set, index.lookup(), &why)) {
        std::fprintf(stderr, "The server would not start: %s\n", why.c_str());
        return 1;
    }
    std::vector<std::pair<lsf::pack::Pack, std::string>> named;
    std::vector<lsf::u32> sizes;
    for (const auto& l : set) named.emplace_back(l.pack, l.sha256), sizes.push_back(lsf::u32(l.bytes.size()));
    lsf::SessionContent content;
    const stdfs::path layer = lsf::packbuild::temp_layer("check");
    int failed = 0;
    if (!lsf::pack::registry_from_manifest(lsf::pack::manifest(named, sizes), content, &why) || !lsf::pack::write_layer(layer, set, &why)) {
        std::fprintf(stderr, "The server would not start: %s\n", why.c_str());
        failed = 1;
    } else {
        data.set_session(layer);
        if (!lsf::pack::check_mounted(data, content, &why)) {
            std::fprintf(stderr, "The server would not start: %s\n", why.c_str());
            failed = 1;
        }
        std::vector<std::string> notes;
        for (const lsf::PackMap& m : content.maps)
            if (!failed && !lsf::packbuild::check_map(data, m.id, notes, &why)) {
                std::fprintf(stderr, "%s\n", why.c_str());
                failed = 1;
            }
        for (const std::string& n : notes) std::printf("  %s\n", n.c_str());
        data.set_session({});
    }
    lsf::pack::remove_layer(layer);
    if (!failed)
        std::printf("%zu packs, %.2f of 50 MB: %zu weapons, %zu characters, %zu parts, %zu maps. They pass.\n", set.size(), double(total) / 1048576.0, content.weapons.size(),
                    content.forces.size(), content.items.size(), content.maps.size());
    return failed;
}

struct Source {
    stdfs::path folder;
    Library fills;
};

int build(const std::vector<Source>& sources, const stdfs::path& out_dir, const stdfs::path& data_dir) {
    sf::Data data;
    if (!open_data(data, data_dir)) return 2;
    lsf::pack::BaseIndex index;
    index.build(data);
    std::error_code ec;
    stdfs::create_directories(out_dir, ec);
    std::vector<lsf::pack::Loaded> done;
    lsf::u64 total = 0;
    for (const Source& s : sources) {
        lsf::packbuild::Built b;
        std::string why;
        if (!lsf::packbuild::build(s.folder, s.fills, data, index, done, b, &why)) {
            std::fprintf(stderr, "%s: %s\n", narrow(s.folder).c_str(), why.c_str());
            return 1;
        }
        const stdfs::path file = out_dir / (b.id + ".lsfpack");
        if (!eng::fs::write_file(file, b.bytes.data(), b.bytes.size())) {
            std::fprintf(stderr, "%s cannot be written\n", narrow(file).c_str());
            return 1;
        }
        std::printf("%s: %zu files, %.2f MB (%zu weapons, %zu characters, %zu parts, %zu maps)\n  SHA-256 %s\n", narrow(file.filename()).c_str(), b.files,
                    double(b.bytes.size()) / 1048576.0, b.weapons, b.characters, b.parts, b.maps, b.sha256.c_str());
        for (const std::string& n : b.notes) std::printf("  %s\n", n.c_str());
        total += b.bytes.size();
        lsf::pack::Loaded l;
        if (!lsf::pack::load_bytes(std::move(b.bytes), l, &why)) {
            std::fprintf(stderr, "%s: %s\n", b.id.c_str(), why.c_str());
            return 1;
        }
        done.push_back(std::move(l));
    }
    if (sources.empty()) std::printf("Nothing to build: no weapons/<pack>, maps/<pack> or chars/<pack> folder has a pack.cfg.\n");
    else std::printf("%zu packs, %.2f of 50 MB.\n", done.size(), double(total) / 1048576.0);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) return usage();
    const std::string verb = args[0];
    stdfs::path target, data, out;
    std::string kind;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--data" && i + 1 < args.size()) data = eng::str::widen(args[++i]);
        else if (args[i] == "--out" && i + 1 < args.size()) out = eng::str::widen(args[++i]);
        else if (args[i] == "--kind" && i + 1 < args.size()) kind = args[++i];
        else if (!args[i].starts_with("--") && target.empty()) target = eng::str::widen(args[i]);
        else return usage();
    }
    if (verb == "list") return target.empty() ? usage() : list(target);
    if (verb == "check") return target.empty() ? usage() : check(target, data);
    if (verb != "build") return usage();
    std::vector<Source> sources;
    if (!kind.empty()) {
        if (target.empty()) return usage();
        const Library fills = kind == "weapons" ? Library::Weapon : kind == "maps" ? Library::Area : kind == "chars" ? Library::Force : Library::Count;
        if (fills == Library::Count) return usage();
        sources.push_back({target, fills});
        if (out.empty()) out = target.parent_path();
    } else {
        std::error_code cwd;
        if (target.empty()) target = stdfs::current_path(cwd);
        const std::pair<const char*, Library> kKinds[] = {{"weapons", Library::Weapon}, {"maps", Library::Area}, {"chars", Library::Force}};
        std::error_code ec;
        for (const auto& [folder, fills] : kKinds) {
            std::vector<stdfs::path> found;
            for (const auto& e : stdfs::directory_iterator(target / folder, ec))
                if (e.is_directory(ec) && stdfs::exists(e.path() / "pack.cfg", ec)) found.push_back(e.path());
            std::sort(found.begin(), found.end());
            for (const stdfs::path& f : found) sources.push_back({f, fills});
        }
        if (out.empty()) out = target / "packs";
    }
    return build(sources, out, data);
}
