// Michael Harris
// HW4 PL/0 machine version 1.0
// COP3402

/*
For a full explanation of each feature of the compiler, see the hard copy User Guide.

To compile and run the PL/0 machine, execute the following at a command prompt:

	gcc hw4compiler.c -o compile && ./compile [args] input.txt

Optional arguments include: (in any order)
	-l  ::  print scanner output to screen
	-a	::	print parser output to screen
	-v	::	print vm output to screen

Required arguments include:
	input.txt	::	valid PL/0 code file
					(must be passed as LAST argument)


As an example, to compile and execute the code within "input.txt", and show all
output from various stages of the machine, run:

	gcc hw4compiler.c -o compile && ./compile -l -a -v input.txt

As a side note, this program will generate a text file called "tokens.txt" for internal usage,
so be sure to have write permissions in the working directory. If this is not the case, an error will display.

Please also note, if both -l and -a flags are chosen, the lexeme list will be printed twice. This is apart of each
project specification and not a duplication of output! */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_STR_LEN 11
#define MAX_INT_LEN 5
#define DEBUG 0 //for scnr
#define MAX_STACK_HEIGHT 2000
#define MAX_CODE_LENGTH 500
#define MAX_LEXI_LEVELS 3
#define INIT_SP_VAL 0
#define INIT_BP_VAL 1
#define INIT_PC_VAL 0
#define INIT_IR_VAL 0
#define NUM_REGISTERS 16
#define LIT 1
#define RTN 2
#define LOD 3
#define STO 4
#define CAL 5
#define INC 6
#define JMP 7
#define JPC 8
#define SIO 9
#define NEG 10
#define ADD 11
#define SUB 12
#define MUL 13
#define DIV 14
#define ODD 15
#define MOD 16
#define EQL 17
#define NEQ 18
#define LSS 19
#define LEQ 20
#define GTR 21
#define GEQ 22

//Data structures
typedef struct instruction
{
    int op;
    int r;
    int l;
    int m;
}instruction;

typedef struct symbol
{
    int kind; //const=1, var=2, proc=3
    char name[12];
    int val;
    int level; //L
    int adr; //offset from base, first four values are FV, SL, DL, RA
    int mark; //always 0 for HW4, using a different implementation to implement variable scoping
}symbol;

typedef struct node {
    int lexval; //lexical value
    int litval; //literal value
    char varname[MAX_STR_LEN + 1];
}lex_rec;

char *instr[23] = { "NOP", "LIT", "RTN", "LOD", "STO", "CAL", "INC", "JMP", "JPC", "SIO",
"NEG", "ADD", "SUB", "MUL", "DIV", "ODD", "MOD", "EQL", "NEQ", "LSS", "LEQ", "GTR", "GEQ" };

char *instr_low[23] = { "nop", "lit", "rtn", "lod", "sto", "cal", "inc", "jmp", "jpc", "sio",
"neg", "add", "sub", "mul", "div", "odd", "mod", "eql", "neq", "lss", "leq", "gtr", "geq" };

// custom and provided data types
char *tokens[33] = { "null",  "identifier", "number", "+", "-", "*", "/", "odd", "=", "<>", "<", "<=",
                     ">", ">=", "(", ")", ",", ";", ".", ":=", "begin", "end", "if", "then", "while",
                     "do", "call", "const", "var", "procedure", "write", "read", "else" };

char *tokens_hw3[33] = { "nulsym",  "identsym", "numbersym", "plussym", "minussym", "multsym", "slashsym",
                         "oddsym", "eqsym", "neqsym", "lessym", "leqsym", "gtrsym", "geqsym", "lparentsym",
                         "rparentsym", "commasym", "semicolonsym", "periodsym", "becomessym", "beginsym",
                         "endsym", "ifsym", "thensym", "whilesym", "dosym", "calsym", "constsym", "varsym",
                         "procsym", "writesym", "readsym", "elsesym" };

typedef enum {
nulsym = 1, identsym, numbersym, plussym, minussym, multsym, slashsym, oddsym, eqsym, neqsym, lessym, leqsym,
gtrsym, geqsym, lparentsym, rparentsym, commasym, semicolonsym, periodsym, becomessym, beginsym, endsym, ifsym,
thensym, whilesym, dosym, callsym, constsym, varsym, procsym, writesym, readsym, elsesym } token_type;

enum STATE { START, BUILD_STR, BUILD_INT, BUILD_SPL, CHECK_COMMENT, IN_ILLEGAL, IN_COMMENT };	//states used for tokenizer

//hw4 global
int level=0; //set level at 0 initially... should be 1 the first time block is ran, increment when entering block


//hw3 globals
instruction code[MAX_CODE_LENGTH];	//hold assembly instructions
symbol sym_t[MAX_CODE_LENGTH/2];	//symbol table used in parser	
int code_index, reg_ptr, st_index=1, end_of_file, parse_error=0, show_scnr, show_prsr, show_vm;
FILE *ifp = NULL;
char token[12];
char show_code[MAX_CODE_LENGTH*5];	//only used for printing out code
int count_code;						//index for above

//hw2 global variables
lex_rec LL[MAX_CODE_LENGTH];                            //lexeme list which will hold  records (lex_rec struct)
int ll_index=0, in_index=0, error=0, errorFree=1;//these indexes will hold the current position in each array, and error will be reset when build_error() is called
char in[MAX_STR_LEN + 1], char1;             //in is our buffer to build a string or number, char1 holds the first position when we need to build a 2 char symbol
                                             //in needs to be of size max(MAX_STR_LEN+1, MAX_INT_LEN+1)

//hw1 globals
// register file, stack, array of instructions to hold our code, and instruction register
// declared as global because they are always in scope, and this initializes all values to 0 by default
int rf[NUM_REGISTERS];		//registers used in the vm
int stack[MAX_STACK_HEIGHT];//hold activation records
instruction ir;				//ir that will hold our current instruction


// prototypes for parser
void getToken();										//get the next tokenized representation, store it in variable 'token'
void emit(int op, int r, int l, int m);					//generate an assembly instruction with opcode, R L and M components
void insert(int kind, char *name, int val, int adr);	//insert an identifer into the symbol table	
int find(char *name);									//returns index of a match in the symbol table, or 0
void show_tokens_hw3();									//show output, also called when an error happens
void error_stop(int e);									//generate a parsing error and halt

