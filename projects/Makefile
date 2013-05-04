all: gcc clang

gcc:
	g++ -std=gnu++11 -Wall -Weffc++ -I ../src -O0 -ggdb -o ut_gcc ../src/MyClass.cc ../src/Tests.cc ../src/unittest.cc -lrt
	@ls -sh ut_gcc
clang:
	clang++ -std=gnu++11 -Wall -Weffc++ -D__STRICT_ANSI__ -I ../src -O0 -ggdb -o ut_clang ../src/MyClass.cc ../src/Tests.cc ../src/unittest.cc -lrt
	@ls -sh ut_clang
link:
	@ln -s ut_gcc ut
clean:
	@rm -f results.html
	@rm -f results.pdf
	@rm -f ut_gcc
	@rm -f ut_clang

