////////////////////////////////////////////////////////////////////
// TinyNPY.cpp
//
// Copyright 2007 cDc@seacave
// Distributed under the Boost Software License, Version 1.0
// (See http://www.boost.org/LICENSE_1_0.txt)

#include "TinyNPY.h"
#include <regex>
#include <functional>
#include <zlib.h>


// D E F I N E S ///////////////////////////////////////////////////


// S T R U C T S ///////////////////////////////////////////////////

// Invoke a function when the object is destroyed,
// typically at scope exit if the object is allocated on the stack
template <typename Functor=std::function<void()>>
class TScopeExitRun
{
public:
	TScopeExitRun(Functor f) : functor(f) {}
	~TScopeExitRun() { functor(); }
	void Reset(Functor f) { functor = f; }

protected:
	Functor functor;
};
typedef class TScopeExitRun<> ScopeExitRun;
/*----------------------------------------------------------------*/


// input
LPCSTR NpyArray::ParseHeaderNPY(const std::string& header, shape_t& shape, size_t& wordSize, char& type, bool& fortranOrder)
{
	ASSERT(header[header.size() - 1] == '\n');

	// fortran order
	size_t loc1 = header.find("fortran_order");
	if (loc1 == std::string::npos)
		return "error: failed to find header keyword 'fortran_order'";
	fortranOrder = (header.substr(loc1+16, 4) == "True");

	// shape
	loc1 = header.find("(");
	size_t loc2 = header.find(")");
	if (loc1 == std::string::npos || loc2 == std::string::npos)
		return "error: failed to find header keyword '(' or ')'";

	shape.clear();
	std::regex num_regex("[0-9][0-9]*");
	std::smatch sm;
	std::string strShape = header.substr(loc1 + 1, loc2 - loc1 - 1);
	while (std::regex_search(strShape, sm, num_regex)) {
		shape.push_back(std::stoi(sm[0].str()));
		strShape = sm.suffix().str();
	}

	// endian, word size, data type
	// byte order code | stands for not applicable. 
	// not sure when this applies except for byte array
	loc1 = header.find("descr");
	if (loc1 == std::string::npos)
		return "error: failed to find header keyword 'descr'";
	loc1 += 9;
	const bool littleEndian = (header[loc1] == '<' || header[loc1] == '|');
	ASSERT(littleEndian);
	type = header[loc1+1];

	const std::string str_ws = header.substr(loc1+2);
	loc2 = str_ws.find("'");
	wordSize = std::stoi(str_ws.substr(0, loc2));
	return NULL;
}

LPCSTR NpyArray::ParseHeaderNPY(const uint8_t* buffer, shape_t& shape, size_t& wordSize, char& type, bool& fortranOrder)
{
	if (buffer[0] != (uint8_t)0x93 || _tcsncmp(reinterpret_cast<const char*>(buffer+1), "NUMPY", 5) != 0)
		return "error: invalid header id";
	// parse the length of the header data
	uint32_t lenHeader, offset;
	ASSERT(buffer[7] >= 0); // minor version number of the file format
	if (buffer[6] > 1) { // major version number of the file format
		// little-endian unsigned int
		lenHeader = (uint32_t(buffer[11])<<24)|(uint32_t(buffer[10])<<16)|(uint32_t(buffer[9])<<8)|uint32_t(buffer[8]);
		offset = 12;
	} else {
		// little-endian unsigned short int
		lenHeader = (uint16_t(buffer[9])<<8)|uint16_t(buffer[8]);
		offset = 10;
	}
	const std::string header(reinterpret_cast<const char*>(buffer+offset), lenHeader);
	return ParseHeaderNPY(header, shape, wordSize, type, fortranOrder);
}

