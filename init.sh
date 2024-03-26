mkdir -p build
cp pcmake.sh build/pcmake.sh
cd build
cmake ..
make PorkchopLite -j$(nproc)
