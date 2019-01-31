//
//  q3302dali.c
//  q3302dali
//
//  Created by Philip Crotwell on 1/29/19.
//

#include <stdio.h>
#include "q3302dali.h"
#include "config.h"


/* Per-trace statistics */
typedef struct tracestats_s
{
  hptime_t earliest;
  hptime_t latest;
  hptime_t update;
  hptime_t xmit;
  int64_t pktcount;
  int64_t reccount;
} TraceStats;

static int verbose     = 0;
static int stopsig     = 0;        /* 1: termination requested, 2: termination and no flush */

enum tlibstate currentLibState;
tpar_register registrationInfo;
tpar_create creationInfo;
tcontext stationContext;


static char *rsaddr    = 0;        /* DataLink/ringserver receiver address in IP:port format */
static DLCP *dlcp      = 0;        /* DataLink connection handle */


static MSTraceGroup *mstg = 0;     /* Staging buffer of data for making miniSEED */

static int flushlatency = 300;     /* Flush data buffers if not updated for latency in seconds */
static int reconnectinterval = 10; /* Interval to wait between reconnection attempts in seconds */
static int int32encoding = DE_STEIM2; /* Encoding for 32-bit integer data */

#define MAX_WAIT_STATE_BEFORE_EXIT 240 /* max seconds to sit in WAIT for reg state */
static unsigned long  MAIN_WHILE_USLEEP =(unsigned long)1e5; /* 1 sec=1e6, sleep 1/10 sec */

#ifndef _WIN32
/********************* Signal handling  routines ******************/

void cleanup() {
  stopsig = 1;
  lib330Interface_cleanup();

  /* Flush all remaining data streams and close the connections */
  packtraces (NULL, 1, HPTERROR);
  if ( dlcp->link != -1 )
    dl_disconnect (dlcp);

  if ( verbose )
  {
    MSTrace *mst = mstg->traces;
    while ( mst )
    {
      logmststats (mst);
      mst = mst->next;
    }
  }

  ms_log (1, "Terminating %s\n", Q3302DALI_NAME);

}

void cleanupAndExit(int i) {
  cleanup();
  // give main while a chance to finish
  dlp_usleep (2*MAIN_WHILE_USLEEP);
  exit(i);
}

static void dummy_handler ( int sig ) { }

static void term_handler ( int sig )
{
  stopsig = 1;
}

static void ThreadSignalHandler ( int sig )
{
  switch (sig)
  {
    case SIGUSR1:
      pthread_exit( (void *)NULL );
  }
}
#endif

int main ( int argc, char **argv )
{
  time_t now;
  time_t tfirsttry;            /* time of first connection attempt */
  int    quit;
  int    retryCount;           /* to prevent flooding the log file */
  int    connected;            /* connection flag */

  int registration_count = 0;
  int wait_counter = 0;
  time_t lastStatusUpdate;

#ifndef _WIN32
  /* Signal handling, use POSIX calls with standardized semantics */
  struct sigaction sa;

  sa.sa_handler = dummy_handler;
  sa.sa_flags = SA_RESTART;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGALRM, &sa, NULL);

  sa.sa_handler = term_handler;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  sa.sa_handler = SIG_IGN;
  sigaction(SIGHUP, &sa, NULL);
  sigaction(SIGPIPE, &sa, NULL);

  /* Signal-handling function that needs to be inherited by threads */
  sa.sa_handler = ThreadSignalHandler;
  sigaction(SIGUSR1, &sa, NULL);
#endif

/* Initialize the verbosity for the dl_log function */
dl_loginit (verbose-1, &print_timelog, "", &print_timelog, "");

