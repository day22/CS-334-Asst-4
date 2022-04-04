driver:driver.c libpng16.a libz.a
    gcc -o driver driver.c libpng16.a libz.a -lm -lpthread