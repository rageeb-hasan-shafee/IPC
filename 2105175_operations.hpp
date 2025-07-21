#include "2105175_peaky_blinders.hpp"

void document_recreation(OperativeData* op) {
    usleep(get_random_delay(4000) * 1000); 

    int station_id = (op->id - 1) % NUM_STATIONS;
    {
        ostringstream oss;
        auto arrival_time = chrono::high_resolution_clock::now();
        //auto duration = chrono::duration_cast<chrono::milliseconds>(arrival_time - start_time);
        oss << "Operative " << op->id << " arrived at TS" << (station_id + 1) << " at time " <<  get_time();
        log_message(oss.str());
    }

    pthread_mutex_lock(&station_mutexes[station_id]);
    station_queues[station_id].push(op->id);

    ostringstream oss_wait;
    oss_wait << "Operative " << op->id << " is waiting at TS" << (station_id + 1);
    log_message(oss_wait.str());

    while (station_queues[station_id].empty() || station_queues[station_id].front() != op->id) {
        pthread_cond_wait(&station_cvs[station_id], &station_mutexes[station_id]);
    }

    chrono::time_point<chrono::high_resolution_clock> start_doc = chrono::high_resolution_clock::now();
    ostringstream oss_start;
    oss_start << "Operative " << op->id << " started document recreation at TS" << (station_id + 1) << " at time " << get_time();
    log_message(oss_start.str());

    pthread_mutex_unlock(&station_mutexes[station_id]);

    chrono::duration<int> doc_duration = chrono::seconds(doc_time);
    chrono::time_point<chrono::high_resolution_clock> work_start = chrono::high_resolution_clock::now();
    while (chrono::high_resolution_clock::now() - work_start < doc_duration) {
        usleep(SLEEP_MULTIPLIER * 1000); // small random delays to simulate realistic work
    }  
    pthread_mutex_lock(&station_mutexes[station_id]);

    auto end_doc = chrono::high_resolution_clock::now();
    chrono::duration<double> doc_duration_seconds = end_doc - start_doc; 
    {
        ostringstream oss;
        oss << "Operative " << op->id << " completed document recreation at time " << get_time() << " (completion took " << doc_duration_seconds.count() <<" seconds)";
        log_message(oss.str());
    }

    station_queues[station_id].pop();
    if (!station_queues[station_id].empty()) {
        pthread_cond_signal(&station_cvs[station_id]);
    }
    pthread_mutex_unlock(&station_mutexes[station_id]);

    // notifying leader
    group_completed_ops[op->group_id - 1]->fetch_add(1);
    pthread_cond_signal(group_cvs[op->group_id - 1]);
}