/* Initialize the logging for the ms_log family */
ms_loginit (&print_timelog, NULL, &print_timelog, NULL);

  if (argc<2) {
    usage();
    exit(1);
  }
  // then read config
  handle_opts(argc, argv);
  verbose = gConfig.Verbosity;
  flushlatency = gConfig.FlushLatency;
  reconnectinterval = gConfig.ReconnectInterval;

  lib330Interface_initialize();

  /* Initialize trace buffer */
  if ( ! (mstg = mst_initgroup ( mstg )) )
  {
    ms_log (2, "Cannot initialize MSTraceList\n");
    exit (1);
    }


  char tmps[285];
  sprintf(tmps, "%s:%d", gConfig.datalinkHost, gConfig.datalinkPort);
  fprintf(stderr, "datalink to %s", tmps);
  rsaddr = tmps;
  /* Allocate and initialize DataLink connection description */
  if ( ! (dlcp = dl_newdlcp (rsaddr, PACKAGE)) )
  {
    fprintf (stderr, "Cannot allocation DataLink descriptor\n");
    exit (1);
  }

  /* Connect to destination DataLink server */
  if ( dl_connect (dlcp) < 0 )
  {
    ms_log (1, "Initial connection to DataLink server (%s) failed, will retry later\n", dlcp->addr);
    dl_disconnect (dlcp);
  }
  else if ( verbose )
    ms_log (1, "Connected to ringserver at %s\n", dlcp->addr);

  /* to prevent flooding the log file during long reconnect attempts */
  retryCount=0;  /* it may be reset elsewere */


  // keep trying to register as long as 1) we haven't and 2) we're
  // not supposed to die and 3) we don't hit the max retry counts (default 5)
  lib330Interface_startRegistration();
  registration_count++;
  while(!lib330Interface_waitForState(LIBSTATE_RUN, 120) && ! stopsig ) {
    lib330Interface_startRegistration();
    registration_count++;
    if (registration_count > gConfig.RegistrationCyclesLimit)
    {
      fprintf(stderr, "q3302ew: regsitration limit of %d tries reached, exiting", registration_count);
      cleanupAndExit(0);
    }
    else
    {
      fprintf(stderr, "q3302ew: retrying registration: %d\n", registration_count);
    }
  }

  // now we're registered and getting data.  We'll keep doing so until we're told to stop.
  lastStatusUpdate = time(NULL);
  while( ! stopsig) {
    if( (time(NULL) - lastStatusUpdate) >= gConfig.statusinterval ) {
      lib330Interface_displayStatusUpdate();
      lastStatusUpdate = time(NULL);
    }
    dlp_usleep (MAIN_WHILE_USLEEP);
    /* new code to detect if we fall into a WAIT for registration state for too long */
    if (lib330Interface_waitForState(LIBSTATE_WAIT, 1) == 1) {
      wait_counter++;
    } else {
      wait_counter = 0;
    }
    if (wait_counter >= MAX_WAIT_STATE_BEFORE_EXIT) {
      fprintf(stderr, "q3302ew: hung in wait state for more than: %d seconds\n", MAX_WAIT_STATE_BEFORE_EXIT);
      cleanup();
    }
  }
  // we've been asked to terminate
  cleanup();
  return(0);
} // end main


/**
 * Fill out the creationInfo struct.
 */
void lib330Interface_initializeCreationInfo() {
  // Fill out the parts of the creationInfo that we know about
  uint64 serial;
  char continuityFile[1024];

  serial = strtoll(gConfig.serialnumber, NULL, 16);

  memcpy(creationInfo.q330id_serial, &serial, sizeof(uint64));
  switch(gConfig.dataport) {
    case 1:
      creationInfo.q330id_dataport = LP_TEL1;
      break;
    case 2:
      creationInfo.q330id_dataport = LP_TEL2;
      break;
    case 3:
      creationInfo.q330id_dataport = LP_TEL3;
      break;
    case 4:
      creationInfo.q330id_dataport = LP_TEL4;
      break;
  }
  strncpy(creationInfo.q330id_station, "UNKN", 5);
  creationInfo.host_timezone = 0;
  strcpy(creationInfo.host_software, "Q3302DALI_NAME");
  if(strlen(gConfig.ContFileDir)) {
    sprintf(continuityFile, "%s/Q3302EW_cont_%s.bin", gConfig.ContFileDir, gConfig.ConfigFileName);
  } else {
    sprintf(continuityFile, "Q3302EW_cont_%s.bin", gConfig.ConfigFileName);
  }
  strcpy(creationInfo.opt_contfile, continuityFile);
  creationInfo.opt_verbose = gConfig.LogLevel;
  creationInfo.opt_zoneadjust = 1;
  creationInfo.opt_secfilter = gConfig.onesecMode;
  creationInfo.opt_minifilter = gConfig.miniseedMode;
  creationInfo.opt_aminifilter = 0;
  creationInfo.amini_exponent = 0;
  creationInfo.amini_512highest = -1000;
  creationInfo.mini_embed = 0;
  creationInfo.mini_separate = 1;
  creationInfo.mini_firchain = 0;
  creationInfo.call_minidata = lib330Interface_miniCallback;
  creationInfo.call_aminidata = lib330Interface_miniCallback;
  creationInfo.resp_err = LIBERR_NOERR;
  creationInfo.call_state = lib330Interface_stateCallback;
  creationInfo.call_messages = lib330Interface_msgCallback;
  creationInfo.call_secdata = lib330Interface_1SecCallback;
  creationInfo.call_lowlatency = NULL;
  fprintf(stderr, "onesecMode set to '%d'\n", creationInfo.opt_secfilter);
  fprintf(stderr, "miniseedMode set to '%d'\n", creationInfo.opt_minifilter);
}


