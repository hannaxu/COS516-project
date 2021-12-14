#include "constants.h"

// value represents size of invariant, max size = 8
int loop_invariant_buffer[MAX_LOADS][MAX_CONTEXTS];
int linear_predictors[MAX_LOADS][MAX_CONTEXTS];

void init_buffers(int num_loop_invariant_loads, int num_contexts) {
    for(int i = 0; i < num_loop_invariant_loads; i++) {
        for(int j = 0; j < num_contexts; j++) {
            // COST OF: mmap both buffers
            loop_invariant_buffer[i][j] = 0;
            linear_predictors[i][j] = 0;
        }
    }
}

void uninit_buffers(int num_loop_invariant_loads, int num_contexts) {
    for(int i = 0; i < num_loop_invariant_loads; i++) {
        for(int j = 0; j < num_contexts; j++) {
            // COST OF: munmap both buffers
        }
    }
}

// change size
void update_loop_invariants(int num_loop_invariant_loads, int num_contexts) {
    for(int i = 0; i < num_loop_invariant_loads; i++) {
        for(int j = 0; j < num_contexts; j++) {
            // COST OF: 4 bit operations
            loop_invariant_buffer[i][j] = (loop_invariant_buffer[i][j]+i) % 8;
        }
    }
}

// change size
void update_linear_predicted_values(int num_loop_invariant_loads, int num_contexts) {
    for(int i = 0; i < num_loop_invariant_loads; i++) {
        for(int j = 0; j < num_contexts; j++) {
            // COST OF: 4 bit operations
            linear_predictors[i][j] = (linear_predictors[i][j]+j) % 8;
        }
    }
}

// change loads
int update_shadow_loop_invariants(int num_loop_invariant_loads, int num_contexts) {
    int change = 0;
    for(int i = 0; i < num_loop_invariant_loads; i++) {
        for(int j = 0; j < num_contexts; j++) {
            // COST OF: 4 bit operations
            for(int k = 0; k < loop_invariant_buffer[i][j]; k++) {
                // COST OF: array access and & bit operation

            }
            if(loop_invariant_buffer[i][j]) {
                change += 1;
            }
        }
    }
    if(change > MAX_LOADS || change <= 0) {
        change = MAX_LOADS;
    }
    return change;
}

// change contexts
int update_shadow_linear_predicted_values(int num_loop_invariant_loads, int num_contexts) {
    int change = 0;
    for(int i = 0; i < num_loop_invariant_loads; i++) {
        for(int j = 0; j < num_contexts; j++) {
            // COST OF: 4 bit operations
            for(int k = 0; k < linear_predictors[i][j]; k++) {
                // COST OF: array access and & bit operation
                
            }
            if(linear_predictors[i][j]) {
                change += 1;
            }
        }
    }
    if(change > MAX_CONTEXTS || change <= 0) {
        change = MAX_CONTEXTS;
    }
    return change;
}