void program();											//routines for each syntactic class	
void block();
void statement();
void condition();
int relop();
void expression();
void term();
void factor();

// function declarations for scanner
int scnr(char *name); //pass filename of file to tokenize, produces tokens.txt if no errors
enum STATE run_DFA(enum STATE state, char c);//run the DFA, once for each character and once at the end to flush what's in the buffer
enum STATE start(char c);                    //start
enum STATE build_str(char c);                //building a string
enum STATE build_int(char c);                //building an integer
enum STATE build_spl(char c);                //builds a special character in the form an int == isSpecialSymbol(char1, c) for a given c (previously detected char1)
enum STATE check_comment(char c);            //once a / is found, see if we are going to be inside a comment. if not, handle the / and the new character
enum STATE in_illegal(char c);               //if we're inside an illegal variable, we'll need to wait to find some whitespace or an operator before breaking out
enum STATE in_comment(char c);               //keeps us inside a comment until we're allowed to break out
int isWhiteSpace (char c);                   //returns 1 for \t, \n, or ' '
int isReservedWord(char* word);              //provides a lexical representation of a reserved word
int isSpecialSymbol(char c, char nextC);     //determines if a single char or a pair of characters is a special symbol
int isSingleSpecialSymbol(char c);           //similar to above but ONLY for simple symbols which only consist of one char
int isValid(char c);                         //determine if we are looking at a legal character or not. decides whether error gets generated
int isC(char c);                             //valid alphabet character
int isI(char c);                             //valid digit character
void printProgram(FILE *ifp, char *s);       //print the program, char by char according to the provided specification
void printLexemeTable();                     //print each token and it's tokenized value
void printLexemeList();                      //print the tokenized lexeme list, seperated by spaces
void build_error(char c);                    //displays an error to the screen based on the error code. resets error flag back to 0.
void generate_lexrec();                      //does some heavy lifting to detect a reserved word, or generate a variable/literal and prepare for a new run
void finish(enum STATE state, char c);       //finalize the lexical list by checking any buffers and generating a lexical record

// prototypes for vm
int vm();						//runs the vm, returns 0 or halt code
int base(int l, int base);		//used for going down L records to find an AR
void printStack(int sp, int bp, int* stack, int lex);	//used for stack trace in hw1

// getToken function, uses %s to handle whitespace. end_of_file used for testing purposes only
void getToken()
{
    if (fscanf(ifp, " %s", token) == EOF)
        end_of_file = 1;
}

// generate an assembly instruction and move code_index forward by one
void emit(int op, int r, int l, int m)
{
    code[code_index].op = op;
    code[code_index].r = r;
    code[code_index].l = l;
    code[code_index++].m = m;

    if (code_index >= MAX_CODE_LENGTH)
        error_stop(31);
}

// insert a record into the symbol table. mark is initially 0, and level is 0 for HW3
void insert(int kind, char *name, int val, int adr)
{
    sym_t[st_index].kind = kind;
    strcpy(sym_t[st_index].name, name);
    sym_t[st_index].val = val;
    sym_t[st_index].level = 0; //HW3
    sym_t[st_index].adr = adr;
    sym_t[st_index++].mark = 0;

    if (st_index >= MAX_CODE_LENGTH/2)
        error_stop(32);
}

// find an item in the symbol table based on name, return index
int find(char *name)
{
    int i;

    for(i=st_index-1; i>=0; i--)
    {
        if (strcmp(sym_t[i].name, name) == 0)
            return i;
    }
    return 0;
}

// output for hw3
void show_tokens_hw3()
{
    int i;

    if (show_prsr == 1) printf("\nTokens:\n");
    //lexeme list from HW2, needs to be apart of HW3 out
    for (i=0; i<ll_index; i++)
    {
        if (show_prsr == 1) printf("%d ", LL[i].lexval);
        if (LL[i].lexval == identsym)
            if (show_prsr == 1) printf("%s ", LL[i].varname);
        if (LL[i].lexval == numbersym)
            if (show_prsr == 1) printf("%d ", LL[i].litval);
    }
    if (show_prsr == 1) printf("\n");

    if (show_prsr == 1) printf("\nSymbolic representation:\n");
    //lexeme list from HW2, converted to symbolic representations. needs to be apart of HW3 out
    for (i=0; i<ll_index; i++)
    {
        if (show_prsr == 1) printf("%s ", tokens_hw3[LL[i].lexval-1]);
        if (LL[i].lexval == identsym)
            if (show_prsr == 1) printf("%s ", LL[i].varname);
        if (LL[i].lexval == numbersym)
            if (show_prsr == 1) printf("%d ", LL[i].litval);
    }
    if (show_prsr == 1) printf("\n\n");
}

// main function: handle arguments, run the scanner, parse tokens, generate and execute instructions
int main(int argc, char** argv)
{
    int i;

    // handle arguments
    for(i=1; i<argc-1; i++)
    {
        if (strcmp(argv[i], "-l") == 0)
            show_scnr = 1;
        if (strcmp(argv[i], "-a") == 0)
            show_vm = 1;
        if (strcmp(argv[i], "-v") == 0)
            show_prsr = 1;
    }

    ///show_scnr=1, show_vm=1, show_prsr=1;

    scnr(argv[argc-1]); //tokenize     // "testing.txt" // argv[argc-1]
    ifp = fopen("tokens.txt", "r");
    if (ifp == NULL) printf("error opening tokens.txt - check write permissions of working directory\n");

    // call the program block and follow grammar for parsing
    program();

    // generate an exit instruction once we're done and show output. run vm if we're error free
    if (parse_error == 0)
    {
        emit(9, 0, 0, 3);

        if (show_prsr == 1) show_tokens_hw3();

        if (show_prsr == 1) printf("No errors, program is syntactically correct\n");

        if (show_prsr == 1) printf("\n");
        for (i=0; i<code_index; i++)
            if (show_prsr == 1) printf("%d %d %d %d\n", code[i].op, code[i].r, code[i].l, code[i].m);

        vm(); //execute! (uses same code data structure as HW3)
    }


    fclose(ifp);

    return 0;
}





void program()
{
    // program ::= block "."

    //get first token, call block
    getToken();
    block(); if (parse_error == 1) return;

    //should be looking at a period here or throw error
    if (strcmp(token, "19") != 0)
    {
        error_stop(9);
        return;
    }
}

