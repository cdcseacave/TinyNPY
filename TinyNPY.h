////////////////////////////////////////////////////////////////////
// TinyNPY.h
//
// Read/write .npy and .npz python numpy array files.
//
// Copyright 2007 cDc@seacave
// Distributed under the Boost Software License, Version 1.0
// (See http://www.boost.org/LICENSE_1_0.txt)

#ifndef __SEACAVE_NPY_H__
#define __SEACAVE_NPY_H__


// I N C L U D E S /////////////////////////////////////////////////

#include <typeinfo>
#include <cstdint>
#include <cstring>
#include <complex>
#include <numeric>
#include <vector>
#include <string>
#include <map>

#define TINYNPY_MAJOR_VERSION 1
#define TINYNPY_MINOR_VERSION 0
#define TINYNPY_PATCH_VERSION 0

#ifdef _MSC_VER
#   ifdef TINYNPY_EXPORT
#       define TINYNPY_LIB __declspec(dllexport)
#   elif defined(TINYNPY_IMPORT)
#       define TINYNPY_LIB __declspec(dllimport)
#   else
#       define TINYNPY_LIB
#   endif
#elif __GNUC__ >= 4
#   define TINYNPY_LIB __attribute__((visibility("default")))
#else
#   define TINYNPY_LIB
#endif


// D E F I N E S ///////////////////////////////////////////////////

#ifndef ASSERT
#define ASSERT(x)
#endif
#ifndef LPCSTR
typedef const char* LPCSTR;
#endif
#ifndef _tcslen
#define _tcslen strlen
#endif
#ifndef _tcsncmp
#define _tcsncmp strncmp
#endif


// S T R U C T S ///////////////////////////////////////////////////

class TINYNPY_LIB NpyArray {
public:
	using shape_t = std::vector<size_t>;
	using npz_t = std::map<std::string, NpyArray>;

private:
	uint8_t* data;
	shape_t shape;
	size_t numValues;
	size_t wordSize;
	char type;
	bool fortranOrder;

public:
	NpyArray() : data(NULL), numValues(0), wordSize(0), type(0), fortranOrder(0) {}

	NpyArray(const shape_t& _shape, size_t _wordSize, char _type, bool _fortranOrder=false)
		: data(NULL), shape(_shape), numValues(NumValue(shape)), wordSize(_wordSize), type(_type), fortranOrder(_fortranOrder) {}

	NpyArray(NpyArray&& arr)
		: data(arr.data), shape(std::move(arr.shape)), numValues(arr.numValues), wordSize(arr.wordSize), type(arr.type), fortranOrder(arr.fortranOrder) { arr.Clean(); }

	NpyArray(const NpyArray&) = delete;

	~NpyArray() { delete[] data; }


	bool IsEmpty() const {
		return data == NULL;
	}
	void Allocate() {
		ASSERT(data == NULL && numValues > 0);
		data = new uint8_t[SizeBytes()];
	}
	void SetData(const uint8_t* _data) {
		ASSERT(data == NULL);
		data = const_cast<uint8_t*>(_data);
	}
	void Release() {
		delete[] data;
		Clean();
	}
	void Clean() {
		data = NULL;
	}


	size_t SizeBytes() const {
		return numValues * wordSize;
	}
	size_t SizeValueBytes() const {
		return wordSize;
	}
	static size_t NumValue(const shape_t& shape) {
		return std::accumulate(shape.cbegin(), shape.cend(), size_t(1), std::multiplies<size_t>());
	}
	size_t NumValue() const {
		return numValues;
	}

	const std::type_info& ValueType() const {
		return getTypeInfo(type, wordSize);
	}

	const shape_t& Shape() const {
		return shape;
	}
	shape_t& Shape() {
		return shape;
	}

	template<typename T=uint8_t>
	const T* Data() const {
		return reinterpret_cast<const T*>(data);
	}
	template<typename T=uint8_t>
	T* Data() {
		return reinterpret_cast<T*>(data);
	}
	template<typename T>
	std::vector<T> DataVector() const {
		const T* p = Data<T>();
		return std::vector<T>(p, p + numValues);
	}


	// input
	LPCSTR LoadNPY(FILE* fp);
	LPCSTR LoadNPY(std::string filename);
	LPCSTR LoadNPZ(FILE* fp, uint32_t compr_bytes, uint32_t uncompr_bytes);
	LPCSTR LoadNPZ(std::string filename, std::string varname);
	static LPCSTR LoadNPZ(std::string filename, npz_t& arrays);


	// output
	LPCSTR SaveNPY(std::string filename, bool bAppend=false) const;
	LPCSTR SaveNPZ(std::string zipname, std::string varname, bool bAppend=true) const;
	template<typename T>
	static LPCSTR SaveNPY(std::string filename, const std::vector<T>& data, shape_t shape=shape_t(), bool bAppend=false) {
		if (shape.empty())
			shape.push_back(data.size());
		NpyArray arr(std::move(shape), sizeof(T), getTypeChar(typeid(T)));
		arr.data = data.data();
		LPCSTR ret = arr.SaveNPY(filename, bAppend);
		arr.data = NULL;
		return ret;
	}
	template<typename T>
	static LPCSTR SaveNPZ(std::string zipname, std::string varname, const std::vector<T>& data, shape_t shape=shape_t(), bool bAppend=false) {
		if (shape.empty())
			shape.push_back(data.size());
		NpyArray arr(std::move(shape), sizeof(T), getTypeChar(typeid(T)));
		arr.data = data.data();
		LPCSTR ret = arr.SaveNPZ(zipname, varname, bAppend);
		arr.data = NULL;
		return ret;
	}

private:
	void init() {
		numValues = NumValue(shape);
		Allocate();
	}

	// input
	static LPCSTR ParseHeaderNPY(const std::string& header, shape_t& shape, size_t& wordSize, char& type, bool& fortranOrder);
	static LPCSTR ParseHeaderNPY(const uint8_t* buffer, shape_t& shape, size_t& wordSize, char& type, bool& fortranOrder);
	static LPCSTR ParseHeaderNPY(FILE* fp, shape_t& shape, size_t& wordSize, char& type, bool& fortranOrder);
	static LPCSTR ParseFooterZIP(FILE* fp, uint16_t& nrecs, size_t& global_header_size, size_t& global_header_offset);
	static LPCSTR LoadArrayNPZ(FILE* fp, std::string& varname, NpyArray& arr);

	// output
	static std::vector<char> CreateHeaderNPY(const shape_t& shape, char type, size_t wordSize);

	// tools
	static char getTypeChar(const std::type_info& t);
	static const std::type_info& getTypeInfo(char t, size_t s);

	static std::vector<char>& add(std::vector<char>& lhs, const std::string rhs) {
		lhs.insert(lhs.end(), rhs.cbegin(), rhs.cend());
		return lhs;
	}
	static std::vector<char>& add(std::vector<char>& lhs, const char* rhs) {
		// write in little endian
		const size_t len = _tcslen(rhs);
		lhs.reserve(len);
		for (size_t byte = 0; byte < len; byte++)
			lhs.push_back(rhs[byte]);
		return lhs;
	}
	template<typename T>
	static std::vector<char>& add(std::vector<char>& lhs, const T rhs) {
		// write in little endian
		for (size_t byte = 0; byte < sizeof(T); byte++)
			lhs.push_back(reinterpret_cast<const uint8_t*>(&rhs)[byte]);
		return lhs;
	}
};

#endif // __SEACAVE_NPY_H__
