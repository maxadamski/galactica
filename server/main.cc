#include <unordered_set>
#include <thread>

#include "util.hh"
#include "nicenet.hh"

using namespace std;

template <size_t N>
int expect(string (&arr)[N], u32 n) {
    for (int i = 0; i < n; i++) {
        if (arr[i] == "") return false;
    }
    return true;
}

bool handle_request(i32 sfd, string &req);
struct Game;


// ----------------------------------------------------------------------------
// -- Global data
// ----------------------------------------------------------------------------

const i32 SHIELD_SHIP_DECAY = 0.5*1000;
const i32 SHIELD_ROCK_DECAY = 2*1000;
const i32 SHIELD_INIT_DECAY = 5*1000;

NicePoll nicepoll;
unordered_set<i32> clients;
map<i32, i32> client_player;
map<i32, thread> world_updates;
map<i32, i32> last_ping;

// ----------------------------------------------------------------------------
// -- Game engine
// ----------------------------------------------------------------------------


struct Player {
    i32 id, x, y, angle, spice, energy, shield, shield_time, shield_decay;

    string encode() const {
        return S(id)+","+S(x)+","+S(y)+","+S(angle)+","+S(spice)+","+S(energy)+","+S(shield);
    }

    void enable_shield(i32 decay) {
        shield = true;
        shield_time = 0;
        shield_decay = decay;
    }

    void update_shield(i32 dt) {
        if (!shield) return;
        shield_time += dt;
        if (shield_time > shield_decay) {
            shield = false;
        }
    }
};

struct Rock {
    i32 id, x, y, angle, speed, size, health;

    string encode() const {
        return S(id)+","+S(x)+","+S(y)+","+S(angle)+","+S(speed)+","+S(size)+","+S(health);
    }
};

struct Bullet {
    i32 id, pid, x, y, angle, time;

    string encode() const {
        return S(id)+","+S(pid)+","+S(x)+","+S(y)+","+S(angle)+","+S(time);
    }
};

struct Pellet {
    i32 id, x, y, value, type;

    string encode() const {
        return S(id)+","+S(x)+","+S(y)+","+S(value)+","+S(type);
    }
};

struct Game {
    Table<Player> players;
    Table<Bullet> bullets;
    Table<Pellet> pellets;
    Table<Rock> rocks;

    bool finished = false;
    bool reset = false;
    i32 map_size = 1000;
    i32 rock_count = map_size / 50;
    i32 max_energy = 3;
    i32 init_angle = -PI/2*1000;
    i32 until_stop = 5*60*1000;
    i32 until_reset = 0;
    i32 until_reset_max = 30*1000;
    i32 bullet_decay = 1000*1;
    i32 bullet_speed = 0.3*1000;

    uniform_int_distribution<> angle_dist{0, (i32)TAU*1000};
    uniform_int_distribution<> coord_dist{0, map_size*1000};

    normal_distribution<> rock_speed_dist{5, 2};
    normal_distribution<> rock_size_dist{60, 10};
    uniform_int_distribution<> unit_dist{0, 1000};

    string encode() const {
        return S(map_size)+","+S(until_reset)+","+S(until_stop)+","+S(finished);
    }

    void init() {
        cout << "new game\n";
        for (i32 i = 0; i < rock_count; i++) {
            spawn_rock();
        }
    }


