#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>


struct RelaxationStruct { // Arguments required when performing relaxation
    double precision, **array;
    int size;
    bool changed;
    pthread_barrier_t queueBarrier, signalBarrier;
};

struct ThreadStruct { // Arguments required when adding a thread to the thread pool
    int start, end;
    bool endOfProgram, jobAvailable;
    struct RelaxationStruct auxStruct;
};

void perform_relaxation_parallel(struct RelaxationStruct*, int start, int end);

void thread_pool_manager(struct ThreadStruct threadStruct) { // Thread pool
    // All threads run this function in parallel.
    int start = threadStruct.start;
    int end = threadStruct.end;
    pthread_barrier_wait(&threadStruct.auxStruct.signalBarrier);

    while (!threadStruct.endOfProgram) { // Run until the final solution is obtained
        if (threadStruct.jobAvailable) {
            // Ensure that all threads have picked up the signal.
            pthread_barrier_wait(&threadStruct.auxStruct.queueBarrier);
            // Perform job
            perform_relaxation_parallel(&threadStruct.auxStruct, start, end);
        }
    }
}


void perform_relaxation_parallel(struct RelaxationStruct* auxStruct, int start, int end) { // Relaxation computation
    // This function is executed by each thread in parallel with unique ranges.
    int size = auxStruct->size;
    double** array = auxStruct->array;
    double currValue;
    // Main relaxation computation
    // Compute the average for the same set range of elements in all rows.
    for (unsigned int i = 1; i < size-1; i++) {
        for (unsigned int j = start; j < end; j++) {
            currValue = array[i][j];
            array[i][j] = (array[i-1][j] +
            array[i+1][j] + array[i][j-1] + array[i][j+1])/4; // Take average and round to specified precision.

            // If change has occurred, set the boolean flag to true.
            if (!auxStruct->changed) {
                if (fabs(array[i][j]-currValue) >= auxStruct->precision) auxStruct->changed = true;
            }
        }
    }
    /*
     * Barrier to ensure all threads have completed their computation before..-
     * -.. the shared, original 'array' is manipulated.
     */
//    pthread_barrier_wait(&auxStruct->queueBarrier);

}

double** perform_relaxation(double** array, int size, int threads, double precision) {
    if (size < 3) return array; // 3x3 is the smallest array to perform this operation.

    if (threads > 1) { // Parallel vs sequential
        // Max number of threads should be half of the inner array size.
        if (threads > (size-2)/2) threads = (size-2)/2;

        /* 'Signal' barrier used to ensure shared resources are not altered
         * before each thread passes a certain point.
         */
        struct ThreadStruct threadStruct;
        pthread_barrier_init(&threadStruct.auxStruct.signalBarrier, NULL, 2);
        // Elements to be processed per thread, taking into account the boundaries
        int portion = (size - 2) / threads;
        int remainder = (size - 2) % threads;
        threadStruct.endOfProgram = false;
        threadStruct.jobAvailable = false; // Initially no job is available.

        pthread_t* threadsArray = (pthread_t*) malloc(threads * sizeof(pthread_t));
        if (threadsArray == NULL) return NULL;
        // Create threads and add them to the thread pool.
        for (int i = 0; i < threads; i++) {
            if (i < remainder) { // Deal with remainders using thread ID
                threadStruct.start = i * (portion+1);
                threadStruct.end = threadStruct.start + portion+1;
            }
            else {
                threadStruct.start = (portion+1)*remainder + portion*(i-remainder);
                threadStruct.end = threadStruct.start + portion;
            }
            // Take into account starting boundary column
            threadStruct.start++;
            threadStruct.end++;
            if (pthread_create(&threadsArray[i], NULL, (void*) thread_pool_manager,
                               &threadStruct)) return NULL; // Ensure that threads are created.
            // Ensure thread has stored its unique range of values to compute.
            pthread_barrier_wait(&threadStruct.auxStruct.signalBarrier);
        }

        threadStruct.auxStruct.changed = true; // Initial value of bool flag.
        threadStruct.auxStruct.array = array;
        threadStruct.auxStruct.size = size;
        threadStruct.auxStruct.precision = precision;

        // Main barrier which all threads and the main thread must wait behind.
        pthread_barrier_init(&threadStruct.auxStruct.queueBarrier, NULL, threads+1);
        while (threadStruct.auxStruct.changed) { // Iterate until values have converged.
            threadStruct.auxStruct.changed = false;
            threadStruct.jobAvailable = true; // Signal for new iteration
            // Ensure all threads have picked up the signal on time
            pthread_barrier_wait(&threadStruct.auxStruct.queueBarrier);
            threadStruct.jobAvailable = false;
            // Wait for all threads to finish working on the shared data.
//            pthread_barrier_wait(&threadStruct.auxStruct.queueBarrier);
        }
        // Solution has been obtained, threads can be destroyed now.
        threadStruct.endOfProgram = true;

        // Ensure all threads are at the end of their function before advancing.
        for (unsigned int i = 0; i < threads; i++) pthread_join(threadsArray[i], NULL);
        free(threadsArray);
        // Destroy used primitives.
        pthread_barrier_destroy(&threadStruct.auxStruct.signalBarrier);
        pthread_barrier_destroy(&threadStruct.auxStruct.queueBarrier);

    }
    else { // Sequential
        bool changed = true;
        double currValue;
        while (changed) { // Iterate until the values have converged.
            changed = false;
            // Main relaxation computation
            // Compute the average for the same set range of elements in all rows.
            for (unsigned int i = 1; i < size-1; i++) {
                for (unsigned int j = 1; j < size-1; j++) {
                    // Take average and round to specified precision.
                    currValue = array[i][j];
                    array[i][j] = (array[i-1][j] +
                    array[i+1][j] + array[i][j-1] + array[i][j+1])/4;

                    // Set bool flag to whether value has changed.
                    if (!changed) changed =
                    fabs(array[i][j]-currValue) >= precision;
                }
            }
        }
    }
    return array;
}

int main () {
    int size = 10; // Size
    // Create original array to pass into the function.
    double** newArray = malloc(size * sizeof(double*));
    if (newArray == NULL) return -1;
    for (int unsigned i = 0; i < size; i++) {
        newArray[i] = malloc(size * sizeof(double));
        if (newArray[i] == NULL) return -1;
    }
    for (unsigned int j = 0; j < size; j++) {
            newArray[0][j] = 1;
    }
    for (unsigned int i = 1; i < size; i++) {
        newArray[i][0] = 1;
    }
    for (unsigned int i = 1; i < size; i++) {
        for (unsigned int j = 1; j < size; j++) {
            newArray[i][j] = 0;
        }
    }

    time_t start, end;
    time(&start);
    newArray = perform_relaxation(newArray, size, 8, 0.001); // Array, size, threads, precision
    time(&end);
    if (newArray == NULL) return 1;

    printf("Time taken: %g seconds.\n", difftime(end, start));

    // Script used for testing.
    // FILE* file = fopen("results.out", "a");
     for (unsigned int i = 0; i < size; i++) {
         for (unsigned int j = 0; j < size; j++) {
             printf("%.3f\t", newArray[i][j]);
            // fprintf(file, "%.3f,", newArray[i][j]);
         }
         printf("\n");
     }
    // fprintf(file, "\n");

    for (unsigned int i = 0; i < size; i++) free(newArray[i]);
    free(newArray);
    return 0;
}
