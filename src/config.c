//
// ======================================================================
// Copyright (C) 2000-2003 Instrumental Software Technologies, Inc. (ISTI)
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. If modifications are performed to this code, please enter your own
// copyright, name and organization after that of ISTI.
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in
// the documentation and/or other materials provided with the
// distribution.
// 3. All advertising materials mentioning features or use of this
// software must display the following acknowledgment:
// "This product includes software developed by Instrumental
// Software Technologies, Inc. (http://www.isti.com)"
// 4. If the software is provided with, or as part of a commercial
// product, or is used in other commercial software products the
// customer must be informed that "This product includes software
// developed by Instrumental Software Technologies, Inc.
// (http://www.isti.com)"
// 5. The names "Instrumental Software Technologies, Inc." and "ISTI"
// must not be used to endorse or promote products derived from
// this software without prior written permission. For written
// permission, please contact "info@isti.com".
// 6. Products derived from this software may not be called "ISTI"
// nor may "ISTI" appear in their names without prior written
// permission of Instrumental Software Technologies, Inc.
// 7. Redistributions of any form whatsoever must retain the following
// acknowledgment:
// "This product includes software developed by Instrumental
// Software Technologies, Inc. (http://www.isti.com/)."
// 8. Redistributions of source code, or portions of this source code,
// must retain the above copyright notice, this list of conditions
// and the following disclaimer.
// THIS SOFTWARE IS PROVIDED BY INSTRUMENTAL SOFTWARE
// TECHNOLOGIES, INC. "AS IS" AND ANY EXPRESSED OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED.  IN NO EVENT SHALL INSTRUMENTAL SOFTWARE TECHNOLOGIES,
// INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include <stdio.h>
#include <string.h>
#include "q3302dali.h"
#include "config.h"
#include "platform.h"


// strncasecmp isn't in win32, but strnicmp is.  They do the same
// thing and take the same args
#ifdef WIN32
#  define strncasecmp _strnicmp
#endif
Configuration gConfig;
unsigned char TypeTrace;         /* Trace EW type for logo */
unsigned char TypeMseed;         /* Mseed EW type for logo */
unsigned char TypeHB;            /* HB=HeartBeat EW type for logo */
unsigned char TypeErr;           /* Error EW type for logo */
unsigned char QModuleId;

/*
 * Does this line contain useful information
 * (is it worth parsing)
 */
int isUsefulLine(char *line) {
    char *pos;
    /* take care of the simple cases */
    if( !strlen(line) || line[0] == '\n' || line[0] == '#' ) {
        return FALSE;
    }
    pos = line;
    while(pos) {
        if(*pos == '#' || *pos == '\0') {
            return FALSE;
        }

        if(*pos != ' ' && *pos != '\t') {
            return TRUE;
        }
        pos++;
    }
   return FALSE;
}


/*
 * Read the config file and populate the gConfig structure
 */
int readConfig(char *configFile) {
  setupDefaultConfiguration();
  strcpy(gConfig.ConfigFileName, configFile);
  if(!k_open(configFile)) {
    fprintf(stderr, "Unable to open config file: %s\n", configFile);
    return -1;
  }

  /*
   * read each line
   */
  while(k_rd()) {

    /* ignore comment/blank lines */
    if(!isUsefulLine(k_get())) {
      continue;
    }

    k_str();

    /*
     * check for the various config items
     */

    if(k_its("ModuleId")) {
        // ignore
      //strcpy(gConfig.ModuleId, k_str());
    } else if(k_its("RingName")) {
        // ignore
      //strcpy(gConfig.RingName, k_str());
      //gConfig.RingKey = GetKey(gConfig.RingName);
    } else if(k_its("HeartbeatInt")) {
        // ignore
      //gConfig.HeartbeatInt = k_int();
    } else if(k_its("Verbosity")) {
      gConfig.Verbosity = k_int();
    } else if(k_its("LogFile")) {
      gConfig.LogFile = k_int();
    } else if(k_its("DataLinkHost")) {
        strcpy(gConfig.datalinkHost, k_str());
    } else if(k_its("DataLinkPort")) {
        gConfig.datalinkPort = k_int();
    } else if(k_its("FlushLatency")) {
      gConfig.FlushLatency = k_int();
    } else if(k_its("ReconnectInterval")) {
      gConfig.ReconnectInterval = k_int();
    } else if(k_its("IPAddress")) {
      strcpy(gConfig.IPAddress, k_str());
    } else if(k_its("BasePort")) {
      gConfig.baseport = k_int();
    } else if(k_its("DataPort")) {
      gConfig.dataport = k_int();
    } else if(k_its("RegistrationCyclesLimit")) {
      gConfig.RegistrationCyclesLimit = k_int();
    } else if(k_its("SerialNumber")) {
      strcpy(gConfig.serialnumber, k_str());
    } else if(k_its("AuthCode")) {
      strcpy(gConfig.authcode, k_str());
    } else if(k_its("MiniseedMask")) {
      gConfig.miniseedMode = k_int();
    } else if(k_its("OneSecMask")) {
      gConfig.onesecMode = k_int();
    } else if(k_its("StatusInterval")) {
      gConfig.statusinterval = k_int();
    } else if(k_its("LogLevel")) {
      // Handle the loglevel stuff here
      int32 logLevel = 0;
      char *tok;
      char localInput[255];
      strcpy(localInput, k_com());
      tok = strtok(localInput, ", ");

      while(tok != NULL) {
	if(!strncasecmp("SD", tok, 2)) {
	  logLevel |= VERB_SDUMP;
	}
	if(!strncasecmp("CR", tok, 2)) {
	  logLevel |= VERB_RETRY;
	}
	if(!strncasecmp("RM", tok, 2)) {
	  logLevel |= VERB_REGMSG;
	}
	if(!strncasecmp("VB", tok, 2)) {
	  logLevel |= VERB_LOGEXTRA;
	}
	if(!strncasecmp("SM", tok, 2)) {
	  logLevel |= VERB_AUXMSG;
	}
	if(!strncasecmp("PD", tok, 2)) {
	  logLevel |= VERB_PACKET;
	}
	if(tok[0] == '#') {
	  // we don't want to parse anything after a comment starts
	  tok = NULL;
	} else {
	  tok = strtok(NULL, ", ");
	}
      }

      gConfig.LogLevel = logLevel;

    } else if(k_its("SourcePortControl")) {
      gConfig.SourcePortControl = k_int();
    } else if(k_its("SourcePortData")) {
      gConfig.SourcePortData = k_int();
    } else if(k_its("FailedRegistrationsBeforeSleep")) {
      gConfig.FailedRegistrationsBeforeSleep = k_int();
    } else if(k_its("MinutesToSleepBeforeRetry")) {
      gConfig.MinutesToSleepBeforeRetry = k_int();
    } else if(k_its("Dutycycle_MaxConnectTime")) {
      gConfig.Dutycycle_MaxConnectTime = k_int();
    } else if(k_its("Dutycycle_SleepTime")) {
      gConfig.Dutycycle_SleepTime = k_int();
    } else if(k_its("Dutycycle_BufferLevel")) {
      gConfig.Dutycycle_BufferLevel = k_int();
    } else if(k_its("ContinuityFileDirectory")) {
      strcpy(gConfig.ContFileDir, k_str());
    } else {
      fprintf(stderr, "%s: Unknown config command (%s)\n", Q3302DALI_NAME, k_get());
    }
  }

  return 1;
}


