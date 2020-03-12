by Michael Harris for COP3402 Summer '18 with Professor Montagne 


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
project specification and not a duplication of output! 