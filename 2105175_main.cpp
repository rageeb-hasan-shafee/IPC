#include "2105175_operations.hpp"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <input_file> <output_file>" << endl;
        return 1;
    }

    ifstream input(argv[1]);
    if (!input) {
        cerr << "Error opening input file" << endl;
        return 1;
    }
    input >> N >> M >> doc_time >> log_time;
    input.close();

    if (N <= 0 || M <= 0 || doc_time <= 0 || log_time <= 0) {
        cerr << "Invalid input parameters" << endl;
        return 1;
    }

    ofstream output(argv[2]);
    if (!output) {
        cerr << "Error opening output file" << endl;
        return 1;
    }
    output_file = &output;

    start_time = chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_STATIONS; i++) {
        pthread_mutex_init(&station_mutexes[i], nullptr);
        pthread_cond_init(&station_cvs[i], nullptr);
    }

    int num_groups = N / M;
    group_mutexes.reserve(num_groups);
    group_cvs.reserve(num_groups);
    group_completed_ops.reserve(num_groups);
    
    for (int i = 0; i < num_groups; i++) {
        pthread_mutex_t* mutex = new pthread_mutex_t;
        pthread_cond_t* cond = new pthread_cond_t;
        pthread_mutex_init(mutex, nullptr);
        pthread_cond_init(cond, nullptr);
        
        group_mutexes.push_back(mutex);
        group_cvs.push_back(cond);
        group_completed_ops.push_back(new atomic<int>(0));
    }

    vector<OperativeData*> operatives;
    vector<pthread_t> operative_threads(N);
    
    for (int i = 1; i <= N; i++) {
        operatives.push_back(new OperativeData(i));
        pthread_create(&operative_threads[i-1], nullptr, operative_thread, operatives[i-1]);
    }

    StaffData staff1(1), staff2(2);
    pthread_t staff_thread1, staff_thread2;
    pthread_create(&staff_thread1, nullptr, intelligence_staff_thread, &staff1);
    pthread_create(&staff_thread2, nullptr, intelligence_staff_thread, &staff2);

    for (int i = 0; i < N; i++) {
        pthread_join(operative_threads[i], nullptr);
    }

    usleep(1000000); // 1 second delay

    simulation_ended.store(true);

    pthread_mutex_lock(&logbook_mutex);
    pthread_cond_broadcast(&logbook_cv);
    pthread_mutex_unlock(&logbook_mutex);

    pthread_join(staff_thread1, nullptr);
    pthread_join(staff_thread2, nullptr);
    {
        pthread_mutex_lock(&output_mutex);
        if (output_file && output_file->is_open()) {
            *output_file << "Simulation completed successfully!" << endl;
        }
        pthread_mutex_unlock(&output_mutex);
    }

    // Cleanup
    for (int i = 0; i < NUM_STATIONS; i++) {
        pthread_mutex_destroy(&station_mutexes[i]);
        pthread_cond_destroy(&station_cvs[i]);
    }

    for (int i = 0; i < num_groups; i++) {
        pthread_mutex_destroy(group_mutexes[i]);
        pthread_cond_destroy(group_cvs[i]);
        delete group_mutexes[i];
        delete group_cvs[i];
        delete group_completed_ops[i];
    }

    for (auto* op : operatives) {
        delete op;
    }

    pthread_mutex_destroy(&output_mutex);
    pthread_mutex_destroy(&logbook_mutex);
    pthread_cond_destroy(&logbook_cv);

    output_file = nullptr;
    output.close();
    return 0;
}