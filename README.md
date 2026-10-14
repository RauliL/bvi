# BVI - Binary visual editor

Fork of the original [BVI] hex editor, with modernized codebase.

## How to compile

My fork uses [CMake] instead of GNU autotools, so that must be installed before
compiling the program.

```bash
git clone https://github.com/RauliL/bvi.git
cd bvi
mkdir build
cd build
cmake ..
make
sudo make install
```

[bvi]: http://bvi.sourceforge.net/
[CMake]: https://cmake.org
