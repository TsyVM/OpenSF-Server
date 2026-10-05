SOLDIER FRONT LEGACY SERVER
===========================

Your own Soldier Front Legacy server. Players sign in with their Team Vanilla account, find your
server on the list (or join it by address), and play. Their rank, record and SP are Team Vanilla's
and follow them everywhere; what your server adds of its own stays on your server.

  1. What is in this folder
  2. Starting it for the first time
  3. Registering it
  4. server.cfg
  5. Letting players in: the port, the firewall, your address
  6. Running it day to day (Windows, Linux, a container)
  7. Staff, channels and the shop
  8. Your own weapons, maps and characters (packs)
  9. What is yours and what is Team Vanilla's
 10. Updating
 11. When something is wrong


1. WHAT IS IN THIS FOLDER
-------------------------

  LegacySFServer.exe       the server (Windows)        legacysf-server (Linux)
  data/                    the Soldier Front game data: the server reads the maps from it
                           (where bots walk, what a soldier can see, what a wall stops)
  server.cfg               its settings, to fill in (see 4)
  server.key               its identity, made on the first run: it is not in the download, and
                           every server makes its own. NEVER share it, never send it anywhere,
                           never put it in a backup that leaves your hands.
  channels.cfg             its channels: the original thirteen to start with (edited in the game too)
  games.cfg                the game types and maps switched off on the whole server (made when a
                           Game Master first saves the Games tab; none: everything is played)
  staff.cfg                who its staff are, by Team Vanilla account (empty to start with)
  shop.cfg                 its own shop: your packs' items, its capsules, base items' prices
                           (the game's own catalog to start with)
  accounts/                what THIS SERVER knows about each player (see 9)
  weapons/ maps/ chars/    your packs' sources (see 8)
  packs/                   your built packs, served to players
  recordings/              every match, kept 7 days
  logs/                    one log a start
  tools/                   lsfpack (builds and checks packs), sfcheck (checks maps and data)
  files.sha256             every file here and its SHA-256, to check nothing is missing or damaged


2. STARTING IT FOR THE FIRST TIME
---------------------------------

Open server.cfg and give the server its name. Then run the server once. It makes server.key and
then stops, because it has no identity yet: a server is never "started anyway" without one.

  Windows:   LegacySFServer.exe
  Linux:     ./legacysf-server

Lost a file? LegacySFServer.exe --defaults (./legacysf-server --defaults) writes any of
server.cfg, channels.cfg, staff.cfg and shop.cfg that is missing, and the empty folders, leaving
everything that is there as it is.


3. REGISTERING IT
-----------------

Every server is registered to a Team Vanilla account: its owner's.

  1. Sign in to the control panel at https://api.teamvanilla.dev/ucp with your Team Vanilla
     account and choose Servers, Register a server. Confirm you have the right to share whatever
     packs it will serve. You are given a code.
  2. Run the server once with that code:

       LegacySFServer.exe --register CODE          ./legacysf-server --register CODE

     It trades the code for its id and writes it into server.cfg.
  3. Start the server normally.

A new server is listed as Unverified. Progress on it counts in full from the first match.
SFLegacy Staff may mark a server Verified; nothing else changes with the badge.


4. SERVER.CFG
-------------

Every value is checked when the server starts. One that is out of range stops the server with
the line and the reason.

  server_id         from the registration
  name              3 to 48 characters; shown in the list
  motd              the message a player reads on joining (at most 200 characters)
  region            a word or two for the list (at most 24 characters)
  bind_address      which of this machine's addresses to listen on (empty: all)
  port              the UDP port (default 27240). It also answers the list's "who is there?"
  public_address    the address players reach it at, when that is not the one Team Vanilla sees
                    the server's heartbeat come from (empty: that one)
  public_port       the outside port, when your router forwards a different one to `port`
  max_players       1 to 512
  reserved_slots    slots kept for staff
  min_rank max_rank min_kd      who may join: ranks 0 to 74 and a kill/death ratio
  password          a private server
  listed            yes or no. no: off the list; friends still join by address
  client_data       the game data folder (empty: data/ here)
  upload_kbps       the most of your upload pack downloads may use, in all
  max_downloaders   how many players download at once; the rest wait their turn
  log_days          how long logs are kept

  LegacySFServer.exe --check     reads server.cfg, the data and the packs, says what is wrong
                                 or that all is well, and stops. It talks to nobody.


5. LETTING PLAYERS IN
---------------------

THE PORT. On a home connection, forward UDP `port` (27240) on your router to this PC's local
address, and give the PC a fixed local address so the forward keeps pointing at it.

THE WINDOWS FIREWALL. The first run offers to let the server in (Windows asks for an
administrator). Later: LegacySFServer.exe --firewall. On Linux, allow UDP on the port in the
system's own firewall.

YOUR ADDRESS. Leave public_address empty and Team Vanilla lists the server at whatever address
its heartbeat comes from, so a home connection whose address changes is picked up by itself. A
host name (a dynamic DNS name, sf.example.com) works too.

Team Vanilla checks that your server answers from the internet before listing it. If it cannot be
reached, the log says so and why; the server still runs for anyone who can reach it by address.

BEHIND A VPN. A server only reachable inside a VPN plays there, unlisted. To be listed it has to
be reachable from the internet.

Addresses are IPv4.


6. RUNNING IT DAY TO DAY
------------------------

Ctrl+C (or SIGTERM) saves and stops. A crash writes a report to logs/.

WINDOWS. Keep the PC awake while it hosts: sleep off, automatic restarts off. To start it again by
itself after a reboot, add a task in Task Scheduler that runs LegacySFServer.exe "At startup",
whether or not anyone is signed in, starting in this folder.

LINUX. The server is one program with no installer (glibc 2.28 or later: Debian 10, Ubuntu 18.10,
RHEL 8 and newer; x86-64, and ARM64 in arm64/). It reaches Team Vanilla through the system's own
libcurl and certificates, which nearly every installation already has (Debian and Ubuntu: the
packages libcurl4 and ca-certificates; RHEL and Fedora: libcurl and ca-certificates). As a service:

  sudo useradd --system --home /opt/legacysf-server --shell /usr/sbin/nologin legacysf
  sudo cp -r . /opt/legacysf-server && sudo chown -R legacysf: /opt/legacysf-server
  sudo cp legacysf-server.service /etc/systemd/system/
  sudo systemctl daemon-reload && sudo systemctl enable --now legacysf-server
  journalctl -u legacysf-server -f

Register it first (as that user, in that folder):  sudo -u legacysf ./legacysf-server --register CODE

A game data folder copied from Windows may have its folders in another case (Area for area): the
server finds them either way.

A CONTAINER. Containerfile builds an image of this folder:

  podman build -t legacysf-server -f Containerfile .        (or docker build)
  podman run --rm -v sf-state:/srv/state legacysf-server --register CODE
  podman run -d --name sf -p 27240:27240/udp -v sf-state:/srv/state legacysf-server

Everything the server writes (server.cfg, server.key, accounts, packs, recordings, logs) is in the
volume. Keep the volume: server.key in it is the server's identity.

UPLOAD. A home connection's upload is small, and pack downloads use it. upload_kbps keeps them
from crowding the matches out; the 50 MB limit on packs keeps a first join short.


7. STAFF, CHANNELS AND THE SHOP
-------------------------------

STAFF. You, the owner, are this server's Owner: by the registration at Team Vanilla, not by a
file, so losing server.cfg never loses the server and editing it never grants it. In the game, on
your server, the staff panel (F9) names your staff:

  Admin          your staff below Admin, the channels, the rules, the shop
  Game Master    the rules, rooms and matches, the game types and maps played, the shop,
                 players' data on this server (their custom items and loadouts here), bans
                 from this server
  Moderator      people: kick and mute; reports and recordings

Nobody can raise anyone to their own level or above. Your staff's reach ends at your server:
nobody on it can touch a player's Team Vanilla account, rank, SP or universal items, and nobody on
it can act on SFLegacy Staff, who may join any server past its gates. Every staff action is
logged, and Team Vanilla is sent the log. staff.cfg holds the roles by account id.

CHANNELS. channels.cfg lists your channels: each one's name, kind, who may enter (ranks, K/D),
how many it holds and which game types it offers. Edit it in the game (the Channels tab of the
staff panel) or by hand with the server stopped. A file that cannot be read falls back to the
original thirteen channels, and the log says so: a server is never left with none. No channel may
hold more than the server's own max_players.

GAMES. Your Game Masters choose what the whole server plays: the Games tab of the staff panel has a
tick for each game type and ON or OFF for each map. A game type switched off (Horror Mode, the
Pirate Ship, any of them) is never made, changed to or started, and a room that already has it
picks another before it starts; a map switched off is never offered or drawn at random. At least
one game type stays on. It is kept in games.cfg:

  modes_off = HOR,HR2,PIR        short names (TB TDM SB SNP CTC CPT HOR TRN TS OCC HR2 PIR) or full
  maps_off = killhouse           map ids

A channel's own game types and maps narrow this further, never back.

THE SHOP. Your shop sells your packs' items for SP at the prices you set (never free), and base
items at or above Team Vanilla's price, never below. Edit it in the game (the Shop editor of the
staff panel). A sale needs the player's own approval at Team Vanilla each time: a server cannot
spend a player's SP by itself, and cannot give SP out. Duffle Bags and Team Vanilla's capsules are
Team Vanilla's: every server sells the same ones, and only the official server's Game Masters
change them; your Shop editor shows them to read.


