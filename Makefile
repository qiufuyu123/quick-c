
.PHONY: clean

qc:
	cd src && bear make -s
	cd src && mv compile_commands.json ../.vscode/

clean:
	rm build/*.o -rf
	rm ./quick-c -rf