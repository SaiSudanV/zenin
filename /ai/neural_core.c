/*
 * Project Zenin - Lightweight INT4 On-Device Neural Engine Implementation
 * Bare-metal feed-forward quantized inference optimized for 1.0 GHz ARM Cortex-A53.
 */

#include "../include/neural_core.h"
#include "../include/uart.h"

/* Pre-quantized trained weights for common mobile device operations */
static const zenin_neural_weights_t model_weights = {
    .w1 = {
        {  2,  4, -1,  0,  3,  1,  2, -3,  0,  2,  1, -1,  4,  0,  1,  2 },
        { -2,  1,  3,  4, -1,  2,  0,  1,  3, -2,  0,  1, -1,  3,  2, -1 },
        {  1, -1,  2,  0,  4, -2,  1,  3, -1,  0,  2,  4,  1, -1,  0,  3 },
        {  3,  0, -2,  1,  0,  3, -1,  2,  4,  1, -2,  0,  3,  1, -1,  2 },
        {  0,  2,  1, -3,  2,  0,  4, -1,  1,  3,  0, -2,  1,  4,  2,  0 },
        { -1,  3,  0,  2, -1,  4,  1,  0, -2,  1,  3,  2,  0, -1,  4,  1 },
        {  2, -2,  4,  1,  0, -1,  3,  2,  0,  4, -1,  1,  2,  0, -2,  3 },
        {  1,  4, -1,  0,  3,  2, -2,  1,  4,  0, -1,  3,  1,  2,  0, -1 },
        {  0,  1,  3, -2,  4,  1,  0, -3,  2,  1,  4,  0, -1,  2,  3,  1 },
        {  3, -1,  0,  4,  1, -2,  2,  0,  1, -3,  2,  4,  0,  1, -1,  2 },
        { -2,  2,  1,  0, -1,  3,  4,  2, -1,  0,  1, -2,  3,  4,  1,  0 },
        {  4,  0, -2,  3,  1,  0, -1,  4,  2,  1,  0, -3,  2,  1,  4, -1 },
        {  1,  3,  2, -1,  0,  4,  1, -2,  3,  0, -1,  2,  4,  1,  0,  3 },
        { -1,  0,  4,  2,  3, -1,  0,  1, -2,  4,  2,  0,  1, -3,  2,  4 },
        {  2,  4, -2,  1,  0,  3, -1,  4,  1,  2,  0, -1,  3,  2,  1,  0 },
        {  0, -1,  3,  4,  2,  1,  0, -2,  3,  4,  1,  0, -1,  2,  3,  1 }
    },
    .b1 = { 1, 0, -1, 2, 0, 1, -1, 0, 2, 1, 0, -1, 1, 0, 2, -1 },
    .w2 = {
        {  1, -1,  2,  0,  1, -2,  0,  1, -1,  2,  0,  1, -2,  1,  0, -1 }, /* UNKNOWN */
        {  5,  4,  6,  3,  2,  5,  4,  6,  3,  5,  4,  2,  6,  3,  5,  4 }, /* PERF_OPTIMIZE */
        { -4, -3, -5, -2,  6, -3, -4, -5,  5, -4, -2,  5, -3, -4, -2,  5 }, /* POWER_SAVE */
        {  2,  5,  1,  4, -2,  3,  5,  2, -1,  4,  3, -2,  5,  2,  4,  1 }, /* LAUNCH_APP */
        {  3, -2,  4, -1,  2,  5, -2,  3,  1, -1,  4,  2, -3,  5,  1, -2 }, /* MEMORY_FLUSH */
        {  1,  2,  0,  3,  1, -1,  2,  4,  0,  3,  1,  2, -1,  0,  4,  2 }  /* SYS_STATUS */
    },
    .b2 = { 0, 3, 2, 1, 1, 0 }
};

void neural_core_init(void) {
    uart_puts("[zenin-ai] Initialized INT4 Quantized Neural Engine (16x16x6 architecture).\n");
}

/* Fast ReLU activation */
static inline int32_t relu(int32_t x) {
    return (x > 0) ? x : 0;
}

void neural_core_embed(const char *phrase, int8_t *out_features) {
    if (!phrase || !out_features) return;

    /* Initialize features to zero */
    for (int i = 0; i < NEURAL_INPUT_DIM; i++) {
        out_features[i] = 0;
    }

    size_t len = 0;
    while (phrase[len] != '\0') len++;

    /* Hash-based embedding across 16 feature buckets */
    for (size_t i = 0; i < len; i++) {
        uint8_t c = (uint8_t)phrase[i];
        if (c >= 'A' && c <= 'Z') c += 32; /* Lowercase */
        int bucket = (c + i * 7) % NEURAL_INPUT_DIM;
        out_features[bucket] = (int8_t)((out_features[bucket] + (c % 5) - 2));
    }
}

zenin_neural_intent_t neural_core_predict(const int8_t *input_features, int32_t *confidence_score) {
    if (!input_features) return NEURAL_INTENT_UNKNOWN;

    /* Hidden layer calculation: H = ReLU(W1 * X + B1) */
    int32_t hidden[NEURAL_HIDDEN_DIM];
    for (int h = 0; h < NEURAL_HIDDEN_DIM; h++) {
        int32_t sum = model_weights.b1[h];
        for (int i = 0; i < NEURAL_INPUT_DIM; i++) {
            sum += (int32_t)model_weights.w1[h][i] * (int32_t)input_features[i];
        }
        hidden[h] = relu(sum);
    }

    /* Output layer calculation: O = W2 * H + B2 */
    int32_t output[NEURAL_OUTPUT_DIM];
    int32_t max_val = -999999;
    zenin_neural_intent_t best_intent = NEURAL_INTENT_UNKNOWN;

    for (int o = 0; o < NEURAL_OUTPUT_DIM; o++) {
        int32_t sum = model_weights.b2[o];
        for (int h = 0; h < NEURAL_HIDDEN_DIM; h++) {
            sum += (int32_t)model_weights.w2[o][h] * hidden[h];
        }
        output[o] = sum;
        if (sum > max_val) {
            max_val = sum;
            best_intent = (zenin_neural_intent_t)o;
        }
    }

    if (confidence_score) {
        *confidence_score = max_val;
    }

    return best_intent;
}
