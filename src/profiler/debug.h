#ifndef _DEBUG_H
#define _DEBUG_H
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <unistd.h>

/* There are several message levels */

#ifdef DEBUG 
#define ERROR(format, ...) { write(STDERR_FILENO, "ERROR: ", sizeof("ERROR: ")); \
    print_error(format,  ##__VA_ARGS__);}

#define WARNING(format, ...) { write(STDERR_FILENO, "WARNING: ", sizeof("WARNING: ")); \
    print_error(format,  ##__VA_ARGS__);}

#define INFO(format, ...) { write(STDOUT_FILENO, "INFO: ", sizeof("INFO: ")); \
    print_info(format,  ##__VA_ARGS__);}

void print_error(const char *format ,...); 
void print_info(const char *format ,...); 

#else
#define ERROR(format, ...) ;
#define WARNING(format, ...) ;
#define INFO(format, ...) ;
#endif

#define INFO_CODE_BEGIN if(true) {
#define INFO_CODE_END }

#define DEBUG_CODE_BEGIN(turn_on) if(turn_on) {
#define DEBUG_CODE_END }

#define IMPLICITLY_USE(x) (void) x;

#endif
