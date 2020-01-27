#include <unordered_set>
#include <thread>
#include <string_view>

#include "game.hh"

using namespace std;

bool handle_request(i32 sfd, string &req);

// ----------------------------------------------------------------------------
// -- Global data
// ----------------------------------------------------------------------------

i32 port = 6666;

Game game;

const i32 SHIELD_SHIP_DECAY = 0.5*1000;
const i32 SHIELD_ROCK_DECAY = 2*1000;
const i32 SHIELD_INIT_DECAY = 5*1000;

NicePoll nicepoll;
unordered_set<i32> clients;
map<i32, u64> last_ping;
map<i32, i32> client_player;
map<i32, i32> player_client;
map<i32, thread> world_updates;


// ----------------------------------------------------------------------------
// -- Outbox
// ----------------------------------------------------------------------------

string encode_pellets(vector<Pellet> &objs) {
    string msg = "";
    for (Pellet &obj : objs) msg += ";stat-pellet,"+obj.encode();
    return msg;
}

void xcast(string message) {
    //printf("[info] xcast %s\n", message.c_str());
    for (i32 fd : clients) xsend(fd, message);
}

void cast_bullet(Bullet &bullet) {
    xcast("stat-bullet,"+bullet.encode());
}

void cast_game_updates(Game &game) {
    xcast("stat-game,"+game.encode());
}

void cast_del_bullet(i32 id) {
    xcast("del-bullet,"+S(id));
}

void cast_del_pellet(i32 id) {
    xcast("del-pellet,"+S(id));
}

void cast_del_ship(i32 id) {
    xcast("del-ship,"+S(id));
}

void cast_del_rock(i32 id) {
    xcast("del-rock,"+S(id));
}

void send_pellets(i32 fd, vector<Pellet> objs) {
    xsend(fd, encode_pellets(objs));
}

void cast_pellets(vector<Pellet> objs) {
    xcast(encode_pellets(objs));
}

void send_bullet(i32 fd, Bullet &bullet) {
    xsend(fd, "stat-bullet,"+bullet.encode());
}

void send_rock(i32 fd, Rock &rock) {
    xsend(fd, "stat-rock,"+rock.encode());
}

void send_all_particles(i32 fd, Game &game) {
    string res = "pong";
    for (auto const& [id, obj] : game.rocks.data)
        res += ";stat-rock,"+obj.encode();
    for (auto const& [id, obj] : game.bullets.data)
        res += ";stat-bullet,"+obj.encode();
    for (auto const& [id, obj] : game.pellets.data)
        res += ";stat-pellet,"+obj.encode();
    xsend(fd, res);
}

void send_world_updates(i32 fd, Player &player, Game &game) {
    //xsend(fd, "stat-ship,"+player.encode());
    string res;
    for (auto const& [id, obj] : game.players.data) {
        //if (obj.game_over) continue;
        //if (dist(player.x, player.y, obj.x, obj.y) > 500*1000) continue;
        //if (player.id == id) continue;
        if (!res.empty()) res += ";";
        res += "stat-ship,"+obj.encode();
    }
    xsend(fd, res);
}


// ----------------------------------------------------------------------------
// -- Game engine
// ----------------------------------------------------------------------------

void Player::enable_shield(i32 decay) {
    shield = true;
    shield_time = 0;
    shield_decay = decay;
}

void Player::update_shield(i32 dt) {
    if (!shield) return;
    shield_time += dt;
    if (shield_time > shield_decay) {
        shield = false;
    }
}

void Game::init() {
    printf("[info] new game\n");
    for (i32 i = 0; i < rock_count; i++) {
        spawn_rock();
    }
}

void Game::terminate_player(Player &player) {
    xcast("del-ship,"+S(player.id));
    player.game_over = true;
    spawn_pellets(player);
}

void Game::did_hit_rock(Player &player) {
    player.energy -= 1;
    if (player.energy <= 0) {
        terminate_player(player);
    } else {
        player.enable_shield(SHIELD_ROCK_DECAY);
    }
}

void Game::did_hit_bullet(Player &player, const Bullet &obj) {
    player.energy -= 1;
    if (player.energy <= 0) {
        terminate_player(player);
    } else {
        player.enable_shield(SHIELD_SHIP_DECAY);
    }
}

void Game::did_hit_pellet(Player &player, const Pellet &obj) {
    xcast("del-pellet,"+S(obj.id));
    if (obj.type == 1)
        player.energy += obj.value;
    else
        player.spice += obj.value;
}

