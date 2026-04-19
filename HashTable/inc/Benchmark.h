#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <stddef.h>

#define TARGET_LOAD_FACTOR 10.0

typedef struct TestContext TestContext;

TestContext* TestContextCreate( int words_count, int max_queries, double load_factor  );
void         TestContextLoadData( TestContext *ctx );
void         TestContextGenerateQueries( TestContext *ctx );
void         TestContextRunWarmup( TestContext *ctx, int warmup_count );
void         TestContextRunBenchmark( TestContext *ctx, int num_runs, int num_searches );
void         TestContextDestroy( TestContext *ctx );

#endif // BENCHMARK_H