vzip: serial.c
	gcc serial.c -lz -lpthread -o vzip

test:
	rm -f video.vzip
	./vzip frames
	./check.sh

submit: 
	mkdir -p src
	cp serial.c Makefile src/
	zip -r src.zip src
	
clean:
	rm -f vzip video.vzip



	





	