void block()
{   // block ::= [const-dec] [var-dec] {proc-dec} statement .
    if (parse_error == 1) return;
    if (++level > MAX_LEXI_LEVELS+1)
    {
        error_stop(30); //first custom error, starting at 30
        return;
    }

    int litval;
    char varname[12];


    //room for FV, SL, DL, RA. start putting constants after this, then variables
    int reserved = 4; 
	
    int procedure_index, previous_symt_idx = st_index;

    int jump_adr = code_index;

    emit(7, 0, 0, 0);





    //const-dec ::= [ "const" ident "=" number {"," ident "=" number} ";"] .
    if (strcmp(token, "28") == 0) //const
    {
        if (parse_error == 1) return;
        do {

            getToken();

            //should have 2 so that we are ready for an ident name
            if (strcmp(token, "2") != 0)
            {
                error_stop(4);
                return;
            }

            getToken();

            strcpy(varname, token);

            getToken();

            //check for = here
            if (strcmp(token, "9") != 0)
            {
                error_stop(3);
                return;
            }

            getToken();

            //prepare for a number, 3 then value
            if (strcmp(token, "3") != 0)
            {
                error_stop(2);
                return;
            }

            getToken();

            //enter the constant (1) into the symbol table, with the var name and value. address is 0
            insert(1, varname, atoi(token), 0);

            getToken();
        } while (strcmp(token, "17") == 0); //loop as long as we have a comma, indcating more declarations

        //should have a semicolon once we finish or error
        if (strcmp(token, "18") != 0)
        {
            error_stop(5);
            return;
        }

        getToken();
    }//end constdec

    //var-dec ::= [ "var" ident {"," ident} ";" ] .
    if (strcmp(token, "29") == 0) //var
    {
        if (parse_error == 1) return;
        do {
            getToken();

            if (strcmp(token, "2") != 0) //check ident
            {
                error_stop(4);
                return;
            }

            getToken();

            //insert into ST, with default value of 0. address starts at 5, leaving room for FV, DL, SL, RA
            insert(2, token, 0, ++reserved);
            sym_t[st_index-1].level = level; //use whatever level we're at (level of nested block()s)

            getToken();
        } while (strcmp(token, "17") == 0); //loop as long as we match a comma, indicating further declarations

        //should have a semicolon once we finish or error
        if (strcmp(token, "18") != 0)
        {
            error_stop(5);
            return;
        }

        getToken();
    }//end var-dec

    //proc-dec ::= { "procedure" ident ";" block ";" } .
    while (strcmp(token, "30") == 0) //procsym
    {
        getToken();

        if (strcmp(token, "2") != 0) //check ident
            {
                error_stop(4);
                return;
            }

        getToken();

        //token now has the value of the procedure name, insert into ST
        insert(3, token, 0, jump_adr+1); //use address of instruction after JMP location for address
        sym_t[st_index-1].level = level; //set level for proc as current level

        getToken();

        if (strcmp(token, "18") != 0) //need semicolon after identifier
        {
            error_stop(5);
            return;
        }

        getToken();

        block(); if (parse_error == 1) return;

        if (strcmp(token, "18") != 0) //need semicolon after identifier
        {
            error_stop(5);
            return;
        }

        getToken();
    }

    code[jump_adr].m = code_index;
    emit(6, 0, 0, reserved);


    statement(); if (parse_error == 1) return;

    if (level>1) //without this my program crashes... seems intuitive. only generate return if we are inside an actual level (level>0)
        emit(2, 0, 0, 0);

    //easy way to make sure all variables are in scope, just undo any additions in this block when exiting
    st_index = previous_symt_idx;

    //exiting block, decrement level
    level--;

}

void statement()
{
    if (parse_error == 1) return;
    //statement ::=

    int st_idx=0, cx_temp=0, cx_temp2=0;

    // [ <ident> ":=" <expression> ]
    if (strcmp(token, "2") == 0)
    {
        getToken();

        st_idx = find(token);
        if (st_idx == 0)
        {
            error_stop(11);
            return;
        } //undeclared
        else if (sym_t[st_idx].kind == 1)
        {
            error_stop(12);
            return;
        } //assign to const

        getToken();

        if (strcmp(token, "20") != 0)
        {
            error_stop(3);
            return;
        } //need :=

        getToken();



        expression(); if (parse_error == 1) return;

        // store result
        emit(4, reg_ptr-1, sym_t[st_idx].level, sym_t[st_idx].adr); //add level from sym_t for HW4

        // storing, decrement register pointer
        reg_ptr--;
    }


    // | "call" ident
    else if (strcmp(token, "27") == 0)
    {
        getToken();

        if (strcmp(token, "2") != 0)
        {
            error_stop(14);
            return;
        }

        getToken();

        st_idx = find(token);
        if (st_idx == 0)
        {
            error_stop(11);
            return;
        } //undeclared

        if (sym_t[st_idx].kind == 3) //valid process identifier
        {
            //process a valid procsym identifier by emitting a CAL instruction
            emit(5, 0, level - sym_t[st_idx].level, sym_t[st_idx].adr); //calculate how many levels need to go from here
        }
        else    //call needs ident(procsym)
        {
            error_stop(14);
            return;
        }

        getToken();
    }


    // | "begin" <statement> { ";" <statement> } "end"
    else if (strcmp(token, "21") == 0)
    {
        getToken();

        statement(); if (parse_error == 1) return;

        while (strcmp(token, "18") == 0) //semicolon
        {
            getToken();

            statement(); if (parse_error == 1) return;
        }

        if (strcmp(token, "22") != 0) //endsym
        {
            error_stop(19);
            return;
        } //end expected

        getToken();
    }


    // | "if" <condition> "then" <statement> [ "else" statement]
    else if (strcmp(token, "23") == 0) //ifsym
    {
        getToken();

        condition(); if (parse_error == 1) return;

        if (strcmp(token, "24") != 0) //thensym
        {
            error_stop(16);
            return;
        }

        getToken();

        cx_temp = code_index;

        // generate jump instruction, come back and insert the code index once known
        emit(8, reg_ptr-1, 0, 0);

        statement(); if (parse_error == 1) return;

        //handling else
        if (strcmp(token, "33") == 0)
        {
            cx_temp2 = code_index;

            emit(7, 0, 0, 0);

            code[cx_temp].m = code_index;

            getToken();

            statement(); if (parse_error == 1) return;

            code[cx_temp2].m = code_index;
        }
        else //no 'else'
        {
            code[cx_temp].m = code_index;
        }


        // update jpc with code index
        code[cx_temp].m = code_index;

        // needed for condition, decrement
        reg_ptr--;
    }


    // | "while" <condition> "do" <statement>
    else if (strcmp(token, "25") == 0)
    {
        cx_temp = code_index;

        getToken();

        condition(); if (parse_error == 1) return;

        cx_temp2 = code_index;

        // setup condition
        emit(8, reg_ptr-1, 0, 0);

        if (strcmp(token, "26") != 0)
        {
            error_stop(18);
            return;
        } //do expected

        getToken();

        statement(); if (parse_error == 1) return;

        // setup loop
        emit(7, 0, 0, cx_temp);

        // update jump address with pc
        code[cx_temp2].m = code_index;

        // cleanup after condition
        reg_ptr--;
    }


    // | "read" <ident>
    else if (strcmp(token, "32") == 0) //readsym
    {
        getToken();

        if (strcmp(token, "2") != 0)
        {
            error_stop(23);
            return;
        } //identifier expected

        getToken();

        st_idx = find(token);

        if (st_idx == 0)
        {
            error_stop(11);
            return;
        } //undeclared

        // read
        emit(9, reg_ptr, 0, 2);

        if (sym_t[st_idx].kind == 1)
        {
            error_stop(12);
            return;
        } //assign to constant
        else if (sym_t[st_idx].kind == 2)
            // store
            emit(4, reg_ptr--, sym_t[st_idx].level, sym_t[st_idx].adr); //adding level for HW4

        getToken();
    }


    // | "write" <ident>
    else if (strcmp(token, "31") == 0)
    {
        getToken();

        if (strcmp(token, "2") != 0)
        {
            error_stop(23);
            return;
        } //identifier expected

        getToken();

        st_idx = find(token);

        if (st_idx == 0)
        {
            error_stop(11);
            return;
        } //undeclared

        if (sym_t[st_idx].kind == 1)
        {
            emit(1, reg_ptr, 0, sym_t[st_idx].val); // put into register, constants are always in scope for PL/0
            emit(9, reg_ptr, 0, 1);
        }
        else if (sym_t[st_idx].kind == 2)
        {
            emit(3, reg_ptr, sym_t[st_idx].level, sym_t[st_idx].adr); // get from memory, adding level for HW4
            emit(9, reg_ptr, 0, 1);
        }

        getToken();
    }

}

