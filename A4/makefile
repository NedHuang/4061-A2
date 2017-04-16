all:
	gcc -pthread -o image_manager image_manager.c
clean:
	rm -rf image_manager out
test1: clean all
	./image_manager v1 ~/Desktop/4061-A4/A4/dir0 out/
mac1: clean all
	./image_manager v1 ~/Desktop/4061-A4/A4/dir0 out/
test2: clean all
	./image_manager v2 ~/Desktop/4061-A4/A4/dir0 out/
mac2: clean all
	./image_manager v2 ~/Desktop/4061-A4/A4/dir0 out/
test3: all
	./image_manager v3 ~/Desktop/4061-A4/A4/dir0 out/
mac3: all
	./image_manager v3 ~/Desktop/4061-A4/A4/dir0 out/
