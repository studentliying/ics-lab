#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "y64asm.h"

line_t *line_head = NULL;
line_t *line_tail = NULL;
int lineno = 0;

#define err_print(_s, _a ...) do { \
  if (lineno < 0) \
    fprintf(stderr, "[--]: "_s"\n", ## _a); \
  else \
    fprintf(stderr, "[L%d]: "_s"\n", lineno, ## _a); \
} while (0);


int64_t vmaddr = 0;    /* vm addr */

/* register table */
const reg_t reg_table[REG_NONE] = {
    {"%rax", REG_RAX, 4},
    {"%rcx", REG_RCX, 4},
    {"%rdx", REG_RDX, 4},
    {"%rbx", REG_RBX, 4},
    {"%rsp", REG_RSP, 4},
    {"%rbp", REG_RBP, 4},
    {"%rsi", REG_RSI, 4},
    {"%rdi", REG_RDI, 4},
    {"%r8",  REG_R8,  3},
    {"%r9",  REG_R9,  3},
    {"%r10", REG_R10, 4},
    {"%r11", REG_R11, 4},
    {"%r12", REG_R12, 4},
    {"%r13", REG_R13, 4},
    {"%r14", REG_R14, 4}
};

const reg_t* find_register(char *name)
{
    int i;
    for (i = 0; i < REG_NONE; i++)
        if (!strncmp(name, reg_table[i].name, reg_table[i].namelen))
            return &reg_table[i];
    return NULL;
}


/* instruction set */
instr_t instr_set[] = {
    {"nop", 3,   HPACK(I_NOP, F_NONE), 1 },
    {"halt", 4,  HPACK(I_HALT, F_NONE), 1 },
    {"rrmovq", 6,HPACK(I_RRMOVQ, F_NONE), 2 },
    {"cmovle", 6,HPACK(I_RRMOVQ, C_LE), 2 },
    {"cmovl", 5, HPACK(I_RRMOVQ, C_L), 2 },
    {"cmove", 5, HPACK(I_RRMOVQ, C_E), 2 },
    {"cmovne", 6,HPACK(I_RRMOVQ, C_NE), 2 },
    {"cmovge", 6,HPACK(I_RRMOVQ, C_GE), 2 },
    {"cmovg", 5, HPACK(I_RRMOVQ, C_G), 2 },
    {"irmovq", 6,HPACK(I_IRMOVQ, F_NONE), 10 },
    {"rmmovq", 6,HPACK(I_RMMOVQ, F_NONE), 10 },
    {"mrmovq", 6,HPACK(I_MRMOVQ, F_NONE), 10 },
    {"addq", 4,  HPACK(I_ALU, A_ADD), 2 },
    {"subq", 4,  HPACK(I_ALU, A_SUB), 2 },
    {"andq", 4,  HPACK(I_ALU, A_AND), 2 },
    {"xorq", 4,  HPACK(I_ALU, A_XOR), 2 },
    {"jmp", 3,   HPACK(I_JMP, C_YES), 9 },
    {"jle", 3,   HPACK(I_JMP, C_LE), 9 },
    {"jl", 2,    HPACK(I_JMP, C_L), 9 },
    {"je", 2,    HPACK(I_JMP, C_E), 9 },
    {"jne", 3,   HPACK(I_JMP, C_NE), 9 },
    {"jge", 3,   HPACK(I_JMP, C_GE), 9 },
    {"jg", 2,    HPACK(I_JMP, C_G), 9 },
    {"call", 4,  HPACK(I_CALL, F_NONE), 9 },
    {"ret", 3,   HPACK(I_RET, F_NONE), 1 },
    {"pushq", 5, HPACK(I_PUSHQ, F_NONE), 2 },
    {"popq", 4,  HPACK(I_POPQ, F_NONE),  2 },
    {".byte", 5, HPACK(I_DIRECTIVE, D_DATA), 1 },
    {".word", 5, HPACK(I_DIRECTIVE, D_DATA), 2 },
    {".long", 5, HPACK(I_DIRECTIVE, D_DATA), 4 },
    {".quad", 5, HPACK(I_DIRECTIVE, D_DATA), 8 },
    {".pos", 4,  HPACK(I_DIRECTIVE, D_POS), 0 },
    {".align", 6,HPACK(I_DIRECTIVE, D_ALIGN), 0 },
    {NULL, 1,    0   , 0 } //end
};

instr_t *find_instr(char *name)
{
    int i;
    for (i = 0; instr_set[i].name; i++)
	if (strncmp(instr_set[i].name, name, instr_set[i].len) == 0)
	    return &instr_set[i];
    return NULL;
}

/* symbol table (don't forget to init and finit it) */
symbol_t *symtab = NULL;

/*
 * find_symbol: scan table to find the symbol
 * args
 *     name: the name of symbol
 *
 * return
 *     symbol_t: the 'name' symbol
 *     NULL: not exist
 */
symbol_t *find_symbol(char *name)
{
	symbol_t *temp;
	temp = symtab -> next;
	while ((temp!=NULL) && (temp->name!=NULL))
	{
		if (strncmp(temp->name,name,strlen(name))==0)
			return temp;
		temp = temp->next;
	}
    return NULL;
}


/*
 * add_symbol: add a new symbol to the symbol table
 * args
 *     name: the name of symbol
 *
 * return
 *     0: success
 *     -1: error, the symbol has exist
 */
int add_symbol(char *name)
{
    /* check duplicate */
	if (find_symbol(name) != NULL)
		return -1;
    /* create new symbol_t (don't forget to free it)*/
	symbol_t *new_sym = (symbol_t *)malloc(sizeof(symbol_t));
	memset(new_sym,0,sizeof(symbol_t));
	new_sym->name = name;
	new_sym->next = NULL;
	new_sym->addr = vmaddr;


    /* add the new symbol_t to symbol table */
	if (symtab->next == NULL)
	{
		symtab->next = new_sym;
		return 0;
	}
	symbol_t *temp = symtab->next;
	while (temp != NULL)
	{
		if (temp->next == NULL)
		{
			temp->next = new_sym;
			return 0;
		}
		temp = temp->next;
	}
}

/* relocation table (don't forget to init and finit it) */
reloc_t *reltab = NULL;

/*
 * add_reloc: add a new relocation to the relocation table
 * args
 *     name: the name of symbol
 *
 * return
 *     0: success
 *     -1: error, the symbol has exist
 */
void add_reloc(char *name, bin_t *bin)
{
    /* create new reloc_t (don't forget to free it)*/
  	reloc_t *new_rel = (reloc_t *)malloc(sizeof(reloc_t));
	new_rel->y64bin = bin;
	new_rel->next = NULL;
	new_rel->name = name;

	if (reltab->next == NULL)
	{
		reltab->next = new_rel;
		return;
	}
	reloc_t *temp = reltab->next;
	while (temp != NULL)
	{
		if (temp->next == NULL)
		{
			temp->next = new_rel;
			return;
		}
		temp = temp->next;
	}	
}


/* macro for parsing y64 assembly code */
#define IS_DIGIT(s) ((*(s)>='0' && *(s)<='9') || *(s)=='-' || *(s)=='+')
#define IS_LETTER(s) ((*(s)>='a' && *(s)<='z') || (*(s)>='A' && *(s)<='Z'))
#define IS_COMMENT(s) (*(s)=='#')
#define IS_REG(s) (*(s)=='%')
#define IS_IMM(s) (*(s)=='$')
#define IS_HEX_LETTER(s) ((*(s)>='a' && *(s)<='f') || (*(s)>='A' && *(s)<='F') || (*(s) == 'x'))

#define IS_BLANK(s) (*(s)==' ' || *(s)=='\t')
#define IS_END(s) (*(s)=='\0')

#define SKIP_BLANK(s) do {  \
  while(!IS_END(s) && IS_BLANK(s))  \
    (s)++;    \
} while(0);