    void step(float dt) {
        if (reset) {
            return;
        } else if (finished) {
            until_reset -= dt;
            if (until_reset < 0) {
                reset = true;
                return;
            }
        } else {
            until_stop -= dt;
            if (until_stop < 0) {
                finished = true;
                until_reset = until_reset_max;
                return;
            }
        }

        vector<i32> del_rocks, del_bullets, del_pellets, del_ships;

        for (auto const& [id, it] : players.data) {
            Player &player = players.data[id];
            player.update_shield(dt);

            if (player.shield) continue;

            for (auto const& [obj_id, obj] : rocks.data) {
                if (dist(it.x, it.y, obj.x, obj.y) < obj.size * 1000 / 2) {
                    player.energy -= 1;
                    if (player.energy <= 0) {
                        for (i32 client : clients) xsend(client, "del-ship,"+S(id));
                        spawn_pellets(player);
                        del_ships.push_back(id);
                    } else {
                        player.enable_shield(SHIELD_ROCK_DECAY);
                    }
                    break;
                }
            }

            for (auto const& [obj_id, obj] : bullets.data) {
                if (id == obj.pid) continue;
                if (dist(it.x, it.y, obj.x, obj.y) < 3*1000) {
                    del_bullets.push_back(obj_id);
                    player.energy -= 1;
                    if (player.energy <= 0) {
                        for (i32 client : clients) xsend(client, "del-ship,"+S(id));
                        spawn_pellets(player);
                        del_ships.push_back(id);
                    } else {
                        player.enable_shield(SHIELD_SHIP_DECAY);
                    }
                    break;
                }
            }

            for (auto const& [obj_id, obj] : pellets.data) {
                if (dist(it.x, it.y, obj.x, obj.y) < 5*1000) {
                    del_pellets.push_back(obj_id);
                    if (obj.type == 1)
                        player.energy += obj.value;
                    else
                        player.spice += obj.value;

                    for (i32 client : clients) xsend(client, "del-pellet,"+S(obj_id));
                }
            }
        }

        for (auto const& [id, it] : rocks.data) {
            Rock &rock = rocks.data[id];
            rock.x += dt * it.speed * cos(it.angle / 1000.0);
            rock.y += dt * it.speed * sin(it.angle / 1000.0);

            for (auto const& [obj_id, obj] : bullets.data) {
                if (dist(it.x, it.y, obj.x, obj.y) < it.size * 1000 / 2) {
                    rock.health -= 1;
                    for (i32 client : clients) xsend(client, "del-bullet,"+S(obj_id));
                    del_bullets.push_back(obj_id);
                }
            }

            bool oob = rock.x < 0 || rock.y < 0 || rock.x > map_size*1000 || rock.y > map_size*1000;
            if (oob || rock.health <= 0) {
                spawn_pellets(rock);
                for (i32 client : clients) xsend(client, "del-rock,"+S(id));
                del_rocks.push_back(id);
            }
        }

        for (auto const& [id, it] : bullets.data) {
            Bullet &bullet = bullets.data[id];
            bullet.x += dt * bullet_speed * cos(bullet.angle / 1000.0);
            bullet.y += dt * bullet_speed * sin(bullet.angle / 1000.0);
            bullet.time += dt;
            bool oob = bullet.x < 0 || bullet.y < 0 || bullet.x > map_size*1000 || bullet.y > map_size*1000;
            bool timeout = bullet.time > bullet_decay;
            if (oob || timeout) {
                for (i32 client : clients) xsend(client, "del-bullet,"+S(id));
                del_bullets.push_back(id);
            }
        }

        pellets.remove(del_pellets);
        bullets.remove(del_bullets);
        rocks.remove(del_rocks);
        players.remove(del_ships);
    }

    Rock &spawn_rock() {
        int x = coord_dist(gen);
        int y = coord_dist(gen);
        int angle = angle_dist(gen);
        int size = random_normal(60, 10);
        int speed = random_normal(5, 2);
        Rock rock{-1, x, y, angle, speed, size, 3};
        i32 id = rocks.append(rock);
        rocks.data[id].id = id;
        return rocks.data[id];
    }

    Player &spawn_player() {
        int x = coord_dist(gen);
        int y = coord_dist(gen);
        Player player{-1, x, y, init_angle, 0, max_energy, false, 0, 0};
        i32 id = players.append(player);
        players.data[id].id = id;
        cout << "[info] spawn player " << players.data[id].id << endl;
        return players.data[id];
    }

    Bullet &spawn_bullet(i32 pid, i32 x, i32 y, i32 angle) {
        Bullet bullet{-1, pid, x, y, angle, 0};
        i32 id = bullets.append(bullet);
        bullets.data[id].id = id;
        return bullets.data[id];
    }

    void spawn_pellets(Rock &rock) {
        i32 spiceCount = 6;

        for (i32 i = 0; i < spiceCount; i++) {
            i32 x = random_normal(rock.x, 10*1000);
            i32 y = random_normal(rock.y, 10*1000);
            Pellet pellet{-1, x, y, 1, 0};
            i32 id = pellets.append(pellet);
            pellets.data[id].id = id;
        }
    }

