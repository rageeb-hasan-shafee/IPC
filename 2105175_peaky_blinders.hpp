#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <sstream>
#include <iomanip>
#include <memory>
#include <random>
#include <chrono>
#include <pthread.h>
#include <unistd.h>
#include <atomic>

using namespace std;

const int NUM_STATIONS = 4;
const int SLEEP_MULTIPLIER = 100;  


int N, M, doc_time, log_time;
atomic<int> completed_operations{0};
atomic<bool> simulation_ended{false};
chrono::time_point<chrono::high_resolution_clock> start_time;


ofstream* output_file = nullptr;

//typrewriting station
pthread_mutex_t output_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t station_mutexes[NUM_STATIONS];
pthread_cond_t station_cvs[NUM_STATIONS];
vector<queue<int>> station_queues(NUM_STATIONS);

// logbook scripting
pthread_mutex_t logbook_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t logbook_cv = PTHREAD_COND_INITIALIZER;
atomic<int> active_readers{0};
atomic<bool> active_writer{false};
queue<int> logbook_queue;

// Group completion 
vector<pthread_mutex_t*> group_mutexes;
vector<pthread_cond_t*> group_cvs;
vector<atomic<int>*> group_completed_ops;

int get_time() {
    auto now = chrono::high_resolution_clock::now();
    return chrono::duration<double>(now - start_time).count();
}

int get_random_delay(int max) {
    static thread_local random_device rd;
    static thread_local mt19937 gen(rd());
    double lambda = 10000.234;
    poisson_distribution<> dist(lambda);
    return dist(gen);
}

void log_message(const string& msg) {
    pthread_mutex_lock(&output_mutex);
    if (output_file && output_file->is_open()) {
        *output_file << fixed << setprecision(2) << msg << endl;
        output_file->flush();
    }
    pthread_mutex_unlock(&output_mutex);
}

struct OperativeData {
    int id;
    int group_id;
    bool is_leader;
    
    OperativeData(int id) : id(id) {
        group_id = (id - 1) / M + 1;
        is_leader = (id % M == 0) || (id == N);
    }
};

struct StaffData {
    int staff_id;
    StaffData(int id) : staff_id(id) {}
};