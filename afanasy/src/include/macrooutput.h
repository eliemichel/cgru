//###################################################################################
#include <errno.h>
#include <iostream>
#include <stdio.h>
#include "../libafanasy/af.h"
//#################### OUPUT ERRORS FORMATING ######################################

//#define AFERROR( msg) {fprintf(stderr,"AFERROR: %s",msg);fprintf(stderr,"\n");fflush( stderr);}
#define AFERROR( msg) { std::cerr << af::time2str()<< ": AFERROR: " << msg << std::endl; std::cerr.flush();}
#define AFERRAR( msg, ...) {fprintf(stderr,"%s: AFERROR: ",af::time2str().c_str());fprintf( stderr, msg, __VA_ARGS__);fprintf(stderr,"\n");fflush( stderr);}
#define AFERRPE( msg) {fprintf(stderr,"%s: AFERROR: ",af::time2str().c_str());fflush( stderr);perror( msg );fprintf(stderr,"\n");fflush( stderr);}
#define AFERRPA( msg, ...) {fprintf(stderr,"%s: AFERROR: ",af::time2str().c_str());fprintf( stderr, msg, __VA_ARGS__);fflush( stderr);perror(0);fprintf(stderr,"\n");fflush( stderr);}
//
//############## OUPUT INFO FORMATING, if AFOUTPUT only ##############################
#ifdef AFOUTPUT
#define AFINFA( msg, ...) {fprintf(stdout,"AFINFO: ");fprintf(stdout,msg,__VA_ARGS__);fprintf(stdout,"\n");fflush( stdout);}
#else
#define AFINFA( msg, ...) {}
#endif
//
#ifdef AFOUTPUT
//#define AFINFO( msg) {fprintf(stdout,"AFINFO: %s", msg);fprintf(stdout,"\n");fflush( stdout);}
#define AFINFO( msg) { std::cout << "AFINFO: " << msg << std::endl; std::cout.flush();}
#else
#define AFINFO( msg) {}
#endif
//###################################################################################
//
//###################################################################################