LPCSTR NpyArray::ParseHeaderNPY(FILE* fp, shape_t& shape, size_t& wordSize, char& type, bool& fortranOrder)
{
	char buffer[32];
	if (fread(buffer, sizeof(char), 10, fp) != 10 ||
		buffer[0] != (char)0x93 || _tcsncmp(buffer+1, "NUMPY", 5) != 0)
		return "error: invalid header id";
	// parse the length of the header data
	uint32_t lenHeader;
	ASSERT(buffer[7] >= 0); // minor version number of the file format
	if (buffer[6] > 1) { // major version number of the file format
		// little-endian unsigned int
		fread(buffer+10, sizeof(char), 2, fp);
		lenHeader = (uint32_t(buffer[11])<<24)|(uint32_t(buffer[10])<<16)|(uint32_t(buffer[9])<<8)|uint32_t(buffer[8]);
	} else {
		// little-endian unsigned short int
		lenHeader = (uint16_t(buffer[9])<<8)|uint16_t(buffer[8]);
	}
	std::string header(lenHeader, '\0');
	if (fread(&header[0], sizeof(char), lenHeader, fp) != lenHeader)
		return "error: invalid header";
	return ParseHeaderNPY(header, shape, wordSize, type, fortranOrder);
}

LPCSTR NpyArray::ParseFooterZIP(FILE* fp, uint16_t& nrecs, size_t& globalHeaderSize, size_t& globalHeaderOffset)
{
	char footer[32];
	fseek(fp, -22, SEEK_END);
	if (fread(footer, sizeof(char), 22, fp) != 22)
		return "error: failed footer";
	const uint16_t diskNo = *(uint16_t*)(footer+4); ASSERT(diskNo == 0);
	const uint16_t diskStart = *(uint16_t*)(footer+6); ASSERT(diskStart == 0);
	const uint16_t nrecsOnDisk = *(uint16_t*)(footer+8);
	nrecs = *(uint16_t*)(footer+10); ASSERT(nrecsOnDisk == nrecs);
	globalHeaderSize = *(uint32_t*)(footer+12);
	globalHeaderOffset = *(uint32_t*)(footer+16);
	const uint16_t lenComment = *(uint16_t*)(footer+20); ASSERT(lenComment == 0);
	return NULL;
}

LPCSTR NpyArray::LoadNPY(FILE* fp)
{
	Release();
	LPCSTR ret = ParseHeaderNPY(fp, shape, wordSize, type, fortranOrder);
	if (ret != NULL)
		return ret;
	init();
	const size_t nread = fread(Data(), 1, SizeBytes(), fp);
	if (nread != SizeBytes())
		return "error: failed fread";
	return NULL;
}

LPCSTR NpyArray::LoadNPY(std::string filename)
{
	FILE* fp = fopen(filename.c_str(), "rb");
	if (!fp)
		return "error: unable to open file";
	const ScopeExitRun closeFp([&]() { fclose(fp); });
	return LoadNPY(fp);
}

LPCSTR NpyArray::LoadNPZ(FILE* fp, uint32_t comprBytes, uint32_t uncomprBytes)
{
	std::vector<uint8_t> bufferCompr(comprBytes);
	std::vector<uint8_t> bufferUncompr(uncomprBytes);
	if (fread(bufferCompr.data(), 1, comprBytes, fp) != comprBytes)
		return "error: failed fread";

	z_stream d_stream;
	d_stream.zalloc = Z_NULL;
	d_stream.zfree = Z_NULL;
	d_stream.opaque = Z_NULL;
	d_stream.avail_in = 0;
	d_stream.next_in = Z_NULL;
	int err = inflateInit2(&d_stream, -MAX_WBITS);
	if (err != Z_OK)
		return "error: can not init inflate";

	d_stream.avail_in = comprBytes;
	d_stream.next_in = bufferCompr.data();
	d_stream.avail_out = uncomprBytes;
	d_stream.next_out = bufferUncompr.data();
	err = inflate(&d_stream, Z_FINISH);
	if (err != Z_STREAM_END && err != Z_OK)
		return "error: can not uncompress";

	err = inflateEnd(&d_stream);

	LPCSTR ret = ParseHeaderNPY(bufferUncompr.data(), shape, wordSize, type, fortranOrder);
	if (ret != NULL)
		return ret;
	init();
	const size_t offset = uncomprBytes - SizeBytes();
	memcpy(Data(), bufferUncompr.data()+offset, SizeBytes());
	return NULL;
}

