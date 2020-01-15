#ifndef x86_misc_h
#define x86_misc_h

#include "profiler_support.h"

bool get_mem_access_length_and_type_address(void * ip, uint32_t *accessLen, AccessType *accessType, FloatType * floatType, void * context, void** address);
bool get_mem_access_length_and_type(void * ip, uint32_t *accessLen, AccessType *accessType);
// void decode_method(const void *method_start_addr, const void *method_end_addr, const void * first_pc);
void *get_previous_instruction(const void *method_start_addr, const void *method_end_addr, const void *ins, void *excludeList[], int numExcludes);
#ifdef PRINT_PMU_INS
void print_single_instruction(std::ofstream *inst_file, const void *ins);
#endif
#ifdef PRINT_METHOD_INS
void print_instructions(std::ofstream *inst_file, const void *method_start_addr, const void *method_end_addr);
bool INS_IsMethodOrSysCall(void *ip);
#endif
// unsigned int get_float_operation_length(void *ip, uint8_t op_idx);
// void * get_previous_instruction(void *ins, void **pip, void ** excludeList, int numExcludes);
// FunctionType is_same_function(void *ins1, void* ins2);

#endif

