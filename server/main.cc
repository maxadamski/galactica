#include <unordered_set>

#include "game.hh"
#include "nicenet.hh"

using namespace std;

void xsend(i32 fd, string x) {
    x += "\n";
    const char *buffer = x.c_str();
    u32 length = x.size();
    u32 written = 0;
    while (written < length)
        written += write(fd, buffer + written, length - written);
}

map<i32, string> buffers;

string xrecv(i32 fd) {
    u8 prefix[4];
    read(fd, prefix, 4);

    u32 length = 0;
    for (i32 i = 0; i < 4; i++)
        length = length << 8 | prefix[i];

    u8 buffer[1024];
    u32 real_length = 0;
    while (real_length < length)
        real_length += read(fd, buffer, length - real_length);

    string s(buffer, buffer + length - 1);
    if (buffer[length-1] != '\n') {
        cout << "[warn] malformed message - " << s << endl;
        u8 sep[1] = {0};
        while (sep[0] != '\n') read(fd, sep, 1);
        cout << "[info] realigned stream\n";
    }

    return s;
}

template <size_t N>
int expect(string (&arr)[N], u32 n) {
    for (int i = 0; i < n; i++) {
        if (arr[i] == "") return false;
    }
    return true;
}

bool handle_request(i32 sfd, string &req);

// ----------------------------------------------------------------------------
// -- Global data
// ----------------------------------------------------------------------------

NicePoll nicepoll;
Game game;
unordered_set<i32> clients;

// ----------------------------------------------------------------------------
// -- Event handlers
// ----------------------------------------------------------------------------

void handle_client(i32 fd, u32 events) {
    if (events & EPOLLIN) {

        //const u64 max_buf = 1024;
        //i8 buf[max_buf] = {0};
        //i64 buf_len = read(fd, buf, max_buf);
        //if (buf_len <= 0) {
        //    //printf("[warn] could not read message from client %d\n", fd);
        //    return;
        //}

        //string req = string(buf, buf + buf_len);

        string req = xrecv(fd);
        cout << "[info] request - " << req << endl;
        try {
            handle_request(fd, req);
        } catch (const char*e) {
            cout << "[warn] exception at request - " << req << "\n";
        }

    }
    if (events & ~EPOLLIN) {
        cout << "[info] removing client " << fd << endl;
        nicepoll.erase(fd);
    }
}

void handle_server(i32 fd, u32 events) {
    if (events & EPOLLIN) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        i32 client = accept(fd, (sockaddr*) &client_addr, &client_len);
        if (client < 0) {
            cerr << "[warn] couldn't connect to client" << endl;
            return;
        }

        char *addr = inet_ntoa(client_addr.sin_addr);
        i32 port = ntohs(client_addr.sin_port);
        printf("[info] new connection from: %s:%hu (fd: %d)\n", addr, port, client);


        clients.insert(client);
        nicepoll.insert(client, EPOLLIN | EPOLLRDHUP, &handle_client);
    }
}

