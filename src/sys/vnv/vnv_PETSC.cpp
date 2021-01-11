///3504522102909215289
/// This file was automatically generated using the VnV-Matcher executable. 
/// The matcher allows for automatic registration of all VnV plugins and injection 
/// points. Building the matcher requires Clang. If Clang is not available on this machine,
/// Registration code should be written manually. 
/// 

//PACKAGENAME: PETSC

#include "VnV.h" 
DECLAREOPTIONS(PETSC)
const char* getFullRegistrationJson_PETSC(){
	 return "{\"Communicator\":{\"docs\":\"\",\"name\":\"mpi\",\"package\":\"VNV\"},\"InjectionPoints\":{\"SanityCheck\":{\"docs\":\"\",\"name\":\"SanityCheck\",\"packageName\":\"PETSC\",\"parameters\":[{\"x\":\"int*\"},{\"x\":\"int*\"}],\"stages\":{\"Begin\":{\"docs\":\"\",\"info\":{\"Calling Function\":\"petsc_vnv_test_function\",\"Calling Function Column\":1,\"Calling Function Line\":26,\"filename\":\"src/sys/vnv/vnv.c\",\"lineColumn\":5,\"lineNumber\":60}},\"End\":{\"docs\":\"\",\"info\":{\"Calling Function\":\"petsc_vnv_test_function\",\"Calling Function Column\":1,\"Calling Function Line\":26,\"filename\":\"src/sys/vnv/vnv.c\",\"lineColumn\":5,\"lineNumber\":69}},\"inner\":{\"docs\":\"\",\"info\":{\"Calling Function\":\"petsc_vnv_test_function\",\"Calling Function Column\":1,\"Calling Function Line\":26,\"filename\":\"src/sys/vnv/vnv.c\",\"lineColumn\":5,\"lineNumber\":83}}}}},\"Options\":\"\\n VnV allows users to set options in hypre using the input file. This callback\\n registers the json schema for the available options with the toolkit and defines\\n the options callback. Because we are in C, we are using the C interface. \\n \\n Life would be way easier if we can just compile this file with c++ :)\\n\\n TODO: Add options to the schema and parse them in this function.\\n \"}";}

INJECTION_REGISTRATION(PETSC){
	REGISTEROPTIONS(PETSC)
	VnV_Declare_Communicator("PETSC","VNV","mpi");
	Register_Injection_Point(PETSC,SanityCheck,0,"{\"x\":\"int*\"}");
	REGISTER_FULL_JSON(PETSC, getFullRegistrationJson_PETSC);
};



