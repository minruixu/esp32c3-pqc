#include <stdlib.h>
#include <string.h>
#include "sphincsa_dp.h"

#define MAX_L 66

/* Each row is its own heap block, so the table also fits a heap that is
 * split into regions (the largest block of the RAM image is 112 KB). */
static uint8_t *row_data[MAX_L + 1];
static uint16_t row_cols[MAX_L + 1];
static uint8_t row_width[MAX_L + 1];
static int cur_l, cur_w, cur_s;

typedef uint64_t u256[4];

static void u256_add(u256 z, const u256 x, const u256 y)
{
    uint64_t c = 0;
    for (int i = 0; i < 4; i++) {
        uint64_t t = x[i] + c;
        c = t < c;
        z[i] = t + y[i];
        c += z[i] < t;
    }
}

static void u256_sub(u256 z, const u256 x, const u256 y)
{
    uint64_t b = 0;
    for (int i = 0; i < 4; i++) {
        uint64_t t = x[i] - b;
        b = x[i] < b;
        z[i] = t - y[i];
        b += t < y[i];
    }
}

static int u256_bytes(const u256 x)
{
    for (int i = 31; i >= 0; i--) {
        if ((x[i / 8] >> (8 * (i % 8))) & 0xff) {
            return i + 1;
        }
    }
    return 1;
}

int sphincsa_dp_select(int l, int w)
{
    int s = l * (w - 1) / 2;
    u256 *prev, *cur, *tmp, acc;

    if (l == cur_l && w == cur_w) {
        return 0;
    }
    cur_l = cur_w = 0;
    for (int i = 0; i <= MAX_L; i++) {
        free(row_data[i]);
        row_data[i] = NULL;
    }
    if (l > MAX_L) {
        return -1;
    }
    /* two rows of the full table are enough to build it */
    prev = calloc((size_t)s + 1, sizeof(u256));
    cur = calloc((size_t)s + 1, sizeof(u256));
    if (prev == NULL || cur == NULL) {
        free(prev);
        free(cur);
        return -1;
    }
    prev[0][0] = 1; /* dp[0][0] = 1 */
    for (int i = 0; i <= l; i++) {
        int cols, width = 1;

        if (i > 0) {
            /* dp[i][j] = sum of dp[i-1][j-k] for k = 0..w-1, as a sliding sum */
            memset(acc, 0, sizeof(acc));
            for (int j = 0; j <= s; j++) {
                u256_add(acc, acc, prev[j]);
                if (j >= w) {
                    u256_sub(acc, acc, prev[j - w]);
                }
                memcpy(cur[j], acc, sizeof(u256));
            }
            tmp = prev;
            prev = cur;
            cur = tmp;
        }
        /* prev now holds row i; keep columns 0..min(s, i(w-1)/2) */
        cols = i * (w - 1) / 2;
        if (cols > s) {
            cols = s;
        }
        cols += 1;
        for (int j = 0; j < cols; j++) {
            int b = u256_bytes(prev[j]);
            if (b > width) {
                width = b;
            }
        }
        row_data[i] = malloc((size_t)cols * (size_t)width);
        if (row_data[i] == NULL) {
            free(prev);
            free(cur);
            return -1;
        }
        row_cols[i] = (uint16_t)cols;
        row_width[i] = (uint8_t)width;
        for (int j = 0; j < cols; j++) {
            for (int b = 0; b < width; b++) {
                row_data[i][(size_t)j * (size_t)width + (size_t)b] =
                    (uint8_t)(prev[j][b / 8] >> (8 * (b % 8)));
            }
        }
    }
    free(prev);
    free(cur);
    cur_l = l;
    cur_w = w;
    cur_s = s;
    return 0;
}

void sphincsa_dp_get(uint64_t out[4], int i, int j)
{
    const int top = i * (cur_w - 1); /* largest sum i digits can reach */
    const uint8_t *p;
    int width;

    out[0] = out[1] = out[2] = out[3] = 0;
    if (j < 0 || j > top) {
        return;
    }
    if (j >= row_cols[i]) {
        j = top - j; /* symmetry */
    }
    width = row_width[i];
    p = row_data[i] + (size_t)j * (size_t)width;
    for (int b = 0; b < width; b++) {
        out[b / 8] |= (uint64_t)p[b] << (8 * (b % 8));
    }
}