int main(int argc, char **argv) {
    const i32 port = 6666;
    const i32 max_pending = 32;
    const u64 max_events = 64;

    i32 server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0)
        fatal("could not create socket");

    if (make_reusable(server) < 0)
        fatal("could not make socket reusable");

    if (make_nonblocking(server) < 0)
        fatal("could not make socket non-blocking");

    sockaddr_in addr{AF_INET, htons(port), {INADDR_ANY}};
    if (bind(server, (sockaddr*) &addr, sizeof(addr)) < 0)
        fatal("could not bind socket");

    if (listen(server, max_pending) < 0)
        fatal("could not listen on socket");

    if (nicepoll.create() < 0)
        fatal("could not create epoll descriptor");

    nicepoll.insert(server, EPOLLIN, &handle_server);

    epoll_event events[max_events];

    game.init();
    auto t1 = now();
    auto t2 = t1;
    while (true) {
        if (game.reset) {
            game = Game();
            game.init();
        }

        t2 = now();
        game.step(millis(t2 - t1));
        t1 = now();

        i32 event_count = nicepoll.wait(events, max_events, -1);
        for (i32 i = 0; i < event_count; i++) {
            nicepoll.handle(events[i]);
        }
    }

    /*
    i32 sfd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in server_addr{AF_INET, htons(port), INADDR_ANY};
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &ONE, sizeof(ONE));
    bind(sfd, (sockaddr*) &server_addr, sizeof(server_addr));

    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    const u64 buf_max = 512;
    i8 buf[buf_max] = {0};

    game.init();
    auto t1 = now();
    while (true) {
        if (game.reset) {
            game = Game();
            game.init();
        }

        auto t2 = now();
        auto dt = millis(t2 - t1);
        game.step(dt);
        t1 = t2;

        size_t recv_len = recvfrom(sfd, buf, buf_max, 0, (sockaddr*) &client_addr, &client_len);
        string req = string(buf, buf+recv_len);

        //cout << "[info] request - " << req << endl;

        try {
            if (!handle_request(sfd, req, client_addr)) {
                // maybe handle the error?
            }
        } catch (const char*e) {
            cout << "[warn] exception at request - " << req << "\n";
        }
    }
    */
}

#define ARGS(arg, n, req) string arg[n]; if (!split(arg, req)) return false;