/**
 * Set up the registration info structure from the config
 **/
void lib330Interface_initializeRegistrationInfo() {
  uint64 auth = strtoll(gConfig.authcode, NULL, 16);
  memcpy(registrationInfo.q330id_auth, &auth, sizeof(uint64));
  strcpy(registrationInfo.q330id_address, gConfig.IPAddress);
  registrationInfo.q330id_baseport = gConfig.baseport;
  registrationInfo.host_mode = HOST_ETH;
  strcpy(registrationInfo.host_interface, "");
  registrationInfo.host_mincmdretry = 5;
  registrationInfo.host_maxcmdretry = 40;
  registrationInfo.host_ctrlport = gConfig.SourcePortControl;
  registrationInfo.host_dataport = gConfig.SourcePortData;
  registrationInfo.opt_latencytarget = 0;
  registrationInfo.opt_closedloop = 0;
  registrationInfo.opt_dynamic_ip = 0;
  registrationInfo.opt_hibertime = gConfig.MinutesToSleepBeforeRetry;
  registrationInfo.opt_conntime = gConfig.Dutycycle_MaxConnectTime;
  registrationInfo.opt_connwait = gConfig.Dutycycle_SleepTime;
  registrationInfo.opt_regattempts = gConfig.FailedRegistrationsBeforeSleep;
  registrationInfo.opt_ipexpire = 0;
  registrationInfo.opt_buflevel = gConfig.Dutycycle_BufferLevel;
}


void lib330Interface_displayStatusUpdate() {
  enum tlibstate currentState;
  enum tliberr lastError;
  topstat libStatus;
  time_t rightNow = time(NULL);
  int i;

  currentState = lib_get_state(stationContext, &lastError, &libStatus);

  // do some internal maintenence if required (this should NEVER happen)
  if(currentState != lib330Interface_getLibState()) {
    string63 newStateName;
    fprintf(stderr, "XXX Current lib330 state mismatch.  Fixing...\n");
    lib_get_statestr(currentState, &newStateName);
    fprintf(stderr, "+++ State change to '%s'\n", newStateName);
    lib330Interface_libStateChanged(currentState);
  }


  // version and localtime
  fprintf(stderr, "+++ %s %s %s status for %s.  Local time: %s", Q3302DALI_NAME, Q3302DALI_VERSION, Q3302DALI_BUILD,
             libStatus.station_name, ctime(&rightNow));

  // BPS entries
  fprintf(stderr, "--- Bps from Q330 (min/hour/day): ");
  for(i=(int)AD_MINUTE; i <= (int)AD_DAY; i = i + 1) {
    if((int)libStatus.accstats[AC_READ][i] != (int)INVALID_ENTRY) {
      fprintf(stderr, "%dBps", (int) libStatus.accstats[AC_READ][i]);
    } else {
      fprintf(stderr, "---");
    }
    if(i != AD_DAY) {
      fprintf(stderr, "/");
    } else {
      fprintf(stderr, "\n");
    }
  }

  fprintf(stderr, "--- Packets from Q330 (min/hour/day): ");
  for(i=(int)AD_MINUTE; i <= (int)AD_DAY; i = i + 1) {
    if((int)libStatus.accstats[AC_PACKETS][i] != (int)INVALID_ENTRY) {
      fprintf(stderr, "%dPkts", (int) libStatus.accstats[AC_PACKETS][i]);
    } else {
      fprintf(stderr, "---");
    }
    if(i != AD_DAY) {
      fprintf(stderr, "/");
    } else {
      fprintf(stderr, "\n");
    }
  }

  // percent of the buffer left, and the clock quality
  fprintf(stderr, "--- Q330 Packet Buffer Available: %d Clock Quality: %d\n", 100-((int)libStatus.pkt_full),
             (int)libStatus.clock_qual);
}


