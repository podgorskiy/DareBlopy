#include "example.h"


const char* Records::DataTypeString(DataType dtype)
{
	switch (dtype)
	{
		case DataType::DT_FLOAT:
			return "float32";
		case DataType::DT_INT64:
			return "int64";
		case DataType::DT_UINT8:
			return "uint8";
		case DataType::DT_STRING:
			return "string";
		case DataType::DT_INVALID:
		default:
			return "invalid";
	}
}