bool handle_request(i32 fd, string &req) {
    if (req.compare(0, 4, "ping") == 0) {
        auto res = "pong";
        xsend(fd, res);

    } else if (req.compare(0, 4, "join") == 0) {
        if (!game.finished) {
            auto player = game.spawn_player();
            auto res = "stat-join,"+player.encode()+","+game.encode();
            xsend(fd, res);
        }

    } else if (req.compare(0, 8, "ask-game") == 0) {
        string arg[2];
        if (!split(arg, req)) return false;

        auto res = "stat-game,-1,"+game.encode();
        xsend(fd, res);

    } else if (req.compare(0, 10, "ask-myship") == 0) {
        string arg[2];
        if (!split(arg, req)) return false;
        i32 id = I(arg[1]);
        Player &player = game.players.data[id];
        string res = "stat-join,"+player.encode()+","+game.encode();
        xsend(fd, res);

    } else if (req.compare(0, 8, "ask-ship") == 0) {
        string arg[2];
        if (!split(arg, req)) return false;
        i32 id = I(arg[1]);

        for (auto const& [oid, ship] : game.players.data) {
            if (id == oid) continue;
            string res = "stat-ship,"+ship.encode();
            xsend(fd, res);
        }

    } else if (req.compare(0, 8, "ask-rock") == 0) {
        string arg[2];
        if (!split(arg, req)) return false;
        i32 id = I(arg[1]);
        //Player &player = game.players.data[id];

        for (auto const& [oid, rock] : game.rocks.data) {
            //if (dist(player.x, player.y, rock.x, rock.y) > 500*1000) continue;
            string res = "stat-rock,"+rock.encode();
            xsend(fd, res);
        }

    } else if (req.compare(0, 10, "ask-bullet") == 0) {
        string arg[2];
        if (!split(arg, req)) return false;
        i32 id = I(arg[1]);
        //Player &player = game.players.data[id];

        for (auto const& [oid, bullet] : game.bullets.data) {
            //if (dist(player.x, player.y, bullet.x, bullet.y) > 500*1000) continue;
            string res = "stat-bullet,"+bullet.encode();
            xsend(fd, res);
        }


    } else if (req.compare(0, 10, "ask-pellet") == 0) {
        string arg[2];
        if (!split(arg, req)) return false;
        i32 id = I(arg[1]);
        //Player &player = game.players.data[id];

        for (auto const& [oid, pellet] : game.pellets.data) {
            //if (dist(player.x, player.y, pellet.x, pellet.y) > 500) continue;
            string res = "stat-pellet,"+pellet.encode();
            xsend(fd, res);
        }

    } else if (req.compare(0, 9, "usr-coord") == 0) {

        string arg[5];
        if (!split(arg, req)) return false;

        Player &player = game.players.data[I(arg[1])];
        player.x = I(arg[2]);
        player.y = I(arg[3]);
        player.angle = I(arg[4]);

    } else if (req.compare(0, 9, "usr-fired") == 0) {

        string arg[5];
        if (!split(arg, req)) return false;

        game.spawn_bullet(I(arg[1]), I(arg[2]), I(arg[3]), I(arg[4]));

    //} else if (req.compare(0, 8, "hit-ship") == 0) {
    } else if (req.compare(0, 8, "hit-rock") == 0) {

        string arg[3];
        if (!split(arg, req)) return false;
        i32 pid = I(arg[1]);
        i32 oid = I(arg[2]);

        if (!game.players.contains(pid)) return true;
        if (!game.rocks.contains(oid)) return true;

        Player &player = game.players.data[pid];
        if (player.shield) return true;

        player.energy -= 1;
        if (player.energy <= 0) {
            game.spawn_pellets(player);
            game.players.remove(oid);

            string res = "del-ship,"+S(player.id);
            xsend(fd, res);
            cout << "[info] del-ship " << pid << endl;
        } else {
            player.enable_shield(SHIELD_ROCK_DECAY);
            cout << "[info] shield-from-rock " << oid << endl;
        }


    //} else if (req.compare(0, 10, "hit-bullet") == 0) {
    } else if (req.compare(0, 10, "hit-pellet") == 0) {

        string arg[3];
        if (!split(arg, req)) return false;
        i32 pid = I(arg[1]);
        i32 oid = I(arg[2]);

        if (!game.players.contains(pid)) return true;
        if (!game.pellets.contains(oid)) return true;

        Player &player = game.players.data[pid];
        Pellet &pellet = game.pellets.data[oid];
        if (pellet.type == 0) player.spice += pellet.value;
        if (pellet.type == 1) player.energy = clip(player.energy + pellet.value, 0, 5);
        game.pellets.remove(oid);
        cout << "[info] hit-pellet " << pid << " " << oid << endl;

    } else if (req.compare(0, 9, "shot-ship") == 0) {

        string arg[3];
        if (!split(arg, req)) return false;
        i32 pid = I(arg[1]);
        i32 oid = I(arg[2]);
        i32 bid = I(arg[3]);

        if (!game.players.contains(pid)) return true;
        if (!game.players.contains(oid)) return true;
        if (!game.bullets.contains(bid)) return true;
        if (pid == oid) return true;
        game.bullets.remove(bid);

        Player &ship = game.players.data[oid];
        if (!ship.shield) return true;

        cout << "[info] shot-ship " << pid << " " << oid << endl;
        if (ship.energy <= 0) {
            game.spawn_pellets(ship);
            game.players.remove(oid);

            string res = "del-ship,"+S(ship.id);
            xsend(fd, res);
            cout << "[info] del-ship " << ship.id << endl;
        } else {
            ship.enable_shield(SHIELD_SHIP_DECAY);
            ship.energy -= 1;
            cout << "[info] shield-from-ship " << ship.id << endl;
        }

    } else if (req.compare(0, 9, "shot-rock") == 0) {

        string arg[3];
        if (!split(arg, req)) return false;

        i32 pid = I(arg[1]);
        i32 oid = I(arg[2]);
        i32 bid = I(arg[3]);
        if (game.rocks.contains(oid)) {
            game.bullets.remove(bid);
            Rock &rock = game.rocks.data[oid];
            rock.health -= 1;
            cout << "[info] shot-rock " << rock.health << endl;
            if (rock.health <= 0) {
                string res = "del-rock,"+S(rock.id);
                xsend(fd, res);

                game.spawn_pellets(rock);
                game.rocks.remove(oid);
                cout << "[info] del-rock " << oid << endl;
            }
        }

    } else {
        return false;
    }

    return true;
}
