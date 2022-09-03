CXX := clang++
STANDARD := -std=c++20
WARNINGS := -Wall -Wextra -Wpedantic
INCLUDES := -Iinclude

.PHONY: debug release test clean

debug:
	$(CXX) $(STANDARD) $(WARNINGS) $(INCLUDES) -O0 -g \
		main.cpp -o main_debug

release:
	$(CXX) $(STANDARD) $(WARNINGS) $(INCLUDES) -O3 -DNDEBUG \
		main.cpp -o main_release

test:
	$(CXX) $(STANDARD) $(WARNINGS) $(INCLUDES) -O1 -g \
		-fsanitize=address,undefined \
		-fno-omit-frame-pointer \
		tests/test_fixed_pool.cpp -o test_fixed_pool
	./test_fixed_pool

clean:
	rm -rf main_debug main_release test_fixed_pool *.dSYM
