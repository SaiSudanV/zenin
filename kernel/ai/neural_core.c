/*
 * Project Zenin - Experimental fixed-weight intent classifier.
 * Training provenance, quantization format, accuracy, and performance are unverified.
 */

#include "../include/neural_core.h"
#include "../include/uart.h"

static int32_t dot_i8_scalar(const int8_t *weights, const int8_t *input) {
    int32_t sum = 0;
    for (int i = 0; i < NEURAL_INPUT_DIM; i++) {
        sum += (int32_t)weights[i] * (int32_t)input[i];
    }
    return sum;
}

/* Exact signed INT8 dot product using baseline AArch64 Advanced SIMD/NEON.
 * The kernel enables FP/SIMD access before kmain. The scalar fallback keeps
 * this neural source portable, but other architecture ports need their own
 * feature detection/build validation before claiming acceleration. */
static int32_t dot_i8_accelerated(const int8_t *weights, const int8_t *input) {
#if defined(__aarch64__)
    int32_t sum;
    __asm__ volatile(
        "movi v0.8h, #0\n"
        "ld1 {v1.16b}, [%1]\n"
        "ld1 {v2.16b}, [%2]\n"
        "smlal v0.8h, v1.8b, v2.8b\n"
        "smlal2 v0.8h, v1.16b, v2.16b\n"
        "saddlv s3, v0.8h\n"
        "umov %w0, v3.s[0]\n"
        : "=r"(sum)
        : "r"(weights), "r"(input)
        : "v0", "v1", "v2", "v3", "memory");
    return sum;
#else
    return dot_i8_scalar(weights, input);
#endif
}

/* Hand-authored prototype weights; this is not a validated trained model. */
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

bool neural_core_backend_selftest(void) {
    int8_t input[8][NEURAL_INPUT_DIM];
    for (int vector = 0; vector < 8; vector++) {
        for (int i = 0; i < NEURAL_INPUT_DIM; i++) {
            if (vector == 0) input[vector][i] = INT8_MAX;
            else if (vector == 1) input[vector][i] = INT8_MIN;
            else if (vector == 2) input[vector][i] = (i & 1) ? INT8_MAX : INT8_MIN;
            else input[vector][i] = (int8_t)(((vector * 11 + i * 7) % 31) - 15);
        }
    }
    for (int vector = 0; vector < 8; vector++) {
        for (int row = 0; row < NEURAL_HIDDEN_DIM; row++) {
            int32_t scalar = dot_i8_scalar(model_weights.w1[row], input[vector]);
            int32_t accelerated = dot_i8_accelerated(model_weights.w1[row], input[vector]);
            if (scalar != accelerated) return false;
        }
    }
    return true;
}

void neural_core_init(void) {
#if defined(__aarch64__)
    uart_puts("[zenin-ai] AArch64 NEON first-layer kernel enabled; model quality is unverified.\n");
#else
    uart_puts("[zenin-ai] Scalar experimental classifier enabled; model quality is unverified.\n");
#endif
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
        sum += dot_i8_accelerated(model_weights.w1[h], input_features);
        hidden[h] = relu(sum);
    }

    /* Output layer calculation: O = W2 * H + B2 */
    int32_t max_val = -999999;
    zenin_neural_intent_t best_intent = NEURAL_INTENT_UNKNOWN;

    for (int o = 0; o < NEURAL_OUTPUT_DIM; o++) {
        int32_t sum = model_weights.b2[o];
        for (int h = 0; h < NEURAL_HIDDEN_DIM; h++) {
            sum += (int32_t)model_weights.w2[o][h] * hidden[h];
        }
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
