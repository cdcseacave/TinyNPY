// Defines the entry point for the console application.

#ifdef _MSC_VER
#include <windows.h>
#endif
#include "TinyNPY.h"
#include <iostream> // std::cout

int main(int argc, const char** argv)
{
	if (argc != 2) {
		std::cout << "Usage: TinyNPY <array_file>\n";
		return -1;
	}

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
	std::cout << "Dimensions:";
	for (size_t s: arr.Shape())
		std::cout << " " << s;
	std::cout << "\n";
	std::cout << "Number of values: " << arr.NumValue() << "\n";
	std::cout << "Size in bytes: " << arr.SizeBytes() << "\n";
	if (typeid(int) == arr.ValueType())
		std::cout << "Value type: int\n";
	if (typeid(float) == arr.ValueType())
		std::cout << "Value type: float\n";
	std::cout << "Values order: " << (arr.ColMajor() ? "col-major\n" : "row-major\n");
	return EXIT_SUCCESS;
}