void condition()
{
    if (parse_error == 1) return;
    //condition ::= "odd" <expression> | <expression> <rel-op> <expression> .
    int opcode=0;

    //starting "odd"
    if (strcmp(token, "8") == 0)
    {
        getToken();

        expression(); if (parse_error == 1) return;

        //evaluate the odd condition
        emit(15, reg_ptr-1, reg_ptr-1, 0);
    }
    //starting <expression>
    else
    {
        expression();

        opcode = relop();
        if (opcode == 0)
        {
            error_stop(20);
            return;
        }

        getToken();

        expression(); if (parse_error == 1) return;

        // generate the instruction with the registers it will need
        emit(opcode, reg_ptr-2, reg_ptr-2, reg_ptr-1);

        // cleanup after op
        reg_ptr--;
    }
}

int relop()
{
    // return opcode for associated symbol or 0 for no match

    if (strcmp(token, "9") == 0) //=
        return 17;
    else if (strcmp(token, "10") == 0) //<>
        return 18;
    else if (strcmp(token, "11") == 0) //<
        return 19;
    else if (strcmp(token, "12") == 0) //<=
        return 20;
    else if (strcmp(token, "13") == 0) //>
        return 21;
    else if (strcmp(token, "14") == 0) //>=
        return 22;
    else
        return 0;
}

void expression()
{
    if (parse_error == 1) return;
    // expression ::= ["+"|"-"] <term> { ("+"|"-") <term> } .
    char temp[3];

    if ((strcmp(token, "4") == 0) || (strcmp(token, "5") == 0))
    {
        strcpy(temp, token);

        getToken();

        term(); if (parse_error == 1) return;

        //if negating a term, push this instruction
        if (strcmp(temp, "5") == 0)
            emit(10, reg_ptr-1, reg_ptr-1, 0);
    }
    else
        term(); if (parse_error == 1) return;

    //almost same as above, except handling an optional 0 or more times
    while ((strcmp(token, "4") == 0) || (strcmp(token, "5") == 0))
    {
        strcpy(temp, token);

        getToken();

        term(); if (parse_error == 1) return;

        //handle addition
        if (strcmp(temp, "4") == 0)
        {
            emit(11, reg_ptr-2, reg_ptr-2, reg_ptr-1);
            reg_ptr--;
        }

        //handle subtraction
        else
        {
            emit(12, reg_ptr-2, reg_ptr-2, reg_ptr-1);
            reg_ptr--;
        }
    }
}

void term()
{
    if (parse_error == 1) return;
    //term ::= <factor> { ("*"|"/") <factor> } .
    char temp[3];

    factor(); if (parse_error == 1) return;

    while ((strcmp(token, "6") == 0) || (strcmp(token, "7") == 0)) //match a * or /
    {
        strcpy(temp, token);

        getToken();

        factor(); if (parse_error == 1) return;

        if (strcmp(temp, "6") == 0)
        {
            //handle multiplication
            emit(13, reg_ptr-2, reg_ptr-2, reg_ptr-1);
            reg_ptr--;
        }
        else
        {
            //handle division
            emit(14, reg_ptr-2, reg_ptr-2, reg_ptr-1);
            reg_ptr--;
        }
    }
}

