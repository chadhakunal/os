#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0;
static int failed = 0;

static void result(const char *name, int ok) {
    if (ok) { printf("  [PASS] %s\n", name); passed++; }
    else     { printf("  [FAIL] %s\n", name); failed++; }
}

/* ------------------------------------------------------------------ */
/* Comparators                                                         */
/* ------------------------------------------------------------------ */

static int cmp_int(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
}

static int cmp_int_r(const void *a, const void *b, void *arg) {
    (void)arg;
    return *(const int *)a - *(const int *)b;
}

static int is_sorted_int(const int *arr, int n) {
    for (int i = 1; i < n; i++)
        if (arr[i] < arr[i-1]) return 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/* 1. Empty and single-element arrays                                  */
/* ------------------------------------------------------------------ */
static void test_edge_cases(void) {
    int arr[4] = {3, 1, 4, 1};

    /* zero elements — must not crash */
    qsort(arr, 0, sizeof(int), cmp_int);
    result("qsort 0 elements: no crash", 1);

    /* one element */
    int one = 42;
    qsort(&one, 1, sizeof(int), cmp_int);
    result("qsort 1 element: value unchanged", one == 42);

    /* NULL base with nmemb=0 — must not crash */
    qsort(NULL, 0, sizeof(int), cmp_int);
    result("qsort NULL base nmemb=0: no crash", 1);
}

/* ------------------------------------------------------------------ */
/* 2. Small arrays — correctness                                       */
/* ------------------------------------------------------------------ */
static void test_small(void) {
    int arr[] = {5, 3, 1, 4, 2};
    int n = (int)(sizeof(arr) / sizeof(arr[0]));
    qsort(arr, (size_t)n, sizeof(int), cmp_int);
    result("qsort small: sorted", is_sorted_int(arr, n));
    result("qsort small: values preserved",
           arr[0]==1 && arr[1]==2 && arr[2]==3 && arr[3]==4 && arr[4]==5);
}

/* ------------------------------------------------------------------ */
/* 3. Already-sorted and reverse-sorted inputs                         */
/* ------------------------------------------------------------------ */
static void test_sorted_inputs(void) {
    int asc[8]  = {1,2,3,4,5,6,7,8};
    int desc[8] = {8,7,6,5,4,3,2,1};

    qsort(asc,  8, sizeof(int), cmp_int);
    result("qsort already sorted: still sorted", is_sorted_int(asc, 8));

    qsort(desc, 8, sizeof(int), cmp_int);
    result("qsort reverse sorted: now sorted", is_sorted_int(desc, 8));
}

/* ------------------------------------------------------------------ */
/* 4. All-equal elements                                               */
/* ------------------------------------------------------------------ */
static void test_all_equal(void) {
    int arr[16];
    for (int i = 0; i < 16; i++) arr[i] = 7;
    qsort(arr, 16, sizeof(int), cmp_int);
    int ok = 1;
    for (int i = 0; i < 16; i++) if (arr[i] != 7) ok = 0;
    result("qsort all-equal: all values unchanged", ok);
}

/* ------------------------------------------------------------------ */
/* 5. Large array (128 elements)                                       */
/* ------------------------------------------------------------------ */
static void test_large(void) {
    static int arr[128];
    /* fill with descending values */
    for (int i = 0; i < 128; i++) arr[i] = 128 - i;
    qsort(arr, 128, sizeof(int), cmp_int);
    result("qsort 128 elements: sorted", is_sorted_int(arr, 128));
}

/* ------------------------------------------------------------------ */
/* 6. Large element size — the original bug (256-byte structs)         */
/* ------------------------------------------------------------------ */
struct big {
    char data[256];
    int  key;
};

static int cmp_big(const void *a, const void *b) {
    return ((const struct big *)a)->key - ((const struct big *)b)->key;
}

static void test_large_element_size(void) {
    static struct big arr[32];
    for (int i = 0; i < 32; i++) {
        memset(arr[i].data, i, sizeof(arr[i].data));
        arr[i].key = 32 - i;  /* descending */
    }

    qsort(arr, 32, sizeof(struct big), cmp_big);

    int sorted = 1;
    for (int i = 1; i < 32; i++)
        if (arr[i].key < arr[i-1].key) { sorted = 0; break; }
    result("qsort 260-byte elements: sorted", sorted);

    /* Verify data integrity — each element's data bytes match its key. */
    int intact = 1;
    for (int i = 0; i < 32; i++) {
        int expected_fill = 32 - arr[i].key; /* original index before sort */
        for (int j = 0; j < (int)sizeof(arr[i].data); j++) {
            if ((unsigned char)arr[i].data[j] != (unsigned char)expected_fill) {
                intact = 0;
                break;
            }
        }
        if (!intact) break;
    }
    result("qsort 260-byte elements: data integrity preserved", intact);
}

/* ------------------------------------------------------------------ */
/* 7. Very large element size (512 bytes)                              */
/* ------------------------------------------------------------------ */
struct huge {
    char pad[504];
    int  key;
};

static int cmp_huge(const void *a, const void *b) {
    return ((const struct huge *)a)->key - ((const struct huge *)b)->key;
}

static void test_huge_element_size(void) {
    static struct huge arr[16];
    for (int i = 0; i < 16; i++) arr[i].key = 16 - i;

    qsort(arr, 16, sizeof(struct huge), cmp_huge);

    int sorted = 1;
    for (int i = 1; i < 16; i++)
        if (arr[i].key < arr[i-1].key) { sorted = 0; break; }
    result("qsort 508-byte elements: sorted", sorted);
}

/* ------------------------------------------------------------------ */
/* 8. Duplicates handled correctly                                     */
/* ------------------------------------------------------------------ */
static void test_duplicates(void) {
    int arr[] = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5};
    int n = (int)(sizeof(arr) / sizeof(arr[0]));
    qsort(arr, (size_t)n, sizeof(int), cmp_int);
    result("qsort with duplicates: sorted", is_sorted_int(arr, n));
}

/* ------------------------------------------------------------------ */
/* 9. qsort_r basic correctness                                        */
/* ------------------------------------------------------------------ */
static void test_qsort_r(void) {
    int arr[] = {5, 3, 1, 4, 2};
    int n = (int)(sizeof(arr) / sizeof(arr[0]));
    qsort_r(arr, (size_t)n, sizeof(int), cmp_int_r, NULL);
    result("qsort_r small: sorted", is_sorted_int(arr, n));
}

/* ------------------------------------------------------------------ */
/* 10. qsort_r passes arg through                                      */
/* ------------------------------------------------------------------ */
static int cmp_with_bias(const void *a, const void *b, void *arg) {
    int bias = *(int *)arg;
    return (*(const int *)a + bias) - (*(const int *)b + bias);
}

static void test_qsort_r_arg(void) {
    int arr[] = {5, 3, 1, 4, 2};
    int n = (int)(sizeof(arr) / sizeof(arr[0]));
    int bias = 100;
    qsort_r(arr, (size_t)n, sizeof(int), cmp_with_bias, &bias);
    result("qsort_r arg passed to comparator", is_sorted_int(arr, n));
}

int main(void) {
    printf("\n=== qsort Tests ===\n\n");

    printf("Edge cases:\n");
    test_edge_cases();

    printf("\nCorrectness:\n");
    test_small();
    test_sorted_inputs();
    test_all_equal();
    test_duplicates();
    test_large();

    printf("\nLarge element sizes (original bug):\n");
    test_large_element_size();
    test_huge_element_size();

    printf("\nqsort_r:\n");
    test_qsort_r();
    test_qsort_r_arg();

    printf("\n=== Summary: %d passed, %d failed ===\n\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