/**
 * Handle and log errors coming from lib330
 **/
void lib330Interface_handleError(enum tliberr errcode) {
  string63 errmsg;
  lib_get_errstr(errcode, &errmsg);
  fprintf(stderr, "XXX : Encountered error: %s\n", errmsg);
}

/**
 * Initialize the lib330 interface.  Setting up the creation info
 * registration info, log all module revs etc...
 **/
void lib330Interface_initialize() {
  pmodules modules;
  const tmodule *module;
  int x;

#ifdef WIN32
  WSAStartup(0x101, &wdata);
#endif
  currentLibState = LIBSTATE_IDLE;
  lib330Interface_initializeCreationInfo();
  lib330Interface_initializeRegistrationInfo();

  modules = lib_get_modules();
  fprintf(stderr, "+++ Lib330 Modules:\n");
  for(x=0; x <= MAX_MODULES - 1; x++) {
    module = &(*modules)[x];
    if(!module->name[0]) {
      continue;
    }
    if( !(x % 4)) {
      if(x > 0) {
        fprintf(stderr, "\n");
      }
      if(x < MAX_MODULES-1) {
        fprintf(stderr, "+++ ");
      }
    }
    fprintf(stderr, "%s:%d ", module->name, module->ver);
  }
  fprintf(stderr, "\n");
  fprintf(stderr, "+++ Initializing station thread\n");
  lib_create_context(&(stationContext), &(creationInfo));
  if(creationInfo.resp_err == LIBERR_NOERR) {
    fprintf(stderr, "+++ Station thread created\n");
  } else {
    lib330Interface_handleError(creationInfo.resp_err);
  }

}

/**
 * Set the the interface up for the new state
 */
void lib330Interface_libStateChanged(enum tlibstate newState) {
  string63 newStateName;

  lib_get_statestr(newState, &newStateName);
  fprintf(stderr, "+++ State change to '%s'\n", newStateName);
  currentLibState = newState;

  /*
   ** We have no good reason for sitting in RUNWAIT, so lets just go
   */
  if(currentLibState == LIBSTATE_RUNWAIT) {
    lib330Interface_startDataFlow();
  }
}

/**
 * What state are we currently in?
 **/
enum tlibstate lib330Interface_getLibState() {
  return currentLibState;
}

/**
 * Start acquiring data
 **/
void lib330Interface_startDataFlow() {
  fprintf(stderr, "+++ Requesting dataflow to start\n");
  lib330Interface_changeState(LIBSTATE_RUN, LIBERR_NOERR);
}

/**
 * Initiate the registration process
 **/
void lib330Interface_startRegistration() {
  enum tliberr errcode;
  lib330Interface_ping();
  fprintf(stderr, "+++ Starting registration with Q330\n");
  errcode = lib_register(stationContext, &(registrationInfo));
  if(errcode != LIBERR_NOERR) {
    lib330Interface_handleError(errcode);
  }
}

/**
 * Request that the lib change its state
 **/
void lib330Interface_changeState(enum tlibstate newState, enum tliberr reason) {
  lib_change_state(stationContext, newState, reason);
}

void lib330Interface_cleanup() {
  enum tliberr errcode;
  fprintf(stderr, "+++ Cleaning up lib330 Interface\n");
  lib330Interface_startDeregistration();
  while(lib330Interface_getLibState() != LIBSTATE_IDLE) {
    dlp_usleep(MAIN_WHILE_USLEEP);
  }
  lib330Interface_changeState(LIBSTATE_TERM, LIBERR_CLOSED);
  while(lib330Interface_getLibState() != LIBSTATE_TERM) {
    dlp_usleep(MAIN_WHILE_USLEEP);
  }
  errcode = lib_destroy_context(&(stationContext));
  if(errcode != LIBERR_NOERR) {
    lib330Interface_handleError(errcode);
  }
  fprintf(stderr, "+++ lib330 Interface closed\n");
}

