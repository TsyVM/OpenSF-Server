# The source lists, shared by the Windows build (CMakeLists.txt) and Android's
# (Client/Android/CMakeLists.txt).
get_filename_component(LSF_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# Soldier Front's own file formats (vendored SFSDK data readers).
set(LSF_SFSDK_SOURCES
    ${LSF_ROOT}/vendor/SFSDK/src/data/sff.cpp
    ${LSF_ROOT}/vendor/SFSDK/src/data/mrg.cpp
    ${LSF_ROOT}/vendor/SFSDK/src/data/archive.cpp
    ${LSF_ROOT}/vendor/SFSDK/src/data/asset_library.cpp
    ${LSF_ROOT}/vendor/SFSDK/src/data/map.cpp
    ${LSF_ROOT}/vendor/SFSDK/src/data/cft.cpp
    ${LSF_ROOT}/vendor/SFSDK/src/data/msf.cpp
    ${LSF_ROOT}/vendor/SFSDK/src/data/wld.cpp
    ${LSF_ROOT}/vendor/SFSDK/src/data/env.cpp
    ${LSF_ROOT}/vendor/SFSDK/src/data/worldscript.cpp
    ${LSF_ROOT}/vendor/SFSDK/src/data/xml.cpp
    ${LSF_ROOT}/vendor/SFSDK/src/data/osf.cpp
    ${LSF_ROOT}/vendor/SFSDK/src/data/map_bundle.cpp
    ${LSF_ROOT}/vendor/SFSDK/src/data/tga.cpp
    ${LSF_ROOT}/vendor/SFSDK/src/data/dds.cpp
    ${LSF_ROOT}/vendor/SFSDK/src/data/cfg.cpp)

# What the client and the server share: the engine's core, the network, the Soldier Front
# loaders and the game's rules and protocol.
set(LSF_SHARED_SOURCES
    ${LSF_ROOT}/Shared/Engine/Core/Log.cpp
    ${LSF_ROOT}/Shared/Engine/Core/Time.cpp
    ${LSF_ROOT}/Shared/Engine/Core/Strings.cpp
    ${LSF_ROOT}/Shared/Engine/Core/FileSystem.cpp
    ${LSF_ROOT}/Shared/Engine/Core/Config.cpp
    ${LSF_ROOT}/Shared/Engine/Core/CrashHandler.cpp
    ${LSF_ROOT}/Shared/Engine/Core/Xml.cpp
    ${LSF_ROOT}/Shared/Engine/Core/Crypto.cpp
    ${LSF_ROOT}/Shared/Engine/Core/Ed25519.cpp
    ${LSF_ROOT}/Shared/Engine/Core/Json.cpp
    ${LSF_ROOT}/Shared/Engine/Physics/CollisionMesh.cpp
    ${LSF_ROOT}/Shared/Engine/Asset/ImageDecode.cpp
    ${LSF_ROOT}/Shared/Engine/Asset/ModelFile.cpp
    ${LSF_ROOT}/Shared/Engine/Asset/TextureFile.cpp
    ${LSF_ROOT}/Shared/Engine/Net/Socket.cpp
    ${LSF_ROOT}/Shared/Engine/Net/Connection.cpp
    ${LSF_ROOT}/Shared/Engine/Net/NetHost.cpp
    ${LSF_ROOT}/Shared/Engine/Net/Https.cpp
    ${LSF_ROOT}/Shared/SF/Data.cpp
    ${LSF_ROOT}/Shared/SF/Image.cpp
    ${LSF_ROOT}/Shared/SF/Lma.cpp
    ${LSF_ROOT}/Shared/SF/Level.cpp
    ${LSF_ROOT}/Shared/SF/Model.cpp
    ${LSF_ROOT}/Shared/SF/UiData.cpp
    ${LSF_ROOT}/Shared/SF/ClanMarks.cpp
    ${LSF_ROOT}/Shared/SF/Tables.cpp
    ${LSF_ROOT}/Shared/SF/SoundTables.cpp
    ${LSF_ROOT}/Shared/Game/Rules.cpp
    ${LSF_ROOT}/Shared/Game/Events.cpp
    ${LSF_ROOT}/Shared/Game/Modes.cpp
    ${LSF_ROOT}/Shared/Game/Items.cpp
    ${LSF_ROOT}/Shared/Game/Protocol.cpp
    ${LSF_ROOT}/Shared/Game/Replay.cpp
    ${LSF_ROOT}/Shared/Game/Navigation.cpp
    ${LSF_ROOT}/Shared/Game/Movement.cpp
    ${LSF_ROOT}/Shared/Game/Ballistics.cpp
    ${LSF_ROOT}/Shared/Game/Wear.cpp
    ${LSF_ROOT}/Shared/Game/Shop.cpp
    ${LSF_ROOT}/Shared/Game/Registry.cpp
    ${LSF_ROOT}/Shared/Game/TvasJson.cpp
    ${LSF_ROOT}/Shared/Game/TvasProto.cpp
    ${LSF_ROOT}/Shared/Game/Tvas.cpp
    ${LSF_ROOT}/Shared/Game/Pack.cpp
    ${LSF_ROOT}/Shared/Game/PackMount.cpp
    ${LSF_SFSDK_SOURCES})

# The server's core: also linked into the game, which hosts a server for a player's own machine.
set(LSF_SERVER_CORE_SOURCES
    ${LSF_ROOT}/Server/Source/Accounts.cpp
    ${LSF_ROOT}/Server/Source/Server.cpp
    ${LSF_ROOT}/Server/Source/Social.cpp
    ${LSF_ROOT}/Server/Source/Shop.cpp
    ${LSF_ROOT}/Server/Source/Staff.cpp
    ${LSF_ROOT}/Server/Source/TvasLink.cpp
    ${LSF_ROOT}/Server/Source/Content.cpp
    ${LSF_ROOT}/Server/Source/Maps.cpp
    ${LSF_ROOT}/Server/Source/Bots.cpp
    ${LSF_ROOT}/Server/Source/Match.cpp
    ${LSF_ROOT}/Server/Source/Modes.cpp)
set(LSF_SERVER_SOURCES ${LSF_ROOT}/Server/Source/main.cpp)