8. YOUR OWN WEAPONS, MAPS AND CHARACTERS
----------------------------------------

PACKS.txt explains it in full. In short:

  weapons/<pack>/pack.cfg     maps/<pack>/pack.cfg     chars/<pack>/pack.cfg

each with its files beside it, then

  tools/lsfpack build --data data

builds packs/<pack>.lsfpack, checking each on the way. Restart the server to serve them. A server
that finds a broken pack does not start, and says which and why.

Packs add; they never change the base game. They are data only. Together they may be 50 MB.


9. WHAT IS YOURS AND WHAT IS TEAM VANILLA'S
-------------------------------------------

accounts/ is NOT the players' accounts. Nobody's account lives on your server. It holds what your
server knows about a Team Vanilla account: its role here, bans and mutes here, the custom items it
owns here, its loadout here. There is no money in it and nothing universal.

Team Vanilla keeps every player's sign-in, rank, record, SP, Coins, base items, clan, friends and
mail. Your server never sees a password: a player's game shows it a short-lived ticket from Team
Vanilla, and that is all your server learns about who they are.

Matches on your server count toward players' rank and record like anywhere else. Team Vanilla
checks every match report; one that could not have happened is refused, and SFLegacy Staff can
take back what a server reported.

If SFLegacy Staff close a server, or it stays off for 60 days, players are refunded what they
spent on its custom items.

Recordings are kept 7 days.


10. UPDATING
------------

A release's game and server are one version: replace the server's program (and tools/) with the
new release's, keep everything else. A server older than Team Vanilla accepts is taken off the
list, and its log says so; a player with an older or newer game is told which of the two is out
of date.

Back up: server.cfg, channels.cfg, games.cfg, staff.cfg, shop.cfg, accounts/, your pack sources. Keep
server.key safe and to yourself.


11. WHEN SOMETHING IS WRONG
---------------------------

  It will not start            it says why, in words. --check says it without starting.
  It is not on the list        the log says whether the heartbeat is accepted and whether Team
                               Vanilla could reach the server from outside (the port, the firewall).
  Players cannot join          the port and the firewall (5); min_rank / max_rank / min_kd; the password.
  A pack is refused            tools/lsfpack check packs --data data  says which rule and which file.
  A crash                      logs/ holds the report. Send it to Team Vanilla with the log beside it.
