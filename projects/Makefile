all:
	g++ -std=gnu++11 -Wall -Weffc++ -I ../src -O0 -ggdb -o ut ../src/MyClass.cc ../src/Tests.cc ../src/unittest.cc -lrt
	@ls -sh ut
clean:
	@rm -f results.html
	@rm -f results.pdf
	@rm -f ut

