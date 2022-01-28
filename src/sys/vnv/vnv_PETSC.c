///PETSC:2232462417717387519
/// This file was automatically generated using the VnV-Matcher executable. 
/// The matcher allows for automatic registration of all VnV plugins and injection 
/// points. Building the matcher requires Clang. If Clang is not available on this machine,
/// Registration code should be written manually. 
/// 

//PACKAGENAME: PETSC

#include "VnV.h" 
DECLARESUBPACKAGE(VnVHypre)
DECLAREOPTIONS(PETSC)
const char* getFullRegistrationJson_PETSC(){
	 return "{\"InjectionPoints\":{\"SanityCheck\":{\"docs\":{\"description\":\"\",\"instructions\":\"\",\"params\":{},\"template\":\"\",\"title\":\"\"},\"loop\":true,\"name\":\"SanityCheck\",\"packageName\":\"PETSC\",\"parameters\":{\"int petsc_vnv_test_function(int)\":{\"x\":\"int*\"}},\"stages\":{\"Begin\":{\"docs\":{\"description\":\"\",\"instructions\":\"\",\"params\":{},\"template\":\"\",\"title\":\"\"},\"info\":{\"Calling Function\":\"petsc_vnv_test_function\",\"Calling Function Column\":1,\"Calling Function Line\":28,\"filename\":\"/home/ben/source/vv/applications/petsc/src/sys/vnv/vnv.c\",\"lineColumn\":5,\"lineNumber\":23}},\"End\":{\"docs\":{\"description\":\"\",\"instructions\":\"\",\"params\":{},\"template\":\"\",\"title\":\"\"},\"info\":{\"Calling Function\":\"petsc_vnv_test_function\",\"Calling Function Column\":1,\"Calling Function Line\":28,\"filename\":\"/home/ben/source/vv/applications/petsc/src/sys/vnv/vnv.c\",\"lineColumn\":5,\"lineNumber\":30}}}}},\"Options\":{\"description\":\"\",\"instructions\":\"\",\"params\":{},\"template\":\"\\n VnV allows users to set options in hypre using the input file. This callback\\n registers the json schema for the available options with the toolkit and defines\\n the options callback. Because we are in C, we are using the C interface. \\n \\n Life would be way easier if we can just compile this file with c++ :)\\n\\n TODO: Add options to the schema and parse them in this function.\\n \\n\",\"title\":\"\"},\"SubPackages\":{\"VnVHypre\":{\"docs\":{\"description\":\"\",\"instructions\":\"\",\"params\":{},\"template\":\"\",\"title\":\"\"},\"name\":\"VnVHypre\",\"packageName\":\"PETSC\"}}}";}

INJECTION_REGISTRATION(PETSC){
	REGISTERSUBPACKAGE(VnVHypre);
	REGISTEROPTIONS(PETSC)
	Register_Injection_Point("PETSC","SanityCheck","{\"int petsc_vnv_test_function(int)\":{\"x\":\"int*\"}}");
	REGISTER_FULL_JSON(PETSC, getFullRegistrationJson_PETSC);
}



