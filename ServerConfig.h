#ifndef _SERVER_CONFIG_H_
#define _SERVER_CONFIG_H_

/* JDBC Project Files */
#include "JDBCServer.h"

/**
 * File: ServerConfig.h.
 *
 * Header file for the code that establishes and modifies the runtime configuration
 * of the JDBC Server.
 *
 * Header file corresponding to code file ServerConfig.c.
 *
 */

/* Function prototypes */

int getConfigurationParameters(char * configFileName);

int set_config_int(int configparam, int value, int err_num,workerDescriptionEntry *wdePtr);

int validate_config_int(int testvalue, int mode, int configparam,
                        int minvalue, int maxvalue,int def_available,
                        int defvalue, int err_num, int warn_num,
                        workerDescriptionEntry *wdePtr);

int set_config_string(int configparam, char *valuePtr, int err_num,
                      workerDescriptionEntry *wdePtr);

int validate_config_string(char * testvaluePtr, int mode, int configparam,
                           int minvaluelen, int maxvaluelen,int def_available,
                           char * defvaluePtr, int err_num, int warn_num,
                           workerDescriptionEntry *wdePtr);

int set_config_filename(int configparam, char *valuePtr, int err_num,
                        workerDescriptionEntry *wdePtr);

int validate_config_filename(char * testvaluePtr, int mode, int configparam,
                             int minvaluelen, int maxvaluelen,int def_available,
                             char * defvaluePtr, int err_num, int warn_num,
                             workerDescriptionEntry *wdePtr);

int set_config_userid(int configparam, char *valuePtr, int err_num,
                      workerDescriptionEntry *wdePtr);

int validate_config_userid(char * testvaluePtr, int mode, int configparam,
                           int minvaluelen, int maxvaluelen,int def_available,
                           char * defvaluePtr, int err_num, int warn_num,
                           workerDescriptionEntry *wdePtr);

int processConfigFile(char * configurationFileName);

int isValidKeyinId(char * string);

int get_RSA_bdi();

int get_application_group_number();

#ifdef XABUILD  /* XA JDBC Server */
int get_UDS_ICR_bdi();
#endif          /* XA JDBC Server */

#endif /* _SERVER_CONFIG_H_ */