LPCSTR NpyArray::LoadNPZ(std::string filename, std::string varname)
{
	Release();
	FILE* fp = fopen(filename.c_str(), "rb");
	if (!fp)
		return "error: unable to open file";
	const ScopeExitRun closeFp([&]() { fclose(fp); });
	while (true) {
		const LPCSTR ret = LoadArrayNPZ(fp, varname, *this);
		if (ret == NULL && !IsEmpty())
			return NULL;
		if (ret == (const char*)1)
			break;
	}
	return "error: variable name not found";
}

LPCSTR NpyArray::LoadNPZ(std::string filename, npz_t& arrays)
{
	FILE* fp = fopen(filename.c_str(), "rb");
	if (!fp)
		return "error: unable to open file";
	const ScopeExitRun closeFp([&]() { fclose(fp); });
	while (true) {
		NpyArray arr;
		std::string varname;
		const LPCSTR ret = LoadArrayNPZ(fp, varname, arr);
		if (ret == (const char*)1)
			break;
		if (ret != NULL)
			return ret;
		arrays.emplace(varname, std::move(arr));
	}
	return NULL;
}

LPCSTR NpyArray::LoadArrayNPZ(FILE* fp, std::string& varname, NpyArray& arr)
{
	char localHeader[32];
	if (fread(localHeader, sizeof(char), 30, fp) != 30)
		return "error: failed fread";

	// if we've reached the global header, stop reading
	if (localHeader[2] != 0x03 || localHeader[3] != 0x04)
		return (const char*)1;

	// read in the variable name
	const uint16_t lenName = *reinterpret_cast<uint16_t*>(localHeader+26);
	std::string vname(lenName, ' ');
	if (fread(&vname[0], sizeof(char), lenName, fp) != lenName)
		return "error: failed fread";

	// erase the lagging .npy        
	vname.erase(vname.end()-4, vname.end());

	// read in the extra field
	const uint16_t lenExtraField = *reinterpret_cast<uint16_t*>(localHeader+28);
	fseek(fp, lenExtraField, SEEK_CUR); // skip past the extra field

	if (varname.empty() || varname == vname) {
		// read current array
		if (varname.empty())
			varname = vname;
		const uint16_t comprMethod = *reinterpret_cast<uint16_t*>(localHeader+8);
		if (comprMethod == 0)
			return arr.LoadNPY(fp);
		const uint32_t comprBytes = *reinterpret_cast<uint32_t*>(localHeader+18);
		const uint32_t uncomprBytes = *reinterpret_cast<uint32_t*>(localHeader+22);
		return arr.LoadNPZ(fp, comprBytes, uncomprBytes);
	}

	// skip current array data
	const uint32_t size = *reinterpret_cast<uint32_t*>(localHeader+22);
	fseek(fp, size, SEEK_CUR);
	return NULL;
}
/*----------------------------------------------------------------*/



// output
std::vector<char> NpyArray::CreateHeaderNPY(const shape_t& shape, char type, size_t wordSize)
{
	std::vector<char> dict;
	add(dict, "{'descr': '");
	#if __BYTE_ORDER == __LITTLE_ENDIAN
	add(dict, '<');
	#else
	add(dict, '>');
	#endif
	add(dict, type);
	add(dict, std::to_string(wordSize));
	add(dict, "', 'fortran_order': False, 'shape': (");
	add(dict, std::to_string(shape[0]));
	for (size_t i = 1; i < shape.size(); i++) {
		add(dict, ", ");
		add(dict, std::to_string(shape[i]));
	}
	add(dict, "), }");
	// pad with spaces so that preamble+dict is modulo 16 bytes
	// preamble is 10/12 bytes and dict needs to end with \n
	char ver = 1;
	size_t remainder = 16 - (10 + dict.size()) % 16;
	if (dict.size() + remainder > 65535) {
		ver = 2;
		remainder = 16 - (12 + dict.size()) % 16;
	}
	dict.insert(dict.end(), remainder, ' ');
	dict.back() = '\n';

	std::vector<char> header;
	add(header, (char)0x93);
	add(header, "NUMPY");
	add(header, ver); // major version of numpy format
	add(header, (char)0); // minor version of numpy format
	if (ver == 1)
		add(header, (uint16_t)dict.size());
	else
		add(header, (uint32_t)dict.size());
	header.insert(header.end(), dict.begin(), dict.end());
	return header;
}