void Game::step(float dt) {
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
            printf("[info] end of round\n");
            finished = true;
            until_reset = until_reset_max;
            return;
        }
    }

    unordered_set<i32> del_rocks, del_bullets, del_pellets;

    for (auto& [id, player] : players.data) {
        if (player.game_over) continue;
        player.update(dt);

        for (auto const& [obj_id, obj] : pellets.data) {
            if (dist(player.x, player.y, obj.x, obj.y) < 8*1000) {
                did_hit_pellet(player, obj);
                del_pellets.insert(obj_id);
            }
        }

        if (player.shield) continue;

        for (auto const& [rock_id, rock] : rocks.data) {
            if (dist(player.x, player.y, rock.x, rock.y) < rock.size * 1000 / 2) {
                did_hit_rock(player);
                break;
            }
        }
    }

    for (auto& [rock_id, rock] : rocks.data) {
        rock.update(dt);

        bool oob = rock.x < 0 || rock.y < 0 || rock.x > map_size*1000 || rock.y > map_size*1000;
        if (oob) {
            //del_rocks.insert(rock_id);
        }
    }

    for (auto& [bullet_id, bullet] : bullets.data) {
        bullet.update(dt);
        bool oob = bullet.x < 0 || bullet.y < 0 || bullet.x > map_size*1000 || bullet.y > map_size*1000;
        bool timeout = bullet.time > bullet_decay;
        if (oob || timeout) {
            del_bullets.insert(bullet_id);
        }

        /*
        // check if bullet collides with any player
        for (auto &[player_id, player] : players.data) {
            if (player.game_over || player.shield || player_id == bullet.pid) continue;

            if (dist(player.x, player.y, bullet.x, bullet.y) < 6*1000) {
                did_hit_bullet(player, bullet);
                del_bullets.insert(bullet_id);
                goto after_checks;
            }
        }
        */

        // check if bullet collides with any rock
        for (auto &[rock_id, rock] : rocks.data) {
            if (rock.disable) continue;
            if (dist(rock.x, rock.y, bullet.x, bullet.y) < rock.size * 1000 / 2) {
                rock.health -= 1;
                if (rock.health <= 0) {
                    rock.disable = true;
                    del_rocks.insert(rock_id);
                    spawn_pellets(rock);
                }
                del_bullets.insert(bullet_id);
            }
        }
    }


    for (i32 id : del_rocks) cast_del_rock(id);
    for (i32 id : del_bullets) cast_del_bullet(id);
    bullets.remove(del_bullets);
    pellets.remove(del_pellets);
    rocks.remove(del_rocks);
}

