/*
 * Name:Taekang Eom, POVIS ID:tkeom0114
 */
#include "cachelab.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <getopt.h>
#include <math.h>

struct cache_param {
    int s;
    int S;
    int E;
    int b;
    int B;
    int hits;
    int misses;
    int evictions;
    int show;
    long long count;
};

struct cache_line {
    long long tag;
    int last_used;
    int valid;
};

void print_usage()
{
    printf("Usage: ./csim [-hv] -s <num> -E <num> -b <num> -t <file>\n");
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n");
    printf("\nExamples:\n");
    printf("  ./csim -s 4 -E 1 -b 4 -t traces/yi.trace\n");
    printf("  ./csim -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}

int find_hit(struct cache_line **cache, struct cache_param param, int set_index ,long long tag)
{
    int index = -1;
    for(int i=0;i < param.E; i++)
    {
        if(cache[set_index][i].valid && cache[set_index][i].tag == tag)
        {
            index = i;
            break;
        }
    }
    return index;
}

int find_empty(struct cache_line **cache, struct cache_param param, int set_index)
{
    int index = -1;
    for(int i=0;i < param.E; i++)
    {
        if(!cache[set_index][i].valid)
        {
            index = i;
            break;
        }
    }
    return index;
}

int find_evict(struct cache_line **cache, struct cache_param param, int set_index)
{
    int index = 0;
    int last_used = cache[set_index][0].last_used;
    for(int i=1;i < param.E; i++)
    {
        if(cache[set_index][i].last_used < last_used)
        {
            index = i;
            last_used = cache[set_index][i].last_used;
        }
    }
    return index;
}

struct cache_param run_cache(struct cache_line **cache, struct cache_param param, long long address)
{
    param.count++;
    int set_index = (int) ((address >> param.b) % param.S);
    long long tag = address >> (param.s + param.b);
    int line_index;
    int hit_index = find_hit(cache, param, set_index, tag);
    int empty_index = find_empty(cache, param, set_index);
    int evict_index = find_evict(cache, param, set_index);
    if(hit_index != -1)
    {
        param.hits++;
        line_index = hit_index;
        if(param.show)
        {
            printf(" hit");
        }
    }
    else
    {
        param.misses++;
        if(param.show)
        {
            printf(" miss");
        }
        if(empty_index != -1)
        {
            line_index = empty_index;
        }
        else
        {
            param.evictions++;
            line_index = evict_index;
            if(param.show)
            {
                printf(" eviction");
            }
        }
    }
    cache[set_index][line_index].last_used = param.count;
    cache[set_index][line_index].tag = tag;
    cache[set_index][line_index].valid = 1;
    return param;
}

int main(int argc, char **argv)
{
    int option=0;
    struct cache_param param;
    char *file_name;
    

    while((option = getopt(argc, argv, "hvs:E:b:t:"))!=EOF)
	{
        
		switch(option)
		{
			case 's': param.s = atoi(optarg); break;
			case 'E': param.E = atoi(optarg); break;
            case 'b': param.b = atoi(optarg); break;
            case 't': file_name = optarg; break;
            case 'h': print_usage(); return 0;
            case 'v': param.show = 1; break;
			default: print_usage(); return 1;
		}
	}

	param.S = 1 << param.s;
    param.B = 1 << param.b;
    param.count = 0;
    param.evictions = 0;
    param.misses = 0;
    param.hits = 0;

    struct cache_line **cache = malloc(param.S * sizeof(struct cache_line*));
    for(int i=0; i < param.S; i++)
    {
        cache[i] = malloc(param.E * sizeof(struct cache_line));
    }

    FILE *trace;
    
    char operation;
    long long address;
    int size;

    trace = fopen(file_name, "r");
    if (trace != NULL) {
		while (fscanf(trace, " %c %llx,%d", &operation, &address, &size) == 3) {
            if(param.show && operation != 'I')
            {
                printf("%c %llx,%d", operation, address, size);
            }
			switch(operation) {
				case 'I':
					break;
				case 'L':
					param = run_cache(cache, param, address);
					break;
				case 'S':
					param = run_cache(cache, param, address);
					break;
				case 'M':
					param = run_cache(cache, param, address);
					param = run_cache(cache, param, address);
					break;
				default:
					break;
			}
            if(param.show && operation != 'I')
            {
                printf("\n");
            }
		}
	}
    printSummary(param.hits, param.misses, param.evictions);
    for(int i=0; i < param.S; i++)
    {
        free(cache[i]);
    }
    free(cache);
	return 0;
}
