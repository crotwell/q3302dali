//
//  q3302dali.h
//  
//
//  Created by Philip Crotwell on 1/29/19.
//

#ifndef q3302dali_h
#define q3302dali_h

#define Q3302DALI_NAME "Q3302DALI"
#define Q3302DALI_VERSION "0.0"
#define Q3302DALI_BUILD "0"

// lib330 stuff
#include <libclient.h>
#include <libtypes.h>
#include <libmsgs.h>
/*
 * Somewhere along the way we end up with a bunch of pascalisms defined.
 * This one is particularly important elsewhere
 */
#undef mod
#undef addr


#include <libmseed.h>
#include <libdali.h>

#include <sys/types.h>

#define PACKAGE "q3302dali"

#ifdef _linux
#include <stdint.h>
#endif

#ifdef WIN32
#define int8 __int8
#define int16 __int16
#define int32 __int32
#define int64 __int64

#define uint8 unsigned __int8
#define uint16 unsigned __int16
#define uint32 unsigned __int32
#define uint64 unsigned __int64
#else

#define int8 int8_t
#define int16 int16_t
#define int32 int32_t
#define int64 int64_t

#define uint8 unsigned int8
#define uint16 unsigned int16
#define uint32 unsigned int32
#define uint64 uint64_t
#endif


static void logit_msg( const char * );
static void logit_err( const char * );


void lib330Interface_initialize();
void lib330Interface_handlerError(enum tliberr errcode);
void lib330Interface_initializeCreationInfo();
void lib330Interface_initializeRegistrationInfo();
void lib330Interface_stateCallback(pointer p);
void lib330Interface_msgCallback(pointer p);
void lib330Interface_1SecCallback(pointer p);
void lib330Interface_miniCallback(pointer p);
void lib330Interface_libStateChanged(enum tlibstate newState);
void lib330Interface_displayStatusUpdate();
void lib330Interface_startDataFlow();
void lib330Interface_startRegistration();
void lib330Interface_changeState(enum tlibstate newState, enum tliberr reason);
void lib330Interface_startDeregistration();
void lib330Interface_ping();
void lib330Interface_cleanup();
int lib330Interface_waitForState(enum tlibstate waitFor, int maxSecondsToWait);
enum tlibstate lib330Interface_getLibState();

void cleanup();
void cleanupAndExit(int i);
static int packtraces ( MSTrace *mst, int flush, hptime_t flushtime );
static void sendrecord ( char *record, int reclen, void *handlerdata );
static void usage ();
static int handle_opts(int argc, char ** argv);
static void logmststats ( MSTrace *mst );

#endif /* q3302dali_h */
