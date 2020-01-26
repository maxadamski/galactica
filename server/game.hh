#pragma once

#include "util.hh"
#include "nicenet.hh"

const i32 SHIELD_SHIP_DECAY = 0.5*1000;
const i32 SHIELD_ROCK_DECAY = 2*1000;
const i32 SHIELD_INIT_DECAY = 5*1000;

map<i32, i32> client_player;
map<i32, thread> world_updates;
map<i32, i32> last_ping;

struct Player {
    i32 id, x, y, angle, spice, energy, shield, shield_time, shield_decay;

    inline string encode() const {
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

    inline string encode() const {
        return S(id)+","+S(x)+","+S(y)+","+S(angle)+","+S(speed)+","+S(size)+","+S(health);
    }
};

struct Bullet {
    i32 id, pid, x, y, angle, time;

    inline string encode() const {
        return S(id)+","+S(pid)+","+S(x)+","+S(y)+","+S(angle)+","+S(time);
    }
};

struct Pellet {
    i32 id, x, y, value, type;

    inline string encode() const {
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

    inline string encode() const {
        return S(map_size)+","+S(until_reset)+","+S(until_stop)+","+S(finished);
    }

    inline void init() {
        cout << "new game\n";
        for (i32 i = 0; i < rock_count; i++) {
            spawn_rock();
        }
    }


    inline void step(float dt) {
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

        vector<i32> del_rocks, del_bullets;

        for (auto const& [id, it] : players.data) {
            Player &player = players.data[id];
            player.update_shield(dt);


            if (player.shield) continue;

            for (auto const& [obj_id, obj] : rocks.data) {
                if (dist(it.x, it.y, obj.x, obj.y) < obj.size) {
                    player.energy -= 1;
                    if (player.energy <= 0) {
                        spawn_pellets(player);
                    } else {
                        player.enable_shield(SHIELD_ROCK_DECAY);
                    }
                    break;
                }
            }

            for (auto const& [obj_id, obj] : bullets.data) {
                if (id == obj.pid) continue;
                if (dist(it.x, it.y, obj.x, obj.y)/1000.0 < 2) {
                    del_bullets.push_back(obj_id);
                    player.energy -= 1;
                    if (player.energy <= 0) {
                        spawn_pellets(player);
                    } else {
                        player.enable_shield(SHIELD_SHIP_DECAY);
                    }
                    break;
                }
            }
        }

        for (auto const& [id, it] : rocks.data) {
            Rock &rock = rocks.data[id];
            rock.x += dt * it.speed * cos(it.angle / 1000.0);
            rock.y += dt * it.speed * sin(it.angle / 1000.0);
            bool oob = rock.x < 0 || rock.y < 0 || rock.x > map_size*1000 || rock.y > map_size*1000;
            if (oob || rock.health <= 0) del_rocks.push_back(id);
        }

        for (auto const& [id, it] : bullets.data) {
            Bullet &bullet = bullets.data[id];
            bullet.x += dt * bullet_speed * cos(bullet.angle / 1000.0);
            bullet.y += dt * bullet_speed * sin(bullet.angle / 1000.0);
            bullet.time += dt;
            bool oob = bullet.x < 0 || bullet.y < 0 || bullet.x > map_size*1000 || bullet.y > map_size*1000;
            bool timeout = bullet.time > bullet_decay;
            if (oob || timeout) del_bullets.push_back(id);
        }

        bullets.remove(del_bullets);
        rocks.remove(del_rocks);
    }

    inline Rock &spawn_rock() {
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

    inline Player &spawn_player() {
        int x = coord_dist(gen);
        int y = coord_dist(gen);
        Player player{-1, x, y, init_angle, 0, max_energy, false, 0, 0};
        i32 id = players.append(player);
        players.data[id].id = id;
        cout << "[info] spawn player " << players.data[id].id << endl;
        return players.data[id];
    }

    inline Bullet &spawn_bullet(i32 pid, i32 x, i32 y, i32 angle) {
        Bullet bullet{-1, pid, x, y, angle, 0};
        i32 id = bullets.append(bullet);
        bullets.data[id].id = id;
        return bullets.data[id];
    }

    inline void spawn_pellets(Rock &rock) {
        i32 spiceCount = 6;

        for (i32 i = 0; i < spiceCount; i++) {
            i32 x = random_normal(rock.x, 10*1000);
            i32 y = random_normal(rock.y, 10*1000);
            Pellet pellet{-1, x, y, 1, 0};
            i32 id = pellets.append(pellet);
            pellets.data[id].id = id;
        }
    }

    inline void spawn_pellets(Player &player) {
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