Rock &Game::spawn_rock() {
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

Player &Game::spawn_player() {
    int x = coord_dist(gen);
    int y = coord_dist(gen);
    Player player{-1, x, y, init_angle, 0, max_energy, false, 0, 0};
    player.enable_shield(SHIELD_INIT_DECAY);
    i32 id = players.append(player);
    players.data[id].id = id;
    cout << "[info] spawn player " << players.data[id].id << endl;
    return players.data[id];
}

Bullet &Game::spawn_bullet(i32 pid, i32 x, i32 y, i32 angle) {
    Bullet bullet{-1, pid, x, y, angle, 0};
    i32 id = bullets.append(bullet);
    bullets.data[id].id = id;
    return bullets.data[id];
}

void Game::spawn_pellets(Rock &rock) {
    i32 spiceCount = 6;

    vector<Pellet> newPellets;
    for (i32 i = 0; i < spiceCount; i++) {
        i32 x = random_normal(rock.x, 10*1000);
        i32 y = random_normal(rock.y, 10*1000);
        Pellet pellet{-1, x, y, 1, 0};
        i32 id = pellets.append(pellet);
        pellets.data[id].id = id;
        newPellets.push_back(pellets.data[id]);
    }

    cast_pellets(newPellets);
}

void Game::spawn_pellets(Player &player) {
    i32 energyCount = 2;
    i32 spiceCount = 3;

    vector<Pellet> newPellets;
    if (player.spice >= spiceCount) {
        for (i32 i = 0; i < spiceCount; i++) {
            i32 x = random_normal(player.x, 10*1000);
            i32 y = random_normal(player.y, 10*1000);
            Pellet pellet{-1, x, y, player.spice / spiceCount, 0};
            i32 id = pellets.append(pellet);
            pellets.data[id].id = id;
            newPellets.push_back(pellets.data[id]);
        }
    }

    for (i32 i = 0; i < energyCount; i++) {
        i32 x = random_normal(player.x, 10*1000);
        i32 y = random_normal(player.y, 10*1000);
        Pellet pellet{-1, x, y, 1, 1};
        i32 id = pellets.append(pellet);
        pellets.data[id].id = id;
        newPellets.push_back(pellets.data[id]);
    }

    cast_pellets(newPellets);
}


// ----------------------------------------------------------------------------
// -- Threads and events
// ----------------------------------------------------------------------------

#define contains(x, y) (x.find(y) != x.end())

void remove_client(i32 fd) {
    printf("[info] removing client %d\n", fd);
    nicepoll.erase(fd);
    clients.erase(fd);
    last_ping.erase(fd);
    if (contains(world_updates, fd)) {
        world_updates[fd].join();
        world_updates.erase(fd);
    }
    if (contains(client_player, fd)) {
        Player &player = game.players.data[client_player[fd]];
        xcast("log-left,"+player.nick);
        game.terminate_player(player);
        client_player.erase(fd);
    }
    if (contains(player_client, fd)) player_client.erase(fd);
}

void prune_clients() {
    vector<i32> to_remove;
    for (auto const& [fd, time] : last_ping) {
        if (millis() - time > 5*1000)
            to_remove.push_back(fd);
    }
    for (i32 fd : to_remove) {
        remove_client(fd);
    }
}

void thread_cast_game() {
    while (true) {
        cast_game_updates(game);
        this_thread::sleep_for(chrono::milliseconds(50));
    }
}

void thread_step_game() {
    auto t1 = now();
    while (true) {
        if (game.reset) {
            game = Game();
            game.init();
        }

        game.step(millis(now() - t1));
        t1 = now();
        this_thread::sleep_for(chrono::milliseconds(16));
    }
}

void thread_send_world_high(i32 fd) {
    i32 my_id = client_player[fd];
    Player &player = game.players.data[my_id];
    auto last = millis();
    while (contains(clients, fd)) {
        send_world_updates(fd, player, game);
        if (millis() - last > 1000) {
            send_all_particles(fd, game);
            last = millis();
        }
        this_thread::sleep_for(chrono::milliseconds(30));
    }
}

void thread_send_world_low(i32 fd) {
    while (true) {
        send_all_particles(fd, game);
        this_thread::sleep_for(chrono::milliseconds(500));
    }
}

void thread_prune_clients() {
    while (true) {
        prune_clients();
        this_thread::sleep_for(chrono::milliseconds(500));
    }
}

bool handle_request(i32 fd, string &req) {
    if (req.compare(0, 4, "ping") == 0) {
        xsend(fd, "pong");

    } else if (req.compare(0, 4, "conn") == 0) {
        if (client_player.find(fd) != client_player.end()) return true;
        printf("[info] connect %d\n", fd);
        world_updates[fd] = thread(thread_send_world_high, fd);

    } else if (req.compare(0, 4, "join") == 0) {
        if (game.finished) return true;
        auto arg = split(2, req, ",");
        auto nick = arg[1];
        printf("[info] join %s %d\n", nick.c_str(), fd);
        Player &player = game.spawn_player();
        player.nick = nick;
        client_player[fd] = player.id;
        player_client[player.id] = fd;
        send_all_particles(fd, game);
        xcast("log-join,"+nick);
        xsend(fd, "join,"+player.encode()+","+game.encode());

    } else if (req.compare(0, 9, "usr-coord") == 0) {
        auto arg = split(5, req, ",");
        Player &player = game.players.data[I(arg[1])];
        player.x = I(arg[2]);
        player.y = I(arg[3]);
        player.angle = I(arg[4]);

    } else if (req.compare(0, 9, "usr-fired") == 0) {
        auto arg = split(5, req, ",");
        Bullet &bullet = game.spawn_bullet(I(arg[1]), I(arg[2]), I(arg[3]), I(arg[4]));
        cast_bullet(bullet);

    } else {
        return false;

    }

    return true;
}

void handle_client(i32 fd, u32 events) {
    if (events & EPOLLIN) {
        last_ping[fd] = millis();
        vector<string> reqs = xrecv(fd);
        for (string req : reqs) {
            try {
                handle_request(fd, req);
            } catch (const char*e) {
                printf("[warn] exception during request - %s\n", req.c_str());
            }
        }
    }
    if (events & ~EPOLLIN) {
        remove_client(fd);
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
        last_ping[client] = millis();
        nicepoll.insert(client, EPOLLIN | EPOLLRDHUP, &handle_client);
    }
}


// ----------------------------------------------------------------------------
// -- Entry point
// ----------------------------------------------------------------------------

void parse_args(int argc, char **argv) {
    if (argc < 1) {
        printf("usage: %s -p PORT\n", argv[0]);
        printf("OPTIONS\n");
        printf("  -p --port    PORT\n");
        printf("  --map-size   INT\n");
        printf("  --game-time  MILLIS\n");
        printf("  --reset-time MILLIS\n");
        exit(1);
    }

    for (i32 i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i],"-p")==0 || strcmp(argv[i],"--port")==0) {
            port = atoi(argv[++i]);

        } else if (strcmp(argv[i],"--game-time")==0) {
            game_time = atoi(argv[++i]);

        } else if (strcmp(argv[i],"--reset-time")==0) {
            reset_time = atoi(argv[i++]);

        } else if (strcmp(argv[i],"--map-size")==0) {
            map_size = atoi(argv[i++]);
        }
    }
}

int main(int argc, char **argv) {
    const i32 max_pending = 32;
    const u32 max_events = 8;

    parse_args(argc, argv);
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

    printf("[info] listening on port %d\n", port);
    if (nicepoll.create() < 0)
        fatal("could not create epoll descriptor");

    nicepoll.insert(server, EPOLLIN, &handle_server);

    epoll_event events[max_events];

    game.init();

    //thread thread1(thread_prune_clients);
    thread thread2(thread_step_game);
    //thread thread3(thread_cast_game);

    while (true) {
        prune_clients();
        cast_game_updates(game);
        i32 event_count = nicepoll.wait(events, 2, -1);
        for (i32 i = 0; i < event_count; i++) {
            nicepoll.handle(events[i]);
        }
    }
}