/**
 * Set all of the config items to rational defaults
 */
void setupDefaultConfiguration() {
  gConfig.RegistrationCyclesLimit = 5;
  gConfig.HeartbeatInt = 10;
  gConfig.ReconnectInterval = 10;
  gConfig.LogFile = 2;
  gConfig.baseport = 5330;
  gConfig.dataport = 2;
  strcpy(gConfig.authcode, "0");
  strcpy(gConfig.ContFileDir, "");
  gConfig.statusinterval = 180;
  gConfig.LogLevel = VERB_SDUMP|VERB_REGMSG|VERB_LOGEXTRA;
  gConfig.SourcePortControl = 0;
  gConfig.SourcePortData = 0;
  gConfig.FailedRegistrationsBeforeSleep = 5;
  gConfig.MinutesToSleepBeforeRetry = 3;
  gConfig.Dutycycle_MaxConnectTime = 0;
  gConfig.Dutycycle_SleepTime = 0;
  gConfig.Dutycycle_BufferLevel = 0;
  gConfig.miniseedMode = 0;
  gConfig.onesecMode = 1; // OSF_ALL
}

void printConfigStructToLog() {

  fprintf(stdout, "+++ Current Configuration:\n");
  fprintf(stdout, "--- ConfigFileName: %s\n", gConfig.ConfigFileName);
    fprintf(stdout, "--- DatalinkHost: %s\n", gConfig.datalinkHost);
    fprintf(stdout, "--- DatalinkPort: %d\n", gConfig.datalinkPort);
  fprintf(stdout, "--- LogFile: %d\n", gConfig.LogFile);
  fprintf(stdout, "--- IPAddress: %s\n", gConfig.IPAddress);
  fprintf(stdout, "--- BasePort: %d\n", gConfig.baseport);
  fprintf(stdout, "--- DataPort: %d\n", gConfig.dataport);
  fprintf(stdout, "--- SerialNumber: %s\n", gConfig.serialnumber);
  fprintf(stdout, "--- AuthCode: %s\n", gConfig.authcode);
  fprintf(stdout, "--- ContinuityFileDirectory: %s\n", gConfig.ContFileDir);
  fprintf(stdout, "--- StatusInterval: %d\n", gConfig.statusinterval);
  fprintf(stdout, "--- LogLevel: %s%s%s%s%s%s\n",
	     gConfig.LogLevel & VERB_SDUMP ? "SD " : "",
	     gConfig.LogLevel & VERB_RETRY ? "CR " : "",
	     gConfig.LogLevel & VERB_REGMSG ? "RM " : "",
	     gConfig.LogLevel & VERB_LOGEXTRA ? "VB " : "",
	     gConfig.LogLevel & VERB_AUXMSG ? "SM " : "",
	     gConfig.LogLevel & VERB_PACKET ? "PD " : ""
	     );
  fprintf(stdout, "--- SourcePortControl: %d\n", gConfig.SourcePortControl);
  fprintf(stdout, "--- SourcePortData: %d\n", gConfig.SourcePortData);
  fprintf(stdout, "--- FailedRegistrationsBeforeSleep: %d\n", gConfig.FailedRegistrationsBeforeSleep);
  fprintf(stdout, "--- MinutesToSleepBeforeRetry: %d\n", gConfig.MinutesToSleepBeforeRetry);
  fprintf(stdout, "--- Dutycycle_MaxConnectTime: %d\n", gConfig.Dutycycle_MaxConnectTime);
  fprintf(stdout, "--- Dutycycle_SleepTime: %d\n", gConfig.Dutycycle_SleepTime);
  fprintf(stdout, "--- Dutycycle_BufferLevel: %d\n", gConfig.Dutycycle_BufferLevel);
  fprintf(stdout, "--- onesecMode: %d\n", gConfig.onesecMode);
  fprintf(stdout, "--- miniseedMode: %d\n", gConfig.miniseedMode);
}
