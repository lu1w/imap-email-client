EXE=fetchmail

$(EXE): src/main.c src/util.c src/cmd_retrieve.c src/cmd_parse.c src/cmd_mime.c src/cmd_list.c
	cc -Wall -o $(EXE) $<

format:
	clang-format -style=file -i *.c

clean: 
	rm -f *.txt *.md *.out ./fetchmail