/**
 * Request a deregistration
 **/
void lib330Interface_startDeregistration() {
  fprintf(stderr, "+++ Starting deregistration from Q330\n");
  lib330Interface_changeState(LIBSTATE_IDLE, LIBERR_NOERR);
}

/**
 * Send a ping to the station
 **/
void lib330Interface_ping() {
  lib_unregistered_ping(stationContext, &(registrationInfo));
}

/**
 * Wait for a particular state to arrive, or timeout after maxSecondsToWait.
 * return value indicated whether we reached the desired state or not
 **/
int lib330Interface_waitForState(enum tlibstate waitFor, int maxSecondsToWait) {
  int i;
  int maxLoop = maxSecondsToWait / MAIN_WHILE_USLEEP;
  if (maxLoop < 2) { maxLoop = 2;}
  for(i=0; i < maxSecondsToWait; i++) {
    if(lib330Interface_getLibState() != waitFor) {
      dlp_usleep(MAIN_WHILE_USLEEP);
    } else {
      return 1;
    }
  }
  return 0;
}

/**
 * Below here are several callbacks for lib330 to use for various events
 **/

void lib330Interface_stateCallback(pointer p){
  tstate_call *state;

  state = (tstate_call *)p;

  if(state->state_type == ST_STATE) {
    lib330Interface_libStateChanged((enum tlibstate)state->info);
  }
}

void lib330Interface_msgCallback(pointer p){
  tmsg_call *msg = (tmsg_call *) p;
  string95 msgText;
  char dataTime[32];

  lib_get_msg(msg->code, &msgText);

  // we don't need to worry about current time, the log system handles that
  //jul_string(msg->timestamp, &currentTime);
  if(!msg->datatime) {
    fprintf(stderr, "{%d} %s %s\n", msg->code, msgText, msg->suffix);
  } else {
    //jul_string(msg->datatime, (char *) &dataTime);
    fprintf(stderr, "{%d} [%s] %s %s\n", msg->code, dataTime, msgText, msg->suffix);
  }
}

void lib330Interface_1SecCallback(pointer p){
  tonesec_call *data = (tonesec_call *) p;

  static struct blkt_1000_s Blkt1000;
  static struct blkt_1001_s Blkt1001;
  MSTrace *mst = NULL;
  static MSRecord *mstemplate = NULL;
  static MSRecord *msr = NULL;


  char *sta, *net;
  char netsta[10];

  fprintf(stderr, "OneSec for %s {%d} %d\n", data->channel, data->rate, data->filter_bits);

  /* Set up MSRecord template */
  if ( (msr = msr_init (msr)) == NULL )
  {
    ms_log (2, "Cannot initialize packing template\n");
    return;
  }

  // seperate the station from the net
  strcpy(netsta, data->station_name);
  net = netsta;
  sta = netsta;
  while(*sta != '-' && *sta != '\0') {
    sta++;
  }
  if(*sta == '-') {
    *sta = '\0';
    sta++;
  } else {
    char *tmp;
    tmp = sta;
    sta = net;
    net = tmp;
  }
  ms_strncpclean (msr->network, net, 2);
  ms_strncpclean (msr->station, sta, 5);
  ms_strncpclean (msr->location, data->location, 2);
  ms_strncpclean (msr->channel, data->channel, 3);
  msr->starttime = (hptime_t)(MS_EPOCH2HPTIME(data->timestamp));
  msr->samprate = data->rate;
  // handle the sub 1hz channels differently
  if(data->rate > 0) {
    msr->numsamples = data->rate;
  } else {
    msr->numsamples = 1;
  }
  msr->samplecnt = msr->numsamples;
  msr->datasamples = data->samples;
  msr->sampletype = 'i';

  processMseed(msr);
}

/* miniseed record mode from q330 */
void lib330Interface_miniCallback(pointer p){

  tminiseed_call *data = (tminiseed_call *) p;

  fprintf(stderr, "Miniseed for %s {%d} %d\n", data->channel, data->data_size, data->filter_bits);

  processMseed(data->data_address);

}

