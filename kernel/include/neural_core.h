/*
 * Project Zenin - Experimental fixed-point intent classifier.
 * The current model uses signed INT8 weight storage; AArch64 uses baseline
 * NEON on the first projection. This is not a validated trained model.
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

/* Tensor dimensions for the experimental INT8 classifier. */
#define NEURAL_INPUT_DIM   16
#define NEURAL_HIDDEN_DIM  16
#define NEURAL_OUTPUT_DIM  6

typedef struct {
    int8_t w1[NEURAL_HIDDEN_DIM][NEURAL_INPUT_DIM];
    int8_t b1[NEURAL_HIDDEN_DIM];
    int8_t w2[NEURAL_OUTPUT_DIM][NEURAL_HIDDEN_DIM];
    int8_t b2[NEURAL_OUTPUT_DIM];
} zenin_neural_weights_t;

/* Initialize neural engine weights and zero-state memory */
void neural_core_init(void);

/* Compare accelerated and scalar first-layer arithmetic on fixed test vectors. */
bool neural_core_backend_selftest(void);

/* Run the fixed-point inference vector (NEON first layer on AArch64). */
zenin_neural_intent_t neural_core_predict(const int8_t *input_features, int32_t *confidence_score);

/* Tokenize raw input phrase into a fixed 16-element embedding vector */
void neural_core_embed(const char *phrase, int8_t *out_features);

#endif /* ZENIN_NEURAL_CORE_H */
