#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 

#include <cstring>
#include <cmath>

#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <iterator>

using namespace std;

const int ZERO = 0;
const int ONE = 1;

struct Player { int id, x, y, angle, energy, spice, shield; };
struct Rock { int id, x, y, angle, speed, size, health; };
struct Bullet { int id, x, y, dx, dy; };
struct Spice { int id, x, y, value; };
struct Energy { int id ,x, y, value; };

map<int, Player> players;
map<int, Spice> spices;
map<int, Energy> energies;
map<int, Rock> rocks;

template <size_t N>
void split(string (&arr)[N], string str) {
	char *token = strtok((char*)str.c_str(), " ,\n");
	for (int i = 0; token != NULL; i++) {
		arr[i] = string(token);
		token = strtok(NULL, " ,\n");
	}
}

#include <random>

random_device rd{};
mt19937 gen{rd()};

int map_size = 3000;
int rock_count = 100;
int max_energy = 10;

normal_distribution<> rock_speed_dist{5, 2};
normal_distribution<> rock_size_dist{60, 10};
uniform_int_distribution<> coord_dist{0, 3000*1000};
uniform_int_distribution<> angle_dist{0, 6283};
uniform_int_distribution<> unit_dist{0, 1000};

int rock_id = 0;
int bullet_id = 0;
int player_id = 0;

void update_world(float dt) {
	for (auto const& pair : rocks) {
		auto id = pair.first;
		auto rock = pair.second;
		rocks[id].x += dt * rock.speed * cos(rock.angle / 1000.0);
		rocks[id].y += dt * rock.speed * sin(rock.angle / 1000.0);
	}
}

Rock spawn_rock() {
	int x = coord_dist(gen);
	int y = coord_dist(gen);
	int size = rock_size_dist(gen);
	int speed = rock_speed_dist(gen);
	int angle = angle_dist(gen);
	Rock rock{rock_id, x, y, angle, speed, size, size / 10};
	rocks[rock_id++] = rock;
	return rock;
}

Player spawn_player() {
	int x = coord_dist(gen);
	int y = coord_dist(gen);
	Player player{player_id, x, y, 0, max_energy, 0, 0};
	players[player_id++] = player;
	return player;
}

#define S(x) to_string(x)
#include <chrono>

using namespace std::chrono;


int main(int argc, char **argv) {

	for (int i = 0; i < 100; i++) {
		Rock rock = spawn_rock();
	}

	int port = 8080;
	int sfd = socket(AF_INET, SOCK_DGRAM, 0);
	sockaddr_in server_addr{AF_INET, htons(port), INADDR_ANY};
	setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &ONE, sizeof(ONE));
	bind(sfd, (sockaddr*) &server_addr, sizeof(server_addr));


	sockaddr_in client_addr{};
	socklen_t client_len = sizeof(client_addr);
	const size_t buf_max = 256;
	char buf[buf_max] = {0};

	auto t1 = high_resolution_clock::now();
	while (true) {
		auto t2 = high_resolution_clock::now();
		duration<double, std::milli> dt = t2 - t1;
		update_world(dt.count());
		t1 = t2;

		size_t recv_len = recvfrom(sfd, buf, buf_max, 0, (sockaddr*) &client_addr, &client_len);

		string msg = string(buf, buf+recv_len);

		if (msg.compare(0, 4, "join") == 0) {

			Player x = spawn_player();
			string res = "join-ok,"+S(x.id)+","+S(x.x)+","+S(x.y)+","+S(x.angle)+","+S(x.spice)+","+S(x.energy)+","+S(x.shield);
			sendto(sfd, res.c_str(), res.size(), 0, (sockaddr*) &client_addr, client_len);

		} else if (msg.compare(0, 4, "rock") == 0) {

			for (auto const& pair : rocks) {
				Rock x = pair.second;
				string res = "rock-ok,"+S(x.id)+","+S(x.x)+","+S(x.y)+","+S(x.angle)+","+S(x.speed)+","+S(x.size)+","+S(x.health);
				sendto(sfd, res.c_str(), res.size(), 0, (sockaddr*) &client_addr, client_len);
			}

		} else if (msg.compare(0, 3, "pos") == 0) {
			/*
			string data[5];
			split(data, msg);
			int id = stoi(data[1]);
			players[id].x = stoi(data[2]);
			players[id].y = stoi(data[3]);
			players[id].angle = stoi(data[4]);
			*/

		} else if (msg.compare(0, 3, "bul") == 0) {
			/*
			string data[6];
			split(data, msg);
			int id = stoi(data[1]);
			cout << "bul " << id << " " << data[2] << " " << data[3] << " " << data[4] << " " << data[5] << endl;
			players[id].x = stoi(data[2]);
			players[id].y = stoi(data[3]);
			players[id].angle = stoi(data[4]);
			*/

		} else {
			cout << "unknown message\n";
			cout << msg;
		}

	}
}