static void processMseed(MSRecord *msr)
{
  MSTrace *mst = NULL;
  int recordspacked = 0;
  /* Add data to trace buffer, creating new entry or extending as needed */
  if ( ! (mst = mst_addmsrtogroup (mstg, msr, 1, -1.0, -1.0)) )
  {
    ms_log (3, "Cannot add data to trace buffer!\n");
    return;
  }

  /* To keep small variations in the sample rate or time base from accumulating
   * to large errors, re-base the time of the buffer by back projecting from
   * the endtime, which is calculated from the tracebuf starttime and number
   * of samples.  In essence, this maintains a time line based on the starttime
   * of received tracebufs.  It also retains the variations of the sample rate
   * and other characteristics of the original data stream to some degree. */

  mst->starttime = mst->endtime - (hptime_t) (((double)(mst->numsamples - 1) / mst->samprate * HPTMODULUS) + 0.5);

  /* Allocate & init per-trace stats structure if needed */
  if ( ! mst->prvtptr )
  {
    if ( ! (mst->prvtptr = malloc (sizeof(TraceStats))) )
    {
      ms_log (3, "Cannot allocate buffer for trace stats!\n");
      return;
    }

    ((TraceStats *)mst->prvtptr)->earliest = HPTERROR;
    ((TraceStats *)mst->prvtptr)->latest = HPTERROR;
    ((TraceStats *)mst->prvtptr)->update = HPTERROR;
    ((TraceStats *)mst->prvtptr)->xmit = HPTERROR;
    ((TraceStats *)mst->prvtptr)->pktcount = 0;
    ((TraceStats *)mst->prvtptr)->reccount = 0;
  }

  ((TraceStats *)mst->prvtptr)->update = dlp_time();
  ((TraceStats *)mst->prvtptr)->pktcount += 1;

  if ( (recordspacked = packtraces (mst, 0, HPTERROR)) < 0 )
  {
    ms_log (3, "Cannot pack trace buffer or send records!\n");
    ms_log (3, "  %s.%s.%s.%s %lld\n",
            mst->network, mst->station, mst->location, mst->channel,
            (long long int) mst->numsamples);
    return;
  }
}

/*********************************************************************
 * packtraces:
 *
 * Package remaining data in buffer(s) into miniSEED records.  If mst
 * is NULL all streams will be packed, otherwise only the specified
 * stream will be packed.
 *
 * If the flush argument is true the stream buffers will be flushed
 * completely, otherwise records are only packed when enough samples
 * are available to fill a record.
 *
 * Returns the number of records packed on success and -1 on error.
 *********************************************************************/
