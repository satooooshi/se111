/*
 *
 * Student Name:Huichuan Hui
 * Student ID:516413990003
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include "cachelab.h"

#define HIT 1
#define MISS 2
#define EVICTION 4

typedef struct CacheTag {
    int s, b, E, v;
    int count;
    int *lines;
    int *timestamps;
    int misses, hits, evictions;
} Cache;

Cache *makeCache(int s, int E, int b, int v);
void freeCache(Cache *cache);
void updateStat(Cache *cache, int result);
void printVerbose(int result);
int visit(Cache *cache, long addr, long size);
void printUsage(char *program);


void printUsage(char *program) {
    const char *usage =
        "Options:\n"
            "  -h         Print this help message.\n"
            "  -v         Optional verbose flag.\n"
            "  -s <num>   Number of set index bits.\n"
            "  -E <num>   Number of lines per set.\n"
            "  -b <num>   Number of block offset bits.\n"
            "  -t <file>  Trace file.\n";
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", program);
    printf("%s\n", usage);
    printf("Examples:\n"
               "  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n"
               "linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n",
           program, program);
}




Cache *makeCache(int s, int E, int b, int v) {
    Cache *cache = (Cache *) malloc(sizeof(Cache));
    cache->s = s;
    cache->b = b;
    cache->E = E;
    cache->v = v;
    size_t size = (1<<s) * E * sizeof(long);
    cache->count = cache->hits = cache->evictions = cache->misses = 0;
    cache->lines = (int *) malloc(size);
    cache->timestamps = (int *) malloc(size);
    memset(cache->lines, 0, size);
    memset(cache->timestamps, 0, size);
    return cache;
}

void freeCache(Cache *cache) {
    free(cache->lines);
    free(cache->timestamps);
    free(cache);
}


void updateStat(Cache *cache, int result) {
    if (result & HIT)cache->hits++;
    if (result & MISS)cache->misses++;
    if (result & EVICTION)cache->evictions++;
}


void printVerbose(int result) {
    if (result & HIT)
        printf("hit ");
    if (result & MISS)
        printf("miss ");
    if (result & EVICTION)
        printf("eviction");
}



int visit(Cache *cache, long addr, long size) {
    int result = 0;

    int set, tag;

    addr >>= cache->b;
    set = addr & ((1 << cache->s) - 1);
    tag = addr >>= cache->s;

    int index = cache->E * set;

    cache->count++;
    int lru = index;
    int lru_time = 0x7FFFFFFF;

    for (int i = index; i < index + cache->E; i++) {
        if (cache->timestamps[i] && cache->lines[i] == tag) {
            cache->timestamps[i] = cache->count;
            result |= HIT;
        }
        if (cache->timestamps[i] < lru_time) {
            lru_time = cache->timestamps[i];
            lru = i;
        }
    }
    if ((result & HIT) == 0) {
        result |= MISS;
        if (cache->timestamps[lru] > 0) result |= EVICTION;
        cache->lines[lru] = tag;
        cache->timestamps[lru] = cache->count;
    }
    updateStat(cache, result);
    if (cache->v) printVerbose(result);
    return result;
}




int main(int argc, char **argv) {
    int s, b, E, v;
    int arg_count = 0;
    char *filename = NULL;

    int ch;
    while ((ch = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
        switch (ch) {
            case 'h':
                printUsage(argv[0]);
                return 0;
                break;
            case 'v':
                v = 1;
                break;
            case 's':
                s = atoi(optarg);
                ++arg_count;
                break;
            case 'E':
                E = atoi(optarg);
                ++arg_count;
                break;
            case 'b':
                b = atoi(optarg);
                ++arg_count;
                break;
            case 't':
                filename = optarg;
                ++arg_count;
                break;
            default:
                break;
        }
    }
    if (arg_count < 4) {
        printf("Missing required command line argument\n");
        printUsage(argv[0]);
    }
    FILE *file = fopen(filename, "r");
    Cache *cache = makeCache(s, E, b, v);
    char line[64];
    while (fgets(line, 64, file) != NULL) {
        if (line[0] != ' ') continue;//line[0] not space
        char mode;
        long addr, size;
        sscanf(line, " %c %lx,%lx", &mode, &addr, &size);
        if (v) printf("%c %lx,%lx ", mode, addr, size);
        visit(cache, addr, size);
        if (mode == 'M')visit(cache, addr, size);
        printf("\n");
    }

    freeCache(cache);

    printSummary(cache->hits, cache->misses, cache->evictions);
    return 0;
}