void factor()
{
    if (parse_error == 1) return;
    //factor ::= <ident> | <number> | "(" <expression> ")" .
    int st_idx=0;

    //check starting ident
    if (strcmp(token, "2") == 0)
    {
        getToken();

        //find index of ST record if it exists, if it hits 0 we know there is no match
        st_idx = find(token);
        if (st_idx == 0)
        {
            error_stop(11);
            return;
        }

        //put ident in register file stack - rf[r]<-m
        if (sym_t[st_idx].kind == 1)
            emit(1, reg_ptr++, 0, sym_t[st_idx].val);
        //load from memory, use address in ST
        else if (sym_t[st_idx].kind == 2)
            emit(3, reg_ptr++, sym_t[st_idx].level, sym_t[st_idx].adr); // update to use level for HW4

        getToken();
    }

    //check starting number
    else if (strcmp(token, "3") == 0)
    {
        getToken();

        // place on stack and update rp
        emit(1, reg_ptr++, 0, atoi(token));

        getToken();
    }

    //check starting "(" <expression> ")"
    else if (strcmp(token, "15") == 0)
    {
        getToken();

        expression(); if (parse_error == 1) return;

        if (strcmp(token, "16") != 0)
        {
            error_stop(22);
            return;
        }

        getToken();
    }

    //if we're here, something doesn't make sense
    else
        error_stop(23);
}

void error_stop(int error)
{
    int i;
//printf("\n\tDEBUGGING:%s, %d\n", token, code_index);
    if (show_prsr == 1) show_tokens_hw3();

    for (i=0; i<count_code; i++)
        if (show_prsr == 1) printf("%c", show_code[i]);
    if (show_prsr == 1) printf("\n");

    printf("  ***** Error number %d - ", error);

    if (error == 1)
        printf("Use = instead of :=");
    else if (error == 2)
        printf("= must be followed be a number");
    else if (error == 3)
        printf("Identifier must be followed by =");
    else if (error == 4)
        printf("const, var, procedure must be followed by identifier");
    else if (error == 5)
        printf("Semicolon or comma missing");
    else if (error == 6)
        printf("Incorrect symbol after procedure declaration");
    else if (error == 7)
        printf("Statement expected");
    else if (error == 8)
        printf("Incorrect symbol after statement part in block");
    else if (error == 9)
        printf("Period expected");
    else if (error == 10)
        printf("Semicolon between statements missing");
    else if (error == 11)
        printf("Undeclared identifier");
    else if (error == 12)
        printf("Assignment to constant or procedure is not allowed");
    else if (error == 13)
        printf("Assignment operator expected");
    else if (error == 14)
        printf("call must be followed by an identifier");
    else if (error == 15)
        printf("Call of a constant or variable is meaningless");
    else if (error == 16)
        printf("then expected");
    else if (error == 17)
        printf("Semicolon or end expected");
    else if (error == 18)
        printf("do expected");
    else if (error == 19)
        printf("Incorrect symbol following statement");
    else if (error == 20)
        printf("Relational operator expected");
    else if (error == 21)
        printf("Expression must not constain a procedure identifier");
    else if (error == 22)
        printf("Right parenthesis missing");
    else if (error == 23)
        printf("Invalid identifier");
    else if (error == 24)
        printf("An expression cannot begin with this symbol");
    else if (error == 25)
        printf("Number too large");
    else if (error == 26)
        printf("Identifier too long");
    else if (error == 27)
        printf("Invalid symbol");
    else if (error == 30)
        printf("Cannot nest deeper than %d", MAX_LEXI_LEVELS);
    else if (error == 31)
        printf("Stack overflow, cannot be deeper than %d", MAX_CODE_LENGTH);
    else if (error == 32)
        printf("Symbol table overflow, cannot be more than %d symbols", MAX_CODE_LENGTH/2);
    else if (error == 33)
        printf("No code found - check the User Guide for how to write PL/0 code");

    printf("\n");

    parse_error=1;
}


// scanner functions:

int scnr(char *name)
{
    char c;
    enum STATE state = START;
    FILE *ifp2 = fopen(name, "r");

    if (ifp2 == NULL)
    {
        printf("Please pass a valid PL/0 code file as the final parameter.\n");
        exit(1);
    }

    printProgram(ifp2, name);
    rewind(ifp2);

    // basic deterministic finite state machine - process characters for the state we are in
    while(fscanf(ifp2, "%c", &c) != EOF)
        state = run_DFA(state, c);
    finish(state, c);

    fclose(ifp2);

    if (ll_index == 0)
    {
        error_stop(33);
        exit(1);
    }

    if (errorFree)
        printLexemeTable(); //this also prints the lexeme list

    return 0;
}

enum STATE run_DFA(enum STATE state, char c)
{
    if (state == START)
        return start(c);
    else if (state == BUILD_STR)
        return build_str(c);
    else if (state == BUILD_INT)
        return build_int(c);
    else if (state == BUILD_SPL)
        return build_spl(c);
    else if (state == CHECK_COMMENT)
        return check_comment(c);
    else if (state == IN_ILLEGAL)
        return in_illegal(c);
    else if (state == IN_COMMENT)
        return in_comment(c);
};

void finish(enum STATE state, char c)
{
    // just in case anything is still in the buffer or in the char1 holding slot (prevents rerunning a char already processed)
    if (in_index != 0 || char1 != 0)
        // passing a blank char here to not cause a duplication of the last char if no terminating whitespace
        run_DFA(state, ' ');

    generate_lexrec();
}

void generate_lexrec()
{
    //null terminate buffer string so we can use strcpy and atoi
    in[in_index] = '\0';

    if (in_index != 0)
        if (isReservedWord(in))
        {
            LL[ll_index++].lexval = isReservedWord(in);
        }
        else if (isC(in[0]))
        {
            LL[ll_index].lexval = identsym;
            strcpy(LL[ll_index++].varname, in);
        }
        else if (isI(in[0]))
        {
            LL[ll_index].lexval = numbersym;
            LL[ll_index++].litval = atoi(in);
        }
    else
        ;//empty token, no need to do anything

    // reset input string buffer and position counter before processing anything else
    strcpy(in, "");
    in_index = 0;
}

void printProgram(FILE *ifp2, char *s)
{
    char c;

    if (show_scnr == 1) printf("Source Program:%s\n", s);
    while(fscanf(ifp2, "%c", &c) != EOF)
    {
        if (show_scnr == 1) printf("%c", c);
        show_code[count_code++] = c;
    }
    if (show_scnr == 1) printf("\n");
}

void printLexemeTable()
{
    int i;

    if (show_scnr == 1) printf("\nLexeme Table:\n");
    if (show_scnr == 1) printf("lexeme\t\ttoken type\n");
    for (i=0; i<ll_index; i++)
    {
        if (LL[i].lexval == identsym)
        {
            if (show_scnr == 1) printf("%s\t\t%d\n", LL[i].varname, LL[i].lexval);
        }
        else if (LL[i].lexval == numbersym)
        {
            if (show_scnr == 1) printf("%d\t\t%d\n", LL[i].litval, LL[i].lexval);
        }
        else
        {
            if (show_scnr == 1) printf("%s\t\t%d\n", tokens[LL[i].lexval - 1], LL[i].lexval);
        }
    }
    printLexemeList();
}

