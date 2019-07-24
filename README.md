# TinyNPY: Tiny C++ loader/exporter of python numpy array NPY/NPZ files

## Introduction

TinyNPY is a tiny, lightweight C++ library for parsing Numpy array files in NPY and NPZ format. No third party dependencies are needed to parse NPY and uncompressed NPZ files, but [ZLIB](https://www.zlib.net) library is needed to parse compressed NPZ files. TinyNPY is easy to use, simply copy the two source files in you project.

## Usage example

```
#include "TinyNPY.h"

int main(int argc, const char** argv)
{
	// read NPY array file
	NpyArray arr;
	const LPCSTR ret = arr.LoadNPY(argv[1]);

	// read NPZ arrays file: specific array
	//NpyArray arr;
	//const LPCSTR ret = arr.LoadNPZ(argv[1], "features");

	// read NPZ arrays file: all arrays
	//NpyArray::npz_t arrays;
	//const LPCSTR ret = arr.LoadNPZ(argv[1], arrays);
	//NpyArray& arr = arrays.begin()->second;

	if (ret != NULL) {
		std::cout << ret << " '" << argv[1] << "'\n";
		return -2;
	}

	// print array metadata
	std::cout << "Number of values " << arr.NumValue() << "\n";
	std::cout << "Size in bytes " << arr.SizeBytes() << "\n";
	if (typeid(int) == arr.ValueType())
		std::cout << "Value type float\n";
	if (typeid(float) == arr.ValueType())
		std::cout << "Value type float\n";
	return EXIT_SUCCESS;
}
```
See `main.cpp` for more details.

## Copyright

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

 - Redistributions of source code must retain the above copyright notice, 
   this list of conditions and the following disclaimer.
 - Redistributions in binary form must reproduce the above copyright notice, 
   this list of conditions and the following disclaimer in the documentation 
 and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY EXPRESS 
OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN 
NO EVENT SHALL THE FREEBSD PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, 
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