/* return value from different parse_xxx function */
typedef enum { PARSE_ERR=-1, PARSE_REG, PARSE_DIGIT, PARSE_SYMBOL, 
    PARSE_MEM, PARSE_DELIM, PARSE_INSTR, PARSE_LABEL} parse_t;

/*
 * parse_instr: parse an expected data token (e.g., 'rrmovq')
 * args
 *     ptr: point to the start of string
 *     inst: point to the inst_t within instr_set
 *
 * return
 *     PARSE_INSTR: success, move 'ptr' to the first char after token,
 *                            and store the pointer of the instruction to 'inst'
 *     PARSE_ERR: error, the value of 'ptr' and 'inst' are undefined
 */
parse_t parse_instr(char **ptr, instr_t **inst)
{
    /* skip the blank */
	/*if (ptr == NULL)
		return PARSE_ERR;*/
	
	SKIP_BLANK(*ptr);
    /* find_instr and check end */
	instr_t *temp = find_instr(*ptr);
	if (temp == NULL)
		return PARSE_ERR;
	
	*ptr += temp->len;
	if (!IS_END(*ptr) && !IS_BLANK(*ptr))
		return PARSE_ERR;
    /* set 'ptr' and 'inst' */
	
	*inst = temp;
    return PARSE_INSTR;
}