    void spawn_pellets(Player &player) {
        i32 energyCount = 2;
        i32 spiceCount = 3;

        for (i32 i = 0; i < spiceCount; i++) {
            i32 x = random_normal(player.x, 10*1000);
            i32 y = random_normal(player.y, 10*1000);
            Pellet pellet{-1, x, y, player.spice / spiceCount, 0};
            i32 id = pellets.append(pellet);
            pellets.data[id].id = id;
        }

        for (i32 i = 0; i < energyCount; i++) {
            i32 x = random_normal(player.x, 10*1000);
            i32 y = random_normal(player.y, 10*1000);
            Pellet pellet{-1, x, y, 1, 1};
            i32 id = pellets.append(pellet);
            pellets.data[id].id = id;
        }
    }

};


// ----------------------------------------------------------------------------
// -- Event handlers
// ----------------------------------------------------------------------------

Game game;

void update_game() {
    auto t1 = now();
    while (true) {
        if (game.reset) {
            game = Game();
            game.init();
        }

        game.step(millis(now() - t1));
        t1 = now();
        this_thread::sleep_for(chrono::milliseconds(10));
    }
}

void send_world_updates(i32 fd) {
    i32 my_id = client_player[fd];
    Player &player = game.players.data[my_id];
    while (true) {
        xsend(fd, "stat-ship,"+player.encode());
        xsend(fd, "stat-game,"+game.encode());

        for (auto const& [id, obj] : game.players.data) {
            if (dist(player.x, player.y, obj.x, obj.y) > 500*1000) continue;
            if (my_id == id) continue;
            xsend(fd, "stat-ship,"+obj.encode());
        }

        for (auto const& [id, obj] : game.bullets.data) {
            if (dist(player.x, player.y, obj.x, obj.y) > 500*1000) continue;
            xsend(fd, "stat-bullet,"+obj.encode());
        }

        for (auto const& [id, obj] : game.rocks.data) {
            if (dist(player.x, player.y, obj.x, obj.y) > 500*1000) continue;
            xsend(fd, "stat-rock,"+obj.encode());
        }

        for (auto const& [id, obj] : game.pellets.data) {
            if (dist(player.x, player.y, obj.x, obj.y) > 500*1000) continue;
            xsend(fd, "stat-pellet,"+obj.encode());
        }

        this_thread::sleep_for(chrono::milliseconds(30));
    }
}

bool handle_request(i32 fd, string &req) {
    if (req.compare(0, 4, "ping") == 0) {
        xsend(fd, "pong");

    } else if (req.compare(0, 4, "join") == 0) {
        cout << req << endl;
        if (game.finished) return true;
        if (client_player.find(fd) != client_player.end()) return true;

        auto player = game.spawn_player();
        client_player[fd] = player.id;
        world_updates[fd] = thread(send_world_updates, fd);
        xsend(fd, "join,"+player.encode()+","+game.encode());

    } else if (req.compare(0, 9, "usr-coord") == 0) {
        string arg[5];
        if (!split(arg, req)) return false;
        Player &player = game.players.data[I(arg[1])];
        player.x = I(arg[2]);
        player.y = I(arg[3]);
        player.angle = I(arg[4]);

    } else if (req.compare(0, 9, "usr-fired") == 0) {
        cout << req << endl;
        string arg[5];
        if (!split(arg, req)) return false;
        game.spawn_bullet(I(arg[1]), I(arg[2]), I(arg[3]), I(arg[4]));

    } else {
        return false;
    }

    return true;
}


void handle_client(i32 fd, u32 events) {
    if (events & EPOLLIN) {
        string req = xrecv(fd);
        //cout << "[info] request - " << req << endl;
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


// ----------------------------------------------------------------------------
// -- Entry point
// ----------------------------------------------------------------------------

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

    thread thread1(update_game);

    while (true) {
        i32 event_count = nicepoll.wait(events, max_events, -1);
        for (i32 i = 0; i < event_count; i++) {
            nicepoll.handle(events[i]);
        }
    }
}