void printLexemeList()
{
    int i;
    FILE *lex_out = fopen("tokens.txt", "w");

    if (show_scnr == 1) printf("\nLexeme List:\n");
    for (i=0; i<ll_index; i++)
    {
        if (show_scnr == 1) printf("%d ", LL[i].lexval);
        fprintf(lex_out, "%d ", LL[i].lexval);

        if (LL[i].lexval == identsym)
        {
            if (show_scnr == 1) printf("%s ", LL[i].varname);
            fprintf(lex_out, "%s ", LL[i].varname);
        }
        if (LL[i].lexval == numbersym)
        {
            if (show_scnr == 1) printf("%d ", LL[i].litval);
            fprintf(lex_out, "%d ", LL[i].litval);
        }
    }
    if (show_scnr == 1) printf("\n");
    fclose(lex_out);
}

int isC(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int isI(char c)
{
    return (c >= '0' && c <= '9');
}

int isValid(char c)
{
    return (isWhiteSpace(c) || isC(c) || (c >= '(' && c <= '>'));
}

int isWhiteSpace(char c)
{
    return (c==' ' || c=='\t' || c=='\n' || c=='\v' || c=='\f' || c=='\r'); //ascii values 11 and 13 showed up in the test files so just being safe
}

enum STATE start(char c)
{
    enum STATE tempState;

    if (isC(c))
    {
        in[in_index++] = c;
        tempState = BUILD_STR;
    }
    else if (isI(c))
    {
        in[in_index++] = c;
        tempState = BUILD_INT;
    }
    else if (c == '/')
    {
        generate_lexrec();
        tempState = CHECK_COMMENT;
    }
    else if (!isValid(c))
    {
        error = 27;
        build_error(c);
        tempState = START;
    }
    else if (isWhiteSpace(c))
    {
        generate_lexrec();
        tempState = START;
    }
    else if (isSingleSpecialSymbol(c))
    {
        generate_lexrec();
        tempState = START;
        LL[ll_index++].lexval = isSingleSpecialSymbol(c);
    }
    else if (isSpecialSymbol(c, '=') || isSpecialSymbol(c, '>'))
    {
        generate_lexrec();
        tempState = BUILD_SPL;
        char1 = c;
    }
    else
    {
        if (DEBUG)
        {
        error = -1;
        build_error(c);
        }
        tempState = START;
    }

    return tempState;
};

enum STATE build_str(char c)
{
    enum STATE tempState;

    if (in_index > MAX_STR_LEN)
    {
        error = 26;
        build_error(c);
        in_index = 0;
        tempState = START;
    }

    if (isC(c) || isI(c))
    {
        tempState = BUILD_STR;
        in[in_index++] = c;
    }
    else if (c == '/')
    {
        generate_lexrec();
        tempState = CHECK_COMMENT;
    }
    else if (!isValid(c))
    {
        error = 27;
        build_error(c);
        tempState = START;
    }
    else if (isSingleSpecialSymbol(c))
    {
        generate_lexrec();
        tempState = START;
        LL[ll_index++].lexval = isSingleSpecialSymbol(c);
    }
    else if (isSpecialSymbol(c, '=') || isSpecialSymbol(c, '>'))
    {
        generate_lexrec();
        tempState = BUILD_SPL;
        char1 = c;
    }
    else if (isWhiteSpace(c))
    {
        generate_lexrec();
        tempState = START;
    }
    else
    {
        if (DEBUG)
        {
        error = -2;
        build_error(c);
        }
        tempState = START;
    }

    return tempState;
};

enum STATE build_int(char c)
{
    enum STATE tempState;

    if (in_index > MAX_INT_LEN)
    {
        error = 25;
        build_error(c);
        in_index = 0;
        tempState = START;
    }

    if (isI(c))
    {
        in[in_index++] = c;
        tempState = BUILD_INT;
    }
    else if (isC(c))
    {
        error = 23;
        build_error(c);
        in_index = 0;
        tempState = IN_ILLEGAL;
    }
    else if (c == '/')
    {
        generate_lexrec();
        tempState = CHECK_COMMENT;
    }
    else if (isSingleSpecialSymbol(c))
    {
        generate_lexrec();
        tempState = START;
        LL[ll_index++].lexval = isSingleSpecialSymbol(c);
    }
    else if (isSpecialSymbol(c, '=') || isSpecialSymbol(c, '>'))
    {
        generate_lexrec();
        tempState = BUILD_SPL;
        char1 = c;
    }
    else if (isWhiteSpace(c))
    {
        generate_lexrec();
        tempState = START;
    }
    else
    {
        if (DEBUG)
        {
        error = -3;
        build_error(c);
        }
        tempState = START;
    }

    return tempState;
};

enum STATE build_spl(char c)
{
    enum STATE tempState = START;

    if (isSpecialSymbol(char1, c))
    {
        LL[ll_index++].lexval = isSpecialSymbol(char1, c);
        tempState = START;
    }
    else if (isSingleSpecialSymbol(char1))
    {
        // didnt match both chars, so add char1 and pass c to the start state
        LL[ll_index++].lexval = isSingleSpecialSymbol(char1);
        tempState = start(c);
    }
    else if (char1 == ':')
    {
        // this makes sure a single : throws an error, as this is only a valid symbol when followed by a =
        error = 27;
        build_error(c);
        tempState = start(c);
    }
    else if (!isValid(c))
    {
        error = 27;
        build_error(c);
        tempState = START;
    }
    else if (c == '/')
    {	//this should work but not tested yet! just comment out entirely if in doubt
        LL[ll_index++].lexval = isSingleSpecialSymbol(char1);
        tempState = CHECK_COMMENT;
    }
    else
    {
        if (DEBUG)
        {
        error = -4;
        build_error(c);
        }
        tempState = START;
    }

    in_index = 0;
    strcpy(in, "");
    char1 = 0;
    return tempState;
};

enum STATE check_comment(char c)
{
    enum STATE tempState;

    if (c == '*') // || c == '/' if // is a comment as well
    {
        tempState = IN_COMMENT;
    }
    else if (!isValid(c))
    {
        error = 27;
        build_error(c);
        tempState = START;
    }
    else
    {
        // deal with the '/' that got us here, then have our state be based on processing our new character 'c' (failed comment second character)
        LL[ll_index++].lexval = slashsym;
        tempState = start(c);
    }

    return tempState;
};

enum STATE in_illegal(char c)
{
    enum STATE tempState = IN_ILLEGAL;

    if (isC(c) || isI(c))
        ;
    else if (isWhiteSpace(c))
    {
        //inside an illegal variable, wait for whitespace for it to be over
        tempState = START;
        in_index = 0;
        strcpy(in, "");
    }
    else if (!isValid(c))
    {
        error = 27;
        build_error(c);
        tempState = START;
    }
    else
        ;

    return tempState;
};

enum STATE in_comment(char c)
{
    enum STATE tempState = IN_COMMENT;

    if (c == '*')
    {
        //looking hopeful..
        tempState = IN_COMMENT;
        char1 = '*';
    }
    else if (char1 == '*' && c == '/')
    {
        //breaking out
        tempState = START;
        char1 = 0;
    }
    else if (char1 == '*')
    {
        //reset if we've encountered a * on the last run but it didn't lead to the comment being closed (c != '/')
        char1 = 0;
    }

    return tempState;
};

//scanner errors
void build_error(char c)
{
    if (error == 0)
        return;

    errorFree = 0;

    if (DEBUG)
        printf("Error %d: ", error);
    else
        printf("Error: ");

    if (error == 23)
        printf("Invalid identifier");
    else if (error == 25)
        printf("Number too large");
    else if (error == 26)
        printf("Identifier too long");
    else if (error == 27)
        printf("Invalid symbol");
    else
        printf("unknown error, char:'%c'\tvalue:(%d)", c, c);

    printf(".\n");
    error = 0;  //flip flag for error encountered so far (HW2)
    parse_error = 1; //this isn't a parse error, but necessary for these errors to prevent continued execution once an error has occured
}

//procedure, call, else all back as reserved words for HW4
int isReservedWord(char* word)
{
    if (strcmp(word, "const") == 0)
        return constsym;
    else if (strcmp(word, "var") == 0)
        return varsym;
    else if (strcmp(word, "procedure") == 0)
        return procsym;
    else if (strcmp(word, "call") == 0)
        return callsym;
    else if (strcmp(word, "begin") == 0)
        return beginsym;
    else if (strcmp(word, "end") == 0)
        return endsym;
    else if (strcmp(word, "if") == 0)
        return ifsym;
    else if (strcmp(word, "then") == 0)
        return thensym;
    else if (strcmp(word, "else") == 0)
        return elsesym;
    else if (strcmp(word, "while") == 0)
        return whilesym;
    else if (strcmp(word, "do") == 0)
        return dosym;
    else if (strcmp(word, "read") == 0)
        return readsym;
    else if (strcmp(word, "write") == 0)
        return writesym;
    else if (strcmp(word, "odd") == 0)
        return oddsym;
    else
        return 0;
}

int isSpecialSymbol(char c, char nextC)
{
    if (c == '+')
        return plussym;
    else if (c == '-')
        return minussym;
    else if (c == '*')
        return multsym;
    else if (c == '/')
        return slashsym;
    else if (c == '=')
        return eqsym;
    else if (c == '<' && nextC == '>')
        return neqsym;
    else if (c == '<' && nextC == '=')
        return leqsym;
    else if (c == '<')
        return lessym;
    else if (c == '>' && nextC == '=')
        return geqsym;
    else if (c == '>')
        return gtrsym;
    else if (c == '(')
        return lparentsym;
    else if (c == ')')
        return rparentsym;
    else if (c == ':' && nextC == '=')
        return becomessym;
    else if (c == ',')
        return commasym;
    else if (c == ';')
        return semicolonsym;
    else if (c == '.')
        return periodsym;
    else
        return 0;
}

int isSingleSpecialSymbol(char c)
{
    if (c == '+')
        return plussym;
    else if (c == '-')
        return minussym;
    else if (c == '=')
        return eqsym;
    else if (c == '(')
        return lparentsym;
    else if (c == ')')
        return rparentsym;
    else if (c == ',')
        return commasym;
    else if (c == ';')
        return semicolonsym;
    else if (c == '.')
        return periodsym;
    else
        return 0;
}


//vm functions:




// main vm function, uses code base from hw3. simulates von neumann machine, fetch cycle, increment pc, execute cycle.
// run as long as halt==0, only an invalid opcode, running out of instructions, or setting halt to !=0 will stop execution and return 0
int vm()
{
    int bp = INIT_BP_VAL;
    int sp = INIT_SP_VAL;
    int pc = INIT_PC_VAL;
    ir.op = INIT_IR_VAL;

    int OP, R, L, M, i=0, j=0, halt=0, count=0, previousPC=INIT_PC_VAL, lex=0;

    // this line of code replaces handling file input and sets the parameter for max number of instructions
    i = code_index;

	{
		if (show_vm == 1) printf("\nExecuting Program:\n");
		for (j=0; j<i; j++)
			if (show_vm == 1) printf("%d %s %d %d %d\n", j, instr_low[code[j].op], code[j].r, code[j].l, code[j].m);

		printf("\nProgram Output and Input (if prompted)\n");
	}

    //execution loop: fetch, then execute. runs as long as halt flag is equal to 0
    while (halt == 0)
    {
        //provided formatting
        if (count==0)
            //The first line header for all the INSTRUCTION and PC, BP & SP - printf("\n OP   Rg Lx Vl[ PC BP SP]\n");
            if (show_vm == 1) printf("\n OP   Rg Lx Vl[ PC BP SP]\n");

        //start fetch cycle
        ir = code[pc++];
        //end fetch


        //start execute cycle
        if (ir.op == LIT) //01
        {
            // 01 LIT R,0,M :: loads a constant value into register R. reg[R] <- M
            rf[ir.r] = ir.m;
        }
        else if (ir.op == RTN) //02
        {
            // 02 RTN 0,0,0 :: return from a subroutine, restore caller environment
            sp = bp - 1;
            bp = stack[sp + 3];
            pc = stack[sp + 4];
            lex--;
        }
        else if (ir.op == LOD) //03
        {
            // 03 LOD R,L,M :: load value into a selected register from the stack location
            //                 at offset M from L lexicographical levels down
            rf[ir.r] = stack[ base(ir.l, bp) + ir.m ];
        }
        else if (ir.op == STO) //04
        {
            // 04 STO R,L,M :: store value from a selected register in the stack location
            //                 at offset M from L lexicographical levels down
            stack[ base(ir.l, bp) + ir.m ] = rf[ir.r];
        }
        else if (ir.op == CAL) //05
        {
            // 05 CAL 0,L,M :: call procedure at code index M, generates new AR and sets pc = M
            stack[sp + 1] = 0;                  //space to return value
            stack[sp + 2] = base(ir.l, bp);     //static link
            stack[sp + 3] = bp;                 //dynamic link
            stack[sp + 4] = pc;                 //return address
            bp = sp + 1;
            pc = ir.m;
            lex++;
        }
        else if (ir.op == INC) //06
        {
            // 06 INC 0,0,M :: allocate M locals (increment sp by M). first four are FV, SL, DL, RA.
            sp = sp + ir.m;
        }
        else if (ir.op == JMP) //07
        {
            // 07 JMP 0,0,M :: jump to instruction M
            pc = ir.m;
        }
        else if (ir.op == JPC) //08
        {
            // 08 JPC R,0,M :: jump to instruction M if R==0
            if (rf[ir.r] == 0)
                pc = ir.m;
        }
        else if (ir.op == SIO) //09
        {
            if (ir.m == 1) // write reg to screen
                printf("%d\n", rf[ir.r]);

            else if (ir.m == 2) // read in from user and store in register
                scanf(" %d", &rf[ir.r]);

            else if (ir.m == 3) // end of program (stop running)
                halt = 1;

            else
            {
                printf("Halt - Invalid M component (\"%d\") detected within SIO on line %d\n", ir.m, pc);
                halt = 1;
                break;
            }
        }
        else if (ir.op == NEG) //10
        {
            rf[ir.r] = -rf[ir.l];
        }
        else if (ir.op == ADD) //11
        {
            rf[ir.r] = rf[ir.l] + rf[ir.m];
        }
        else if (ir.op == SUB) //12
        {
            rf[ir.r] = rf[ir.l] - rf[ir.m];
        }
        else if (ir.op == MUL) //13
        {
            rf[ir.r] = rf[ir.l] * rf[ir.m];
        }
        else if (ir.op == DIV) //14
        {
            rf[ir.r] = rf[ir.l] / rf[ir.m];
        }
        else if (ir.op == ODD) //15
        {
            rf[ir.r] = rf[ir.r] % 2; //or ord(odd(R[i]))?
        }
        else if (ir.op == MOD) //16
        {
            rf[ir.r] = rf[ir.l] % rf[ir.m];
        }
        else if (ir.op == EQL) //17
        {
            rf[ir.r] = rf[ir.l] == rf[ir.m];
        }
        else if (ir.op == NEQ) //18
        {
            rf[ir.r] = rf[ir.l] != rf[ir.m];
        }
        else if (ir.op == LSS) //19
        {
            rf[ir.r] = rf[ir.l] < rf[ir.m];
        }
        else if (ir.op == LEQ) //20
        {
            rf[ir.r] = rf[ir.l] <= rf[ir.m];
        }
        else if (ir.op == GTR) //21
        {
            rf[ir.r] = rf[ir.l] > rf[ir.m];
        }
        else if (ir.op == GEQ) //22
        {
            rf[ir.r] = rf[ir.l] >= rf[ir.m];
        }
        else
        {
            if (ir.op==0) //remove this if op 00 becomes a no op
                printf("Program ended unexpectedly (without SIO 0 0 3 instruction) on line %d\n", pc);
            else
                printf("Halt - Invalid opcode (\"%d\") detected on line %d\n", ir.op, pc);
            halt = 1;
            break;
        }
        //end execution


        //printout
		{
            // The op code, register, lexical level, value, PC, BP, & SP
            //printf("%-4s%3d%3d%3d[%3d%3d%3d] ", op, reg, lex, value, pc, bp, sp);
            if (show_vm == 1) printf("%-4s%3d%3d%3d[%3d%3d%3d] ", instr[ir.op], ir.r, ir.l, ir.m, pc, bp, sp);


            // The STACK section
            //printf("|"); //for the BEGINNING of a new lexical level
            //printf("%3d ",stack[i]); //for each member of the current level
            //printf("\n"); // at the end of full stack display
            if (show_vm == 1) printStack(sp, bp, stack, lex);
            if (show_vm == 1) printf("\n");

            // The REGISTERS - printf("\tRegisters:[%3d%3d%3d%3d%3d%3d%3d%3d]\n",r0, r1, r2, r3, r4, r5, r6, r7);
            /*printf("\tRegisters:[%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d]\n",
                        rf[0], rf[1], rf[2], rf[3], rf[4], rf[5], rf[6], rf[7], rf[8], rf[9], rf[10], rf[11], rf[12], rf[13], rf[14], rf[15]);*/
            if (show_vm == 1) printf("\tRegisters:[");
            for (j=0; j<NUM_REGISTERS; j++)
                if (show_vm == 1) printf("%3d", rf[j]);
            if (show_vm == 1) printf("]\n");
        }

        // used for obsolete print modes - can probably(?!) get rid of this. (count is needed for printing)
		previousPC = pc;
		count++;
    } // end (while halt == 0)


    if (halt != 0)
        return halt;

    return 0;
}


// Appendix D: Find base L levels down
int base(int l, int base)
{
    int b1; //find base L levels down
    b1 = base;
    while(l > 0)
    {
        b1 = stack[b1 + 1];
        l--;
    }
    return b1;
}

// Provided recursive function that prints the contents of the stack
void printStack(int sp, int bp, int* stack, int lex){
    int i;
    if (bp == 1) {
        if (lex > 0) {
        if (show_vm == 1) printf("|");
        }
    }

    else {
    //Print the lesser lexical level
        printStack(bp-1, stack[bp + 2], stack, lex-1);
        if (show_vm == 1) printf("|");
    }
    //Print the stack contents - at the current level
    for (i = bp; i <= sp; i++) {
        if (show_vm == 1) printf("%3d ", stack[i]);
    }
}