/*
 * parse_delim: parse an expected delimiter token (e.g., ',')
 * args
 *     ptr: point to the start of string
 *
 * return
 *     PARSE_DELIM: success, move 'ptr' to the first char after token
 *     PARSE_ERR: error, the value of 'ptr' and 'delim' are undefined
 */
parse_t parse_delim(char **ptr, char delim)
{
    /* skip the blank and check */
	SKIP_BLANK(*ptr);
	if (IS_END(*ptr))
		return PARSE_ERR;
    /* set 'ptr' */
	if ((*ptr)[0] != delim)
	{
		err_print("Invalid ','");
		return PARSE_ERR;
	}
    *ptr +=  1;
	return PARSE_DELIM;
}

/*
 * parse_reg: parse an expected register token (e.g., '%rax')
 * args
 *     ptr: point to the start of string
 *     regid: point to the regid of register
 *
 * return
 *     PARSE_REG: success, move 'ptr' to the first char after token, 
 *                         and store the regid to 'regid'
 *     PARSE_ERR: error, the value of 'ptr' and 'regid' are undefined
 */
parse_t parse_reg(char **ptr, regid_t *regid)
{
    /* skip the blank and check */
	SKIP_BLANK(*ptr);
	if (IS_END(*ptr))
		return PARSE_ERR;
    /* find register */
	reg_t *reg = find_register(*ptr);
	if (reg == NULL)
		return PARSE_ERR;
    /* set 'ptr' and 'regid' */
	*ptr += reg->namelen;
	*regid = reg->id;

	return PARSE_REG;
}

/*
 * parse_symbol: parse an expected symbol token (e.g., 'Main')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *
 * return
 *     PARSE_SYMBOL: success, move 'ptr' to the first char after token,
 *                               and allocate and store name to 'name'
 *     PARSE_ERR: error, the value of 'ptr' and 'name' are undefined
 */
parse_t parse_symbol(char **ptr, char **name)
{
    /* skip the blank and check */
	SKIP_BLANK(*ptr);
	if (IS_END(*ptr))
		return PARSE_ERR;
    /* allocate name and copy to it */
    int len = 0;
	while (IS_LETTER(*ptr+len) || (IS_DIGIT(*ptr+len)))
	{
		len++;
		if (len >= strlen(*ptr))
			break;
	}
	if (!IS_LETTER(*ptr+len))
		len -= 1;
	char* temp_name = (char *)malloc(len*sizeof(char)+1);
	strncpy(temp_name,*ptr,len+1);
	*(temp_name+len+1)='\0';
	if (!IS_LETTER(temp_name))
		return PARSE_ERR;
    /* set 'ptr' and 'name' */
	*ptr += len + 1;
	*name = temp_name;


    return PARSE_SYMBOL;
}

/*
 * parse_digit: parse an expected digit token (e.g., '0x100')
 * args
 *     ptr: point to the start of string
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, move 'ptr' to the first char after token
 *                            and store the value of digit to 'value'
 *     PARSE_ERR: error, the value of 'ptr' and 'value' are undefined
 */
parse_t parse_digit(char **ptr, long *value)
{
    /* skip the blank and check */
	
	SKIP_BLANK(*ptr);
	if (IS_END(*ptr))
		return PARSE_ERR;
	
    /* calculate the digit, (NOTE: see strtoull()) */
	long val = strtoul(*ptr, ptr, 0);	

	while ((IS_DIGIT(*ptr)) || (IS_HEX_LETTER(*ptr)))
		(*ptr)++;

    /* set 'ptr' and 'value' */

	*value = val;
    return PARSE_DIGIT;

}

/*
 * parse_imm: parse an expected immediate token (e.g., '$0x100' or 'STACK')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, the immediate token is a digit,
 *                            move 'ptr' to the first char after token,
 *                            and store the value of digit to 'value'
 *     PARSE_SYMBOL: success, the immediate token is a symbol,
 *                            move 'ptr' to the first char after token,
 *                            and allocate and store name to 'name' 
 *     PARSE_ERR: error, the value of 'ptr', 'name' and 'value' are undefined
 */
parse_t parse_imm(char **ptr, char **name, long *value)
{
   /* skip the blank and check */
	SKIP_BLANK(*ptr);
	if (IS_END(*ptr))
		return PARSE_ERR;
	if (!IS_IMM(*ptr) && !IS_LETTER(*ptr))
		return PARSE_ERR;
	parse_t result;
    /* if IS_IMM, then parse the digit */
	if (IS_IMM(*ptr) || (IS_DIGIT(*ptr+1)))
	{
		(*ptr) ++;
		result = parse_digit(ptr,value);
		return PARSE_DIGIT;
	}
    /* if IS_LETTER, then parse the symbol */
    else if (IS_LETTER(*ptr))
	{
		result = parse_symbol(ptr,name);
		return PARSE_SYMBOL;
	}
	
    return PARSE_ERR;
}


