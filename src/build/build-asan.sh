export CC=clang
export CXX=clang++
export CFLAGS='-fsanitize=address'
export CXXFLAGS='-fsanitize=address'
cmake .. -DCMAKE_BUILD_TYPE=Release -GNinja && cmake --build .