LPCSTR NpyArray::SaveNPY(std::string filename, bool bAppend) const
{
	FILE* fp;
	shape_t _shape;
	const shape_t* pShape;
	if (bAppend && (fp=fopen(filename.c_str(), "r+b")) != NULL) {
		// file exists, append to it; read the header, modify the array size
		char _type;
		size_t _wordSize;
		bool _fortranOrder;
		LPCSTR ret = ParseHeaderNPY(fp, _shape, _wordSize, _type, _fortranOrder);
		if (ret != NULL)
			return ret;
		ASSERT(!_fortranOrder);

		if (wordSize != _wordSize)
			return "error: npy_save word size";
		if (shape.size() != _shape.size())
			return "error: npy_save attempting to append mis-dimensioned data";

		for (size_t i = 1; i < shape.size(); i++) {
			if (shape[i] != _shape[i])
				return "error: npy_save attempting to append misshaped data";
		}
		_shape[0] += shape[0];
		pShape = &_shape;
	} else {
		// create a new file
		fp = fopen(filename.c_str(), "wb");
		pShape = &shape;
	}
	if (!fp)
		return "error: unable to open file";

	const std::vector<char> header = CreateHeaderNPY(*pShape, std::abs(type), wordSize);

	fseek(fp, 0, SEEK_SET);
	fwrite(header.data(), sizeof(char), header.size(), fp);
	fseek(fp, 0, SEEK_END);
	fwrite(Data(), wordSize, numValues, fp);
	fclose(fp);
	return NULL;
}

LPCSTR NpyArray::SaveNPZ(std::string zipname, std::string varname, bool bAppend) const
{
	FILE* fp;
	uint16_t nrecs = 0;
	size_t globalHeaderOffset = 0;
	std::vector<char> globalHeader;
	if (bAppend && (fp=fopen(zipname.c_str(), "r+b")) != NULL) {
		// zip file exists, add a new NPY array to it;
		// first read the footer and parse the offset and size of the global header
		// then read and store the global header;
		// the new data will be written at the start of the global header,
		// then append the global header and footer below it
		size_t globalHeaderSize;
		LPCSTR ret = ParseFooterZIP(fp, nrecs, globalHeaderSize, globalHeaderOffset);
		if (ret != NULL)
			return ret;
		fseek(fp, (long)globalHeaderOffset, SEEK_SET);
		globalHeader.resize(globalHeaderSize);
		size_t res = fread(globalHeader.data(), sizeof(char), globalHeaderSize, fp);
		if (res != globalHeaderSize)
			return "error: header read error while adding to existing zip";
		fseek(fp, (long)globalHeaderOffset, SEEK_SET);
	} else {
		fp = fopen(zipname.c_str(), "wb");
	}
	if (!fp)
		return "error: unable to open file";

	const std::vector<char> npyHeader = CreateHeaderNPY(shape, std::abs(type), wordSize);
	const size_t nbytes = SizeBytes() + npyHeader.size();

	// get the CRC of the data to be added
	uint32_t crc = crc32(0L, (uint8_t*)npyHeader.data(), (uLong)npyHeader.size());
	crc = crc32(crc, Data(), (uLong)SizeBytes());

	// append NPY extension
	varname += ".npy";

	// build the local header
	std::vector<char> localHeader;
	add(localHeader, "PK"); // first part of signature
	add(localHeader, (uint16_t)0x0403); // second part of signature
	add(localHeader, (uint16_t)20); // min version to extract
	add(localHeader, (uint16_t)0); // general purpose bit flag
	add(localHeader, (uint16_t)0); // compression method
	add(localHeader, (uint16_t)0); // file last mod time
	add(localHeader, (uint16_t)0); // file last mod date
	add(localHeader, (uint32_t)crc); // CRC
	add(localHeader, (uint32_t)nbytes); // compressed size
	add(localHeader, (uint32_t)nbytes); // uncompressed size
	add(localHeader, (uint16_t)varname.size()); // variable name length
	add(localHeader, (uint16_t)0); // extra field length
	add(localHeader, varname);

	// build global header
	add(globalHeader, "PK"); // first part of signature
	add(globalHeader, (uint16_t)0x0201); // second part of signature
	add(globalHeader, (uint16_t)20); // version made by
	globalHeader.insert(globalHeader.end(), localHeader.begin()+4, localHeader.begin()+30);
	add(globalHeader, (uint16_t)0); // file comment length
	add(globalHeader, (uint16_t)0); // disk number where file starts
	add(globalHeader, (uint16_t)0); // internal file attributes
	add(globalHeader, (uint32_t)0); // external file attributes
	add(globalHeader, (uint32_t)globalHeaderOffset); // relative offset of local file header, since it begins where the global header used to begin
	add(globalHeader, varname);

	// build footer
	std::vector<char> footer;
	add(footer, "PK"); // first part of signature
	add(footer, (uint16_t)0x0605); // second part of signature
	add(footer, (uint16_t)0); // number of this disk
	add(footer, (uint16_t)0); // disk where footer starts
	add(footer, (uint16_t)(nrecs+1)); // number of records on this disk
	add(footer, (uint16_t)(nrecs+1)); // total number of records
	add(footer, (uint32_t)globalHeader.size()); // number of bytes of global headers
	add(footer, (uint32_t)(globalHeaderOffset + nbytes + localHeader.size())); // offset of start of global headers, since global header now starts after newly written array
	add(footer, (uint16_t)0); // zip file comment length

	// write everything
	fwrite(localHeader.data(), sizeof(char), localHeader.size(), fp);
	fwrite(npyHeader.data(), sizeof(char), npyHeader.size(), fp);
	fwrite(Data(), wordSize, numValues, fp);
	fwrite(globalHeader.data(), sizeof(char), globalHeader.size(), fp);
	fwrite(footer.data(), sizeof(char), footer.size(), fp);
	fclose(fp);
	return NULL;
}
/*----------------------------------------------------------------*/