parse_t parse_mem(char **ptr, long *value, regid_t *regid)
{
	SKIP_BLANK(*ptr);
	if (IS_END(*ptr))
 		return PARSE_ERR;

	if (IS_DIGIT(*ptr))
	{
		if (parse_digit(ptr, value) == PARSE_ERR)
		{	
			err_print("Invalid MEM");
			return PARSE_ERR;
		}
	}

	if ((*ptr)[0] != '(')
    {
        err_print("Invalid MEM");
        return PARSE_ERR;
    }
	else
	{
		(*ptr) ++;
		parse_t result2 = parse_reg(ptr,regid);
		if (result2 == PARSE_ERR)
			return PARSE_ERR;
		if ((*ptr)[0] != ')')
		{
			err_print("Invalid MEM");
			return PARSE_ERR;
		}
	}
    /* set 'ptr', 'value' and 'regid' */
	(*ptr) ++;
    return PARSE_MEM;
}

/*
 * parse_data: parse an expected data token (e.g., '0x100' or 'array')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, data token is a digit,
 *                            and move 'ptr' to the first char after token,
 *                            and store the value of digit to 'value'
 *     PARSE_SYMBOL: success, data token is a symbol,
 *                            and move 'ptr' to the first char after token,
 *                            and allocate and store name to 'name' 
 *     PARSE_ERR: error, the value of 'ptr', 'name' and 'value' are undefined
 */
parse_t parse_data(char **ptr, char **name, long *value)
{
    /* skip the blank and check */
	SKIP_BLANK(*ptr);
	if (IS_END(*ptr))
		return PARSE_ERR;
	if (!IS_DIGIT(*ptr) && !IS_LETTER(*ptr))
		return PARSE_ERR;
	parse_t result;
    /* if IS_DIGIT, then parse the digit */
	if (IS_DIGIT(*ptr))
	{	
		result = parse_digit(ptr,value);
		if (result == PARSE_ERR)
			return PARSE_ERR;
	}
    /* if IS_LETTER, then parse the symbol */
	if (IS_LETTER(*ptr))
	{
		result = parse_symbol(ptr,name);
		if (result == PARSE_ERR)
			return PARSE_ERR;
	}
    /* set 'ptr', 'name' and 'value' */
    return result;
}

/*
 * parse_label: parse an expected label token (e.g., 'Loop:')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *
 * return
 *     PARSE_LABEL: success, move 'ptr' to the first char after token
 *                            and allocate and store name to 'name'
 *     PARSE_ERR: error, the value of 'ptr' is undefined
 */
parse_t parse_label(char **ptr, char **name)
{
    /* skip the blank and check */
	SKIP_BLANK(*ptr);
	if (IS_END(*ptr))
		return PARSE_ERR;
    /* allocate name and copy to it */
	int len = 0;
	if (!IS_DIGIT(*ptr) && !IS_LETTER(*ptr))
		return PARSE_ERR;
	else
	{
		while (*(*ptr+len)!=':' && len<strlen(*ptr))
			len++;
	}
	if (*(*ptr+len) != ':')
		return PARSE_ERR;
	else
		len++;

	char *temp_name = (char *)malloc(len*(sizeof(char)));
	memset(temp_name,0,len-1*(sizeof(char)));
	strncpy(temp_name,*ptr,len-1);
	*(temp_name+len-1)='\0';
    /* set 'ptr' and 'name' */
	*name = temp_name;
	*ptr += len;
	return PARSE_LABEL;
}

/*
 * parse_line: parse a line of y64 code (e.g., 'Loop: mrmovq (%rcx), %rsi')
 * (you could combine above parse_xxx functions to do it)
 * args
 *     line: point to a line_t data with a line of y64 assembly code
 *
 * return
 *     PARSE_XXX: success, fill line_t with assembled y64 code
 *     PARSE_ERR: error, try to print err information (e.g., instr type and line number)
 */