static int packtraces ( MSTrace *mst, int flush, hptime_t flushtime )
{
  static struct blkt_1000_s Blkt1000;
  static struct blkt_1001_s Blkt1001;
  static MSRecord *mstemplate = NULL;

  MSTrace *prevmst;
  void *handlerdata = mst;
  int trpackedrecords = 0;
  int packedrecords = 0;
  int flushflag = flush;
  int encoding = -1;

  /* Set up MSRecord template, include blockette 1000 and 1001 */
  if ( (mstemplate = msr_init (mstemplate)) == NULL )
  {
    ms_log (2, "Cannot initialize packing template\n");
    return -1;
  }
  else
  {
    mstemplate->dataquality = 'D';

    /* Add blockettes 1000 & 1001 to template */
    memset (&Blkt1000, 0, sizeof(struct blkt_1000_s));
    msr_addblockette (mstemplate, (char *) &Blkt1000,
                      sizeof(struct blkt_1001_s), 1000, 0);
    memset (&Blkt1001, 0, sizeof(struct blkt_1001_s));
    msr_addblockette (mstemplate, (char *) &Blkt1001,
                      sizeof(struct blkt_1001_s), 1001, 0);
  }

  if ( mst )
  {
    if ( mst->sampletype == 'f' )
      encoding = DE_FLOAT32;
    else if ( mst->sampletype == 'd' )
      encoding = DE_FLOAT64;
    else
      encoding = int32encoding;

    strcpy (mstemplate->network, mst->network);
    strcpy (mstemplate->station, mst->station);
    strcpy (mstemplate->location, mst->location);
    strcpy (mstemplate->channel, mst->channel);

    trpackedrecords = mst_pack (mst, sendrecord, handlerdata, 512,
                                encoding, 1, NULL, flushflag,
                                verbose-2, mstemplate);

    if ( trpackedrecords == -1 )
      return -1;

    packedrecords += trpackedrecords;
  }
  else
  {
    mst = mstg->traces;
    prevmst = NULL;

    while ( mst && stopsig != 2 )
    {
      if ( mst->numsamples > 0 )
      {
        if ( mst->sampletype == 'f' )
          encoding = DE_FLOAT32;
        else if ( mst->sampletype == 'd' )
          encoding = DE_FLOAT64;
        else
          encoding = int32encoding;

        /* Flush data buffer if update time is less than flushtime */
        if ( flush == 0 && mst->prvtptr && flushtime != HPTERROR )
          if (((TraceStats *)mst->prvtptr)->update < flushtime )
          {
            ms_log (1, "Flushing data buffer for %s_%s_%s_%s\n",
                    mst->network, mst->station, mst->location, mst->channel);
            flushflag = 1;
          }

        strcpy (mstemplate->network, mst->network);
        strcpy (mstemplate->station, mst->station);
        strcpy (mstemplate->location, mst->location);
        strcpy (mstemplate->channel, mst->channel);

        trpackedrecords = mst_pack (mst, sendrecord, handlerdata, 512,
                                    encoding, 1, NULL, flushflag,
                                    verbose-2, mstemplate);

        if ( trpackedrecords == -1 )
          return -1;

        packedrecords += trpackedrecords;
      }

      /* Remove trace buffer entry if no samples remaining */
      if ( mst->numsamples <= 0 )
      {
        MSTrace *nextmst = mst->next;

        if ( verbose )
          logmststats (mst);

        if ( ! prevmst )
          mstg->traces = mst->next;
        else
          prevmst->next = mst->next;

        mst_free (&mst);

        mst = nextmst;
      }
      else
      {
        prevmst = mst;
        mst = mst->next;
      }
    }
  }

  return packedrecords;
}  /* End of packtraces() */

/*********************************************************************
 * sendrecord:
 *
 * Routine called to send a record to the DataLink server.
 *
 * Returns 0
 *********************************************************************/
static void sendrecord ( char *record, int reclen, void *handlerdata )
{
  static MSRecord *msr = NULL;
  MSTrace *mst = handlerdata;
  TraceStats *stats;
  hptime_t endtime;
  char streamid[100];
  int writeack = 0;
  int rv;

  if ( ! record )
    return;

  /* Parse Mini-SEED header */
  if ( (rv = msr_unpack (record, reclen, &msr, 0, 0)) != MS_NOERROR )
  {
    ms_recsrcname (record, streamid, 0);
    ms_log (2, "Error unpacking %s: %s", streamid, ms_errorstr(rv));
    return;
  }

  /* Generate stream ID for this record: NET_STA_LOC_CHAN/MSEED */
  msr_srcname (msr, streamid, 0);
  strcat (streamid, "/MSEED");

  /* Determine high precision end time */
  endtime = msr_endtime (msr);

  if ( verbose >= 2 )
    ms_log (1, "Sending %s\n", streamid);

  /* Send record to server, loop */
  while ( dl_write (dlcp, record, reclen, streamid, msr->starttime, endtime, writeack) < 0 )
  {
    if ( dlcp->link == -1 )
      dl_disconnect (dlcp);

    if ( stopsig )
    {
      ms_log (2, "Termination signal with no connection to DataLink, the data buffers will be lost");
      stopsig = 2;
      break;
    }

    if ( ! reconnectinterval )
    {
      stopsig = 2;
      break;
    }
    else if ( dl_connect (dlcp) < 0 )
    {
      ms_log (2, "Error re-connecting to DataLink server: %s, sleeping\n", dlcp->addr);
      dlp_usleep (reconnectinterval * (unsigned long)1e6);
    }
  }

  /* Update stats */
  if ( mst )
  {
    stats = (TraceStats *)mst->prvtptr;

    if ( stats->earliest == HPTERROR || stats->earliest > msr->starttime )
      stats->earliest = msr->starttime;

    if ( stats->latest == HPTERROR || stats->latest < endtime )
      stats->latest = endtime;

    stats->xmit = dlp_time();
    stats->reccount += 1;
  }
}  /* End of sendrecord() */


