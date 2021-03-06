/* Student Name: Li Ying
   Student ID: 516030910444
 
   I use three structure to construct my cache
   (1) A struct line, which is composed of three elements: valid,tag,nextline. 
       valid is a int to record whether the line is used, 
       tag is a long to identify the line in the set, 
       next_line is a ptr pointing to the next line.
   (2) A linkedlist, each list stands for a set and is composed of used lines.
   (3) The cache is an array, which consists of several linkedlists.
  
   I use the function parse_cmd to parse the command user inputs.
   I use the function process to process instructions. The main idea is to go through the list in the set.
   IF hit, put it to the back. Else, judge whether the set is full. 
   IF full, evict the head_line and change its tag then put it to the back. Else, add a new line to the back.
   
 */
#include "cachelab.h"
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#define HIT 1
#define MISS 2
#define MISS_M_EVICTION 3
#define MISS_M_HIT 4
#define MISS_EVICTION 5
#define GET_NO_SET(addr,s,b) ((0x7fffffff>>(31-s))&((addr)>>b))
#define GET_ADDR_TAG(addr,s,b) (((addr)>>(s+b))&(0x7fffffff>>(s+b)))

static int s, E, b;
static int hit_count;
static int miss_count;
static int eviction_count;

typedef struct line
{
	int valid;
	long tag;
	struct line *next_line;
} line;


line **cache;


int init_cache()
{
 	cache = (line**)malloc((1<<s) * sizeof(long));
	for (int i = 0; i < (1<<s); i++)
		cache[i] = (line*)malloc(sizeof(line)); 
	return 1;
}


int parse_cmd(int argc, char** argv, int* s, int* E, int* b, int* flag, char* filename)
{
	int arg_num = 0;
	int temp;
	while ((temp = getopt(argc, argv, "s:E:b:t:vh")) != -1)
	{
		switch (temp){
			case 's':
				*s = atoi(optarg);
				arg_num += 1;
				break;
			case 'E':
			    *E = atoi(optarg);
				arg_num += 1;
				break;
			case 'b':
				*b = atoi(optarg);
				arg_num += 1;
				break;
			case 'v':
				*flag = 1;
				arg_num += 1;
				break;
			case 't':
				strcpy(filename, optarg);
				arg_num ++;
				break;
			case 'h':
			default:
				printf("mdzz");
				break;
		}
	}
	if (arg_num < 4)
		return -1;
	return 0;
}


int process(char op, int NO_set, long addr_tag, int size)
{
	line *temp = (cache[NO_set]);
	line *add_line = NULL;
	int count = 0;
	while (temp->next_line)
	{
		//if hit
        if ((((temp->next_line)->valid) == 1) && (((temp->next_line)->tag) == addr_tag))
		{
			add_line = temp->next_line;
			line *post = (add_line->next_line);	
			(temp->next_line) = post;
			(add_line->next_line) = NULL;
			if (op=='M')
				hit_count += 2;
			else
				hit_count += 1;
			if (!(temp->next_line))
			{
				temp->next_line = add_line;
				return HIT;
			}
		}
		temp = (temp->next_line);
		count ++;			
	}	
	if (add_line != NULL)
	{
		temp->next_line = add_line;
		return HIT;
	}	
	//if miss
	miss_count += 1;
	if (op != 'M')
	{
		if (count == E)      //evict the least recently used line
		{
			if ( E == 1)
				(((cache[NO_set])->next_line)->tag) = addr_tag;
			else
			{
				add_line = ((cache[NO_set])->next_line);
				((cache[NO_set])->next_line) = (add_line->next_line);
				(add_line->tag) = addr_tag;
				(add_line->next_line) = NULL;
				(temp->next_line) = add_line;
			}
			eviction_count += 1;
			return MISS_EVICTION;
		}
		else                           //put the new_line to the array
		{
			line *new_line = (line*)malloc(sizeof(line));
			(new_line->valid) = 1;
			(new_line->tag) = addr_tag;
			(new_line->next_line) = NULL;
			(temp->next_line) = new_line;
			return MISS;
		}
	}
	else
	{
		if (count == E)      //evict the least recently used line
		{	
			if (count == 1)
				(((cache[NO_set])->next_line)->tag) = addr_tag;
			else
			{
				add_line = ((cache[NO_set])->next_line);
				((cache[NO_set])->next_line) = (add_line->next_line);
				(add_line->tag) = addr_tag;
				(add_line->next_line) = NULL;
				(temp->next_line) = add_line;
			}
			eviction_count += 1;
			hit_count += 1;
			return MISS_M_EVICTION;
		}
		else                           //put the new_line to the array
		{
			line *new_line = (line*)malloc(sizeof(line));
			(new_line->valid) = 1;
			(new_line->tag) = addr_tag;
			(new_line->next_line) = NULL;
			(temp->next_line) = new_line;
			hit_count += 1;
			return MISS_M_HIT;
		}	
	}
}


int main(int argc, char** argv)
{
	int flag = 0;
	char filename[50];
	if (parse_cmd(argc, argv, &s, &E, &b, &flag, filename) < 0)
		exit(-1);
	init_cache();

	FILE *tracefile = fopen(filename,"r");
	if (!tracefile)
	{
		fprintf(stderr,"%s: No such file or dictionary.\n",filename);
		return 1;
	}
	int size;
	long addr;
	char op;
	int c;
	while ((c=getc(tracefile))!= EOF)
	{
		if(c != ' ')
		{
			while ((c = getc(tracefile)) != '\n')
			{
				if (c == EOF)
					break;
			}
			continue;
		}
		fscanf(tracefile,"%c %lx,%d",&op, &addr, &size);
		while (c != '\n')
			c = getc(tracefile);
		
		int NO_set = GET_NO_SET(addr, s, b);
		long addr_tag = GET_ADDR_TAG(addr, s, b);
		int state = process(op, NO_set, addr_tag, size);
		if (flag)
		{
			switch (state){
				case(HIT):
					if(op == 'M')
						printf("%c %lx,%d hit hit\n", op, addr, size);
					else
						printf("%c %lx,%d hit\n", op, addr, size);
					break;
				case(MISS):
					printf("%c %lx,%d miss\n", op, addr, size);
					break;
				case(MISS_M_EVICTION):
			        printf("%c %lx,%d miss,eviction hit\n", op, addr, size);
					break;
				case(MISS_M_HIT):
					printf("%c %lx,%d miss hit\n", op, addr, size);
					break;
				case(MISS_EVICTION):
					printf("%c %lx,%d miss eviction\n", op, addr, size);
					break;
				default:
					break;
				}
		}
	}
    printSummary(hit_count, miss_count, eviction_count);
    return 0;
}