type_t parse_line(line_t *line)
{

/* when finish parse an instruction or lable, we still need to continue check 
* e.g., 
*  Loop: mrmovl (%rbp), %rcx
*           call SUM  #invoke SUM function */

    /* skip blank and check IS_END */
	
//	err_print("line %d",lineno);

    char *pos = line->y64asm;
	SKIP_BLANK(pos);
	if (IS_END(pos))
		return line->type;
    /* is a comment ? */
	if (IS_COMMENT(pos))
		return line->type;
    /* is a label ? */
	parse_t result;
	char *temp_name;
	result = parse_label(&pos,&temp_name);
	if (result == PARSE_LABEL)
	{
		(line->y64bin).addr = vmaddr;
		line->type = TYPE_INS;
		if (add_symbol(temp_name) == -1)
		{
			err_print("Dup symbol:%s",temp_name);
			return TYPE_ERR;
		}
		SKIP_BLANK(pos);
		if (IS_END(pos) || IS_COMMENT(pos))
			return line->type;
	}
    /* is an instruction ? */

	SKIP_BLANK(pos);
	instr_t *inst = NULL;
	result = parse_instr(&pos,&inst);
	if (result == PARSE_ERR)
	{
		err_print("Invalid instr");
		return TYPE_ERR;
	}
    /* set type and y64bin */
	line->type = TYPE_INS;
	(line->y64bin).addr = vmaddr;
	(line->y64bin).codes[0] = inst->code;
	(line->y64bin).bytes = inst->bytes;
    /* update vmaddr */    
	vmaddr += inst->bytes;
    /* parse the rest of instruction according to the itype */
	char *name;
	long value;
	char delim = ',';
	regid_t regB;
	regid_t regA;
	parse_t result2;
	parse_t result3;
	switch (HIGH(inst->code))
	{
	case I_NOP: case I_HALT: case I_RET:
		return line->type;

	case I_IRMOVQ:
		result = parse_imm(&pos,&name,&value);
		if (result == PARSE_ERR){
			err_print("Invalid Immediate");
			return TYPE_ERR;
		}
		result2 = parse_delim(&pos,delim);
		if (result2 == PARSE_ERR)
			return TYPE_ERR;
		result3 = parse_reg(&pos,&regB);
		if (result3 == PARSE_ERR)
		{
			err_print("Invalid REG");
			return TYPE_ERR;
		}
		if (result == PARSE_DIGIT)
		{
			line->y64bin.codes[1] = HPACK(REG_NONE,regB); 
			(line->y64bin).codes[2] = (value & 0xff);
			(line->y64bin).codes[3] = (value>>8) & 0xff;
			(line->y64bin).codes[4] = (value>>16) & 0xff;
			(line->y64bin).codes[5] = (value>>24) & 0xff;
			(line->y64bin).codes[6] = (value>>32) & 0xff;
			(line->y64bin).codes[7] = (value>>40) & 0xff;
			(line->y64bin).codes[8] = (value>>48) & 0xff;
			(line->y64bin).codes[9] = (value>>56) & 0xff;
			return line->type;
		}
		if(result == PARSE_SYMBOL)
		{
			add_reloc(name,&(line->y64bin));
			(line->y64bin).codes[1] = HPACK(REG_NONE, (regB));
			return line->type;
		}

	case I_RRMOVQ:
		result = parse_reg(&pos,&regA);
		if (result == PARSE_ERR)
		{
			line->type = TYPE_ERR;
			return TYPE_ERR;
		}
		result2 = parse_delim(&pos,delim);
		if (result2 == PARSE_ERR)
		{
			line->type = TYPE_ERR;
			return TYPE_ERR;
		}
		result3 = parse_reg(&pos,&regB);
		if (result3 == PARSE_ERR)
		{
			line->type = TYPE_ERR;
			return TYPE_ERR;
		}
		line->y64bin.codes[1] = HPACK(regA,regB);
		return line->type;

	case I_RMMOVQ:
		if (parse_reg(&pos,&regA) == PARSE_ERR)
		{
			line->type = TYPE_ERR;
			err_print("Invalid REG");
			return TYPE_ERR;
		}	
		if (parse_delim(&pos,delim) == PARSE_ERR)
		{
			line->type = TYPE_ERR;
			return TYPE_ERR;
		}
		if (parse_mem(&pos,&value,&regB) == PARSE_ERR)
		{
			line->type = TYPE_ERR;
			return TYPE_ERR;
		}
		(line->y64bin).codes[1] = ((regA)<<4) + (regB);
		(line->y64bin).codes[2] = value & 0xff;
		(line->y64bin).codes[3] = (value>>8) & 0xff;
		(line->y64bin).codes[4] = (value>>16) & 0xff;
		(line->y64bin).codes[5] = (value>>24) & 0xff;
		(line->y64bin).codes[6] = (value>>32) & 0xff;
		(line->y64bin).codes[7] = (value>>40) & 0xff;
		(line->y64bin).codes[8] = (value>>48) & 0xff;
		(line->y64bin).codes[9] = (value>>56) & 0xff;
		return line->type;

	case I_MRMOVQ:
		if (parse_mem(&pos,&value,&regB) == PARSE_ERR)
		{
			line->type = TYPE_ERR;
			return TYPE_ERR;
		}	
		if (parse_delim(&pos,delim) == PARSE_ERR)
		{
			line->type = TYPE_ERR;
			return TYPE_ERR;
		}
		if (parse_reg(&pos,&regA) == PARSE_ERR)
		{
			line->type = TYPE_ERR;
			err_print("Invalid REG");
			return TYPE_ERR;
		}
		(line->y64bin).codes[1] = ((regA)<<4) + (regB);
		(line->y64bin).codes[2] = value & 0xff;
		(line->y64bin).codes[3] = (value>>8) & 0xff;
		(line->y64bin).codes[4] = (value>>16) & 0xff;
		(line->y64bin).codes[5] = (value>>24) & 0xff;
		(line->y64bin).codes[6] = (value>>32) & 0xff;
		(line->y64bin).codes[7] = (value>>40) & 0xff;
		(line->y64bin).codes[8] = (value>>48) & 0xff;
		(line->y64bin).codes[9] = (value>>56) & 0xff;
		return line->type;

	case I_ALU:
		if (parse_reg(&pos,&regA) == PARSE_ERR)
		{
			line->type = TYPE_ERR;
			err_print("Invalid REG");
			return TYPE_ERR;
		}
		
		if (parse_delim(&pos,delim) == PARSE_ERR)
		{
			line->type = TYPE_ERR;
			return TYPE_ERR;
		}
		if (parse_reg(&pos,&regB) == PARSE_ERR)
		{
			line->type = TYPE_ERR;
			err_print("Invalid REG");
			return TYPE_ERR;
		}
		(line->y64bin).codes[1] = ((regA)<<4) + (regB);
		return line->type;

	case I_JMP: case I_CALL:
		result = parse_data(&pos,&name,&value);
		if (result == PARSE_ERR)
		{
			line->type = TYPE_ERR;
			return TYPE_ERR;
		}
		if (result == PARSE_DIGIT)
		{
			line_t *temp_line = line_head;
			while (temp_line->next != NULL)
			{
				temp_line = temp_line->next;
			}
			if (value<0 || value>=temp_line->y64bin.addr)
			{
				err_print("Invalid DEST");
				line->type = TYPE_ERR;
				return TYPE_ERR;
			}
		(line->y64bin).codes[1] = value & 0xff;
		(line->y64bin).codes[2] = (value<<8) & 0xff;
		(line->y64bin).codes[3] = (value<<16) & 0xff;
		(line->y64bin).codes[4] = (value<<24) & 0xff;
		(line->y64bin).codes[5] = (value<<32) & 0xff;
		(line->y64bin).codes[6] = (value<<40) & 0xff;
		(line->y64bin).codes[7] = (value<<48) & 0xff;
		(line->y64bin).codes[8] = (value<<56) & 0xff;
		return line->type;
		}
		if (result == PARSE_SYMBOL)
		{
			add_reloc(name,&(line->y64bin));
			return line->type;
		}

	case I_PUSHQ: case I_POPQ:
		if (parse_reg(&pos,&regA) == PARSE_ERR)
		{
			err_print("Invalid REG");
			line->type = TYPE_ERR;
			return TYPE_ERR;
		}
		line->y64bin.codes[1] = ((regA)<<4) + 0xf;
		return line->type;
	case I_DIRECTIVE:
		switch(LOW(inst->code))
		{
		case D_DATA:
			result = parse_data(&pos,&name,&value);
			if (result == PARSE_ERR)
			{
				line->type = TYPE_ERR;
				return TYPE_ERR;
			}
			if (result == PARSE_DIGIT)
			{
				/*(line->y64bin).codes[0] = value & 0xff;
				(line->y64bin).codes[1] = (value>>8) & 0xff;
				(line->y64bin).codes[2] = (value>>16) & 0xff;
				(line->y64bin).codes[3] = (value>>24) & 0xff;
				(line->y64bin).codes[4] = (value>>32) & 0xff;
				(line->y64bin).codes[5] = (value>>40) & 0xff;
				(line->y64bin).codes[6] = (value>>48) & 0xff;
				(line->y64bin).codes[7] = (value>>56) & 0xff;*/
				for (int i = 0 ;i < inst->bytes;i++)
					(line->y64bin).codes[i] = HPACK( (value>>(i*8+4))&0xF, (value>>(i*8))&0xF);
				return line->type;
			}
					
			if (result == PARSE_SYMBOL)
			{
				add_reloc(name,&line->y64bin);
				return line->type;
			}

		case D_POS:
			if (parse_digit(&pos,&value) == PARSE_ERR)
			{
				line->type = TYPE_ERR;
				return TYPE_ERR;
			}
			vmaddr = value;
			line->y64bin.addr = value;
			return line->type;
		
		case D_ALIGN:
			if (parse_digit(&pos,&value) == PARSE_ERR)
			{
				line->type = TYPE_ERR;
				return TYPE_ERR;
			}
			if (vmaddr%value != 0)
			{
				vmaddr = vmaddr+value-vmaddr%value;
				line->y64bin.addr = vmaddr;
			}
			return line->type;
		}
	}
   // line->type = TYPE_ERR;
    return line->type;
}