/***************************************************************************
 * usage():
 *
 * Print usage message and exit.
 ***************************************************************************/
static void usage ( void )
{
  fprintf(stderr,"\n"
          "Usage: q3302dali [options] <exportserver:port> <ringserver:port>\n"
          "\n"
          "  -u            Print this usage message\n"
          "  -v            Be more verbose, mutiple flags can be used\n"
          "\n"
          "  -f latency    Internal buffer flush latency in seconds, default 300\n"
          "  -R interval   Reconnection interval/wait in seconds, default 10\n"
          "  -t timeout    Socket timeout in seconds, default 80\n"
          "  -Ie encoding  Specify encoding for 32-bit integers, default STEIM2\n"
          "  -Ar rate      Rate (approximate) to send hearbeats to server, default 120\n"
          "  -At text      Text for heartbeat to server, default='alive' or 'ImpAlive'\n"
          "  -Sr rate      Rate at which to expect heartbeats from server, default 60\n"
          "  -St text      Text expected in heartbeats from server, default='ExpAlive'\n"
          "  -m maxmsg     Maximum message size that can be received, default 4096\n"
          "  -Hl #/#/#     Specify the logo to use for heartbeats, default '0/0/3'\n"
          "                   #/#/# is the <inst id>/<mod id>/<heartbeat type>\n"
          "\n"
          "The export server and ringserver addresses are specified in host:port format\n"
          "\n");

  exit (1);
} /* End of usage() */


/* handle_opts() - handles any command line options.
 and sets the Progname extern to the
 base-name of the command.

 Returns:
 TRUE if options are parsed okay.
 FALSE if there are bad or conflicting
 options.

 */

static int handle_opts(int argc, char ** argv)  {

  if(argc != 2) {
    usage();
    fprintf(stderr,"Config file not specified\n");
    cleanupAndExit(1);
  }
  if (readConfig(argv[1]) == -1) {
    usage();
    fprintf(stderr,"Too many q3302ew.d config problems\n");
    cleanupAndExit(1);
  }
  return 1;
}

/***************************************************************************
 * print_timelog:
 *
 * Log message print handler.  Prefixes a local time string to the
 * message before printing.
 ***************************************************************************/
static void print_timelog ( char *msg )
{
  char timestr[100];
  time_t loc_time;

  /* Build local time string and cut off the newline */
  time(&loc_time);
  strcpy(timestr, asctime(localtime(&loc_time)));
  timestr[strlen(timestr) - 1] = '\0';

  fprintf (stderr, "%s - %s", timestr, msg);
}  /* End of print_timelog() */

/*********************************************************************
 * logmststats:
 *
 * Log MSTrace stats.
 *********************************************************************/
static void logmststats ( MSTrace *mst )
{
  TraceStats *stats;
  char etime[50];
  char ltime[50];
  char utime[50];
  char xtime[50];

  stats = (TraceStats *) mst->prvtptr;
  ms_hptime2mdtimestr (stats->earliest, etime, 1);
  ms_hptime2mdtimestr (stats->latest, ltime, 1);
  ms_hptime2mdtimestr (stats->update, utime, 1);
  ms_hptime2mdtimestr (stats->xmit, xtime, 1);

  ms_log (0, "%s_%s_%s_%s, earliest: %s, latest: %s\n",
          mst->network, mst->station, mst->location, mst->channel,
          (stats->earliest == HPTERROR) ? "NONE":etime,
          (stats->latest == HPTERROR) ? "NONE":ltime);
  ms_log (0, "  last update: %s, xmit time: %s\n",
          (stats->update == HPTERROR) ? "NONE":utime,
          (stats->xmit == HPTERROR) ? "NONE":xtime);
  ms_log (0, "  pktcount: %lld, reccount: %lld\n",
          (long long int) stats->pktcount,
          (long long int) stats->reccount);
}  /* End of logmststats() */