void logbook_entry(OperativeData* op) {
    if (!op->is_leader) return;

    //waiting for all members to be finished
    pthread_mutex_lock(group_mutexes[op->group_id - 1]);
    while (group_completed_ops[op->group_id - 1]->load() < M) {
        pthread_cond_wait(group_cvs[op->group_id - 1], group_mutexes[op->group_id - 1]);
    }
    pthread_mutex_unlock(group_mutexes[op->group_id - 1]);

    if (simulation_ended.load()) return;

    chrono::time_point<chrono::high_resolution_clock> logbook_start_time = chrono::high_resolution_clock::now();
    {
        ostringstream oss_start;
        oss_start << "Unit " << op->group_id << " is requesting logbook access at time " << get_time();
        log_message(oss_start.str());
    }

    pthread_mutex_lock(&logbook_mutex);
    logbook_queue.push(op->group_id);

    /* Writer wait korbe jodi:
    1. active readers thake (must wait korte hobe until ALL readers complete hoy)
    2. active writer jodi thake
    3. writer jodi queue er front e na thake*/
    while ((active_readers.load() > 0 || 
            active_writer.load() || 
            logbook_queue.empty() || 
            logbook_queue.front() != op->group_id) && 
           !simulation_ended.load()) {
        pthread_cond_wait(&logbook_cv, &logbook_mutex);
    }

    if (simulation_ended.load()) {
        while (!logbook_queue.empty() && logbook_queue.front() == op->group_id) {
            logbook_queue.pop();
        }
        pthread_mutex_unlock(&logbook_mutex);
        return;
    }

    active_writer.store(true);
    logbook_queue.pop();
    pthread_mutex_unlock(&logbook_mutex);

    chrono::time_point<chrono::high_resolution_clock> log_start_time = chrono::high_resolution_clock::now();
    {
        ostringstream oss_start;
        oss_start << "Unit " << op->group_id << " has started logbook entry at time " << get_time();
        log_message(oss_start.str());
    }

    chrono::duration<int> log_duration = chrono::seconds(log_time);
    chrono::time_point<chrono::high_resolution_clock> log_start = chrono::high_resolution_clock::now();
    while (chrono::high_resolution_clock::now() - log_start < log_duration) {
        usleep(SLEEP_MULTIPLIER * 1000); 
    }

    auto end_log = chrono::high_resolution_clock::now();
    chrono::duration<double> log_duration_seconds = end_log - start_time;
    
    completed_operations.fetch_add(1);
    {
        ostringstream oss;
        oss << "Unit " << op->group_id << " has completed intelligence distribution at time " << get_time() << " . Operations completed = " << completed_operations.load();
        log_message(oss.str());
    }

    pthread_mutex_lock(&logbook_mutex);
    active_writer.store(false);
    pthread_cond_broadcast(&logbook_cv); //notifying all threads waiting for logbook access
    pthread_mutex_unlock(&logbook_mutex);
}

void* operative_thread(void* arg) {
    OperativeData* op = static_cast<OperativeData*>(arg);
    
    document_recreation(op);
    logbook_entry(op);
    
    return nullptr;
}

void* intelligence_staff_thread(void* arg) {
    StaffData* staff = static_cast<StaffData*>(arg);
    int total_units = N / M; 

    while (!simulation_ended.load() && completed_operations.load() < total_units) {
        usleep(get_random_delay(3000) * 1000); 

        if (simulation_ended.load()) break;

        pthread_mutex_lock(&logbook_mutex);

        /* Reader wait korbe kebol jodi ekta active writer thake
        Multiple readers simultaneously read korte parbe*/
        while (active_writer.load() && !simulation_ended.load()) {
            pthread_cond_wait(&logbook_cv, &logbook_mutex);
        }

        if (simulation_ended.load()) {
            pthread_mutex_unlock(&logbook_mutex);
            break;
        }

        active_readers.fetch_add(1);
        pthread_mutex_unlock(&logbook_mutex);
        {
            ostringstream oss;
            oss << "\nIntelligence Staff " << staff->staff_id << " began reviewing logbook at time " << get_time() << ". Operations completed = " << completed_operations.load() << endl;
            log_message(oss.str());
        }

        // Simulate realistic review time with random delays
        chrono::time_point<chrono::high_resolution_clock> review_start = chrono::high_resolution_clock::now();
        chrono::duration<int> review_duration = chrono::seconds(log_time); // 1-4 seconds
        while (chrono::high_resolution_clock::now() - review_start < review_duration) {
            usleep(SLEEP_MULTIPLIER * 1000); // Add small delays to simulate realistic review work
        }

        auto review_end = chrono::high_resolution_clock::now();
        chrono::duration<double> review_time = review_end - review_start;      
        {
            ostringstream oss;
            oss << "\nIntelligence Staff " << staff->staff_id << " finished reviewing logbook at time " << get_time() << " (review took " << review_time.count() << " seconds)\n";
            log_message(oss.str());
        }

        pthread_mutex_lock(&logbook_mutex);
        active_readers.fetch_sub(1);

        // When the last reader finishes, wake up waiting writers
        if (active_readers.load() == 0) {
            pthread_cond_broadcast(&logbook_cv);
        }
        pthread_mutex_unlock(&logbook_mutex);
    }

    ostringstream oss;
    oss << "Intelligence Staff " << staff->staff_id << " exiting at time " << get_time();
    log_message(oss.str());
    
    return nullptr;
}