/*
 * assemble: assemble an y64 file (e.g., 'asum.ys')
 * args
 *     in: point to input file (an y64 assembly file)
 *
 * return
 *     0: success, assmble the y64 file to a list of line_t
 *     -1: error, try to print err information (e.g., instr type and line number)
 */
int assemble(FILE *in)
{
    static char asm_buf[MAX_INSLEN]; /* the current line of asm code */
    line_t *line;
    int slen;
    char *y64asm;

    /* read y64 code line-by-line, and parse them to generate raw y64 binary code list */
    while (fgets(asm_buf, MAX_INSLEN, in) != NULL) {
        slen  = strlen(asm_buf);
        while ((asm_buf[slen-1] == '\n') || (asm_buf[slen-1] == '\r')) { 
            asm_buf[--slen] = '\0'; /* replace terminator */
        }

        /* store y64 assembly code */
        y64asm = (char *)malloc(sizeof(char) * (slen + 1)); // free in finit
        strcpy(y64asm, asm_buf);

        line = (line_t *)malloc(sizeof(line_t)); // free in finit
        memset(line, '\0', sizeof(line_t));

        line->type = TYPE_COMM;
        line->y64asm = y64asm;
        line->next = NULL;

        line_tail->next = line;
        line_tail = line;
        lineno ++;

        if (parse_line(line) == TYPE_ERR) {
            return -1;
        }
    }

	lineno = -1;
    return 0;
}

