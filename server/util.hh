#pragma once

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

#include <string>
#include <vector>
#include <stack>
#include <map>

#include <memory>
#include <algorithm>
#include <random>
#include <chrono>
#include <iterator>


//
// Shorthands
//

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::vector;
using std::stack;
using std::map;
using std::shared_ptr;
using std::weak_ptr;

using namespace std;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float  f32;
typedef double f64;

const int ZERO = 0;
const int ONE = 1;


//
// Input-Output
//

#define DEBUG

// Removing commas with '##' only works in GCC!
#define eprintf(f_, ...) fprintf(stderr, (f_), ##__VA_ARGS__)

#ifdef DEBUG
#define dprintf(f_, ...) fprintf(stderr, (f_), ##__VA_ARGS__)
#else
#define dprintf(f_, ...)
#endif


inline void fatal(const char *message) {
	fprintf(stderr, "fatal error: %s\n", message);
	exit(1);
}


//
// Math
//

#define PI 3.14159f
#define TAU (2*PI)

#define sizeof_val(X) (sizeof(X[0]))
#define sizeof_vec(X) (sizeof(X[0]) * X.size())


inline f64 clip(f64 value, f64 min, f64 max) {
    value = value < min ? min : value;
    value = value > max ? max : value;
    return value;
}

inline i32 clip(i32 value, i32 min, i32 max) {
    value = value < min ? min : value;
    value = value > max ? max : value;
    return value;
}

inline f64 rad(f64 deg) {
    return deg * PI / 180.0;
}

inline f64 distsq(f32 x1, f32 y1, f32 x2, f32 y2) {
    f32 dx = x2 - x1;
    f32 dy = y2 - y1;
    return dx*dx + dy*dy;
}

inline f64 dist(f32 x1, f32 y1, f32 x2, f32 y2) {
    return sqrt(distsq(x1, y1, x2, y2));
}


// 
// Random
// 

std::random_device rd{};
std::mt19937 gen{rd()};

inline f32 random_normal(f32 mean, f32 std) {
    std::normal_distribution<> dist{mean, std};
    return dist(gen);
}

inline i32 random_uniform(i32 lower, i32 upper) {
    std::uniform_int_distribution<> dist{lower, upper};
    return dist(gen);
}


//
// Time
//

inline auto now() {
    return chrono::high_resolution_clock::now();
}

template <class T>
inline i32 millis(T delta) {
    return chrono::duration_cast<chrono::milliseconds>(delta).count();
}


//
// String
//

#define S(x) to_string(x)
#define I(x) (((x[0] >= '0' && x[0] <= '9') || x[0] == '-') ? stoi(x) : (throw "bad request"))

template <size_t N>
bool split(string (&arr)[N], string str) {
    char *token = strtok((char*)str.c_str(), " ,\n");
    for (int i = 0; token != NULL; i++) {
        arr[i] = string(token);
        if (arr[i] == "") return false;
        token = strtok(NULL, " ,\n");
    }
    return true;
}


//
// Data structures
//

template <class Item>
struct Table {
    map<i32, Item> data;
    stack<i32> free_id;
    i32 next_id = 0;

    inline i32 new_id() {
        //if (free_id.empty()) {
            i32 id = next_id;
            next_id += 1;
            return id;
        //} else {
        //    i32 id = free_id.top();
        //    free_id.pop();
        //    return id;
        //}
    }

    inline i32 append(Item item) {
        i32 id = new_id();
        data[id] = item;
        return id;
    }

    inline void remove(i32 id) {
        data.erase(id);
        //free_id.push(id);
    }

    inline void remove(vector<i32> id) {
        for (i32 i : id) remove(i);
    }

    inline bool contains(i32 id) {
        return data.find(id) != data.end();
    }
};


//
// File
//

bool readFile(const string &path, string &data);
bool fileExists(const string &path);

