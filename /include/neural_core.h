/*
 * Project Zenin - Lightweight INT4 On-Device Neural Engine
 * Ultra-fast quantized tensor inference engine running on bare-metal Cortex-A53.
 * Zero external libraries, zero dynamic memory leaks, instant hardware-level dispatch.
 */

#ifndef ZENIN_NEURAL_CORE_H
#define ZENIN_NEURAL_CORE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Number of neural classes recognized by Zenin AI */
typedef enum {
    NEURAL_INTENT_UNKNOWN = 0,
    NEURAL_INTENT_PERF_OPTIMIZE,   /* Boost CPU, disable background sync, max fps */
    NEURAL_INTENT_POWER_SAVE,      /* Clock down, screen dim, disable radios */
    NEURAL_INTENT_LAUNCH_APP,      /* Multi-OS application dispatch */
    NEURAL_INTENT_MEMORY_FLUSH,    /* Reclaim zombie heap blocks */
    NEURAL_INTENT_SYS_STATUS       /* Query thermal and resource metrics */
} zenin_neural_intent_t;

/* Tensor dimensions for lightweight INT4 classifier */
#define NEURAL_INPUT_DIM   16
#define NEURAL_HIDDEN_DIM  16
#define NEURAL_OUTPUT_DIM  6

typedef struct {
    int8_t w1[NEURAL_HIDDEN_DIM][NEURAL_INPUT_DIM]; /* INT4/INT8 quantized weights */
    int8_t b1[NEURAL_HIDDEN_DIM];
    int8_t w2[NEURAL_OUTPUT_DIM][NEURAL_HIDDEN_DIM];
    int8_t b2[NEURAL_OUTPUT_DIM];
} zenin_neural_weights_t;

/* Initialize neural engine weights and zero-state memory */
void neural_core_init(void);

/* Run feedforward quantized INT4 inference vector */
zenin_neural_intent_t neural_core_predict(const int8_t *input_features, int32_t *confidence_score);

/* Tokenize raw input phrase into a fixed 16-element embedding vector */
void neural_core_embed(const char *phrase, int8_t *out_features);

#endif /* ZENIN_NEURAL_CORE_H */