/*
 * relocate: relocate the raw y64 binary code with symbol address
 *
 * return
 *     0: success
 *     -1: error, try to print err information (e.g., addr and symbol)
 */
int relocate(void)
{
    reloc_t *rtmp = NULL;
    symbol_t *sym = NULL;
    rtmp = reltab->next;
    while (rtmp) {
        /* find symbol */
		sym = find_symbol(rtmp->name);
		if (sym == NULL)
		{
			err_print("Unknown symbol:'%s'",rtmp->name);
			return -1;
		}
        /* relocate y64bin according itype */
		rtmp->y64bin->codes[rtmp->y64bin->bytes-8] = (sym->addr) & 0xff;
		rtmp->y64bin->codes[rtmp->y64bin->bytes-7] = (sym->addr>>8) & 0xff;
		rtmp->y64bin->codes[rtmp->y64bin->bytes-6] = (sym->addr>>16) & 0xff;
		rtmp->y64bin->codes[rtmp->y64bin->bytes-5] = (sym->addr>>24) & 0xff;
		rtmp->y64bin->codes[rtmp->y64bin->bytes-4] = (sym->addr>>32) & 0xff;
		rtmp->y64bin->codes[rtmp->y64bin->bytes-3] = (sym->addr>>40) & 0xff;
		rtmp->y64bin->codes[rtmp->y64bin->bytes-2] = (sym->addr>>48) & 0xff;
		rtmp->y64bin->codes[rtmp->y64bin->bytes-1] = (sym->addr>>56) & 0xff;
        /* next */
        rtmp = rtmp->next;
    }
    return 0;
}

/*
 * binfile: generate the y64 binary file
 * args
 *     out: point to output file (an y64 binary file)
 *
 * return
 *     0: success
 *     -1: error
 */
int binfile(FILE *out)
{
    /* prepare image with y64 binary code */
	line_t *temp_line = line_head;
	int cur_addr = temp_line->y64bin.addr;
    /* binary write y64 code to output file (NOTE: see fwrite()) */
    while (temp_line != NULL)
	{
		int flag = 0;
		line_t *new_line = temp_line->next;
		while (new_line != NULL)
		{ 
			if(new_line->y64bin.bytes != 0)
			{
				flag =1;
				break;
			}
			new_line = new_line->next;
		}
		if (cur_addr < temp_line->y64bin.addr && flag == 1)
		{
			char a = 0;
			for (int i=0; i<temp_line->y64bin.addr-cur_addr; i++)
				fwrite(&a,sizeof(char),1,out);
			cur_addr = temp_line->y64bin.addr;
		}
		fwrite(temp_line->y64bin.codes,sizeof(char),temp_line->y64bin.bytes,out);
		cur_addr = cur_addr + temp_line->y64bin.bytes;
		temp_line = temp_line->next;
	}
    return 0;
}