// tools
char NpyArray::getTypeChar(const std::type_info& t)
{
	if (t == typeid(float)) return 'f';
	if (t == typeid(double)) return 'f';
	if (t == typeid(long double)) return 'f';

	if (t == typeid(int)) return 'i';
	if (t == typeid(char)) return 'i';
	if (t == typeid(short)) return 'i';
	if (t == typeid(long)) return 'i';
	if (t == typeid(long long)) return 'i';

	if (t == typeid(uint8_t)) return 'u';
	if (t == typeid(unsigned short)) return 'u';
	if (t == typeid(unsigned long)) return 'u';
	if (t == typeid(unsigned long long)) return 'u';
	if (t == typeid(unsigned int)) return 'u';

	if (t == typeid(bool)) return 'b';

	if (t == typeid(std::complex<float>)) return 'c';
	if (t == typeid(std::complex<double>)) return 'c';
	if (t == typeid(std::complex<long double>)) return 'c';

	return '?';
}

const std::type_info& NpyArray::getTypeInfo(char t, size_t s)
{
	switch (t) {
	case 'f': switch (s) {
		case  4: return typeid(float);
		case  8: return typeid(double);
		case 16: return typeid(long double);
		} break;
	case 'i': switch (s) {
		case  1: return typeid(char);
		case  2: return typeid(short);
		case  4: return typeid(int);
		case  8: return typeid(long);
		case 16: return typeid(long long);
		} break;
	case 'u': switch (s) {
		case  1: return typeid(unsigned char);
		case  2: return typeid(unsigned short);
		case  4: return typeid(unsigned int);
		case  8: return typeid(unsigned long);
		case 16: return typeid(unsigned long long);
		} break;
	case 'c': switch (s) {
		case  8: return typeid(std::complex<float>);
		case 16: return typeid(std::complex<double>);
		case 32: return typeid(std::complex<long double>);
		} break;
	case 'b': return typeid(bool);
	}
	return typeid(void);
}
/*----------------------------------------------------------------*/