/* whether print the readable output to screen or not ? */
bool_t screen = FALSE; 

static void hexstuff(char *dest, int value, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        char c;
        int h = (value >> 4*i) & 0xF;
        c = h < 10 ? h + '0' : h - 10 + 'a';
        dest[len-i-1] = c;
    }
}

void print_line(line_t *line)
{
    char buf[64];

    /* line format: 0xHHH: cccccccccccc | <line> */
    if (line->type == TYPE_INS) {
        bin_t *y64bin = &line->y64bin;
        int i;
        
        strcpy(buf, "  0x000:                      | ");
        
        hexstuff(buf+4, y64bin->addr, 3);
        if (y64bin->bytes > 0)
            for (i = 0; i < y64bin->bytes; i++)
                hexstuff(buf+9+2*i, y64bin->codes[i]&0xFF, 2);
    } else {
        strcpy(buf, "                              | ");
    }

    printf("%s%s\n", buf, line->y64asm);
}

/* 
 * print_screen: dump readable binary and assembly code to screen
 * (e.g., Figure 4.8 in ICS book)
 */
void print_screen(void)
{
    line_t *tmp = line_head->next;
    while (tmp != NULL) {
        print_line(tmp);
        tmp = tmp->next;
    }
}

/* init and finit */
void init(void)
{
    reltab = (reloc_t *)malloc(sizeof(reloc_t)); // free in finit
    memset(reltab, 0, sizeof(reloc_t));

    symtab = (symbol_t *)malloc(sizeof(symbol_t)); // free in finit
    memset(symtab, 0, sizeof(symbol_t));

    line_head = (line_t *)malloc(sizeof(line_t)); // free in finit
    memset(line_head, 0, sizeof(line_t));
    line_tail = line_head;
    lineno = 0;
}

void finit(void)
{
    reloc_t *rtmp = NULL;
    do {
        rtmp = reltab->next;
        if (reltab->name) 
            free(reltab->name);
        free(reltab);
        reltab = rtmp;
    } while (reltab);
    
    symbol_t *stmp = NULL;
    do {
        stmp = symtab->next;
        if (symtab->name) 
            free(symtab->name);
        free(symtab);
        symtab = stmp;
    } while (symtab);

    line_t *ltmp = NULL;
    do {
        ltmp = line_head->next;
        if (line_head->y64asm) 
            free(line_head->y64asm);
        free(line_head);
        line_head = ltmp;
    } while (line_head);
}

static void usage(char *pname)
{
    printf("Usage: %s [-v] file.ys\n", pname);
    printf("   -v print the readable output to screen\n");
    exit(0);
}

int main(int argc, char *argv[])
{
    int rootlen;
    char infname[512];
    char outfname[512];
    int nextarg = 1;
    FILE *in = NULL, *out = NULL;
    
    if (argc < 2)
        usage(argv[0]);
    
    if (argv[nextarg][0] == '-') {
        char flag = argv[nextarg][1];
        switch (flag) {
          case 'v':
            screen = TRUE;
            nextarg++;
            break;
          default:
            usage(argv[0]);
        }
    }

    /* parse input file name */
    rootlen = strlen(argv[nextarg])-3;
    /* only support the .ys file */
    if (strcmp(argv[nextarg]+rootlen, ".ys"))
        usage(argv[0]);
    
    if (rootlen > 500) {
        err_print("File name too long");
        exit(1);
    }
 

    /* init */
    init();

    
    /* assemble .ys file */
    strncpy(infname, argv[nextarg], rootlen);
    strcpy(infname+rootlen, ".ys");
    in = fopen(infname, "r");
    if (!in) {
        err_print("Can't open input file '%s'", infname);
        exit(1);
    }
    
    if (assemble(in) < 0) {
        err_print("Assemble y64 code error");
        fclose(in);
        exit(1);
    }
    fclose(in);


    /* relocate binary code */
    if (relocate() < 0) {
        err_print("Relocate binary code error");
        exit(1);
    }


    /* generate .bin file */
    strncpy(outfname, argv[nextarg], rootlen);
    strcpy(outfname+rootlen, ".bin");
    out = fopen(outfname, "wb");
    if (!out) {
        err_print("Can't open output file '%s'", outfname);
        exit(1);
    }

    if (binfile(out) < 0) {
        err_print("Generate binary file error");
        fclose(out);
        exit(1);
    }
    fclose(out);
    
    /* print to screen (.yo file) */
    if (screen)
       print_screen(); 

    /* finit */
    finit();
    return 0;
}


