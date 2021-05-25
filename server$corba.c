#include <otm-diag.h>
#include <stdio.h>

/* Standard C header files and OS2200 System Library files */
#include "corba.h"

/* The lines below (up to ending comment line) are #includes added to support the SGD access */
/* C header files */
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <task.h>
#include <sysutil.h>

/* RDMS and the C-Interface Library header files */
#include <convbyte.h>

/* JDBC Server's own header file */
#include "JDBCServer.h"
#include "skel.h"

extern serverGlobalData sgd;               /* The Server Global Data (SGD),*/
                                           /* visible to all Server activities. */


/* End of explicitly added #includes and #defines for sgd access */
/* XA Server debug code is added below (see references to sgd.debugXA) */

int main(void)
{
    CORBA_ORBid id = "2200";
    CORBA_ORB orb;
    CORBA_Object poa_ref;
    CORBA_Environment env;

    /* Beginning of code to get option bits, configuration file name, and set debugXA if needed */

    /*
     * Get the XA JDBC Server invocation parameters and
     * configuration file name - this sets sgd.option_bits
     * and sgd.configFileName. Then test if we are to
     * begin XA tracing.
     */
    getInvocationImageParameters();

    /* X-Option on processor invocation turns on XA debug */
    if (sgd.option_bits & (1<<('Z'-'X'))){
        sgd.debugXA = 1; /* XA debug is on. */
    } else {
        sgd.debugXA = 0; /* XA debug is not on. */
    }

    /* End of code to get option bits, configuration file name, and set debugXA if needed */

    /* Change to IDLC-generated source.
       Temporarily added printf. */
    if (sgd.debugXA) {
        printf("Entering XA Server main() in server$corba.c \n");
    }

    /* Change to IDLC-generated source.
       Keytape bypass code added. */
    env.exception = "_JCA";

    orb = CORBA_ORB_init(NULL, NULL, id, &env);

    if (env._major != 0) {
        userlog("Error status from CORBA_ORB_init():  %d\n", env._major);
        return(-1);
    }

    /* Change to IDLC-generated source.
       Temporarily added printf. */
    if (sgd.debugXA){
        printf("Returned from CORBA_ORB_init() in main() in server$corba.c \n");
    }

    poa_ref = CORBA_ORB_resolve_initial_references(orb, "RootPOA", &env);

    if (env._major != 0) {
        userlog("Error status from CORBA_ORB_resolve_initial_references():  %d\n", env._major);
       return(-1);
    }

    /* Change to IDLC-generated source.
       Temporarily added printf. */
    if (sgd.debugXA) {
        printf("Returned from resolve_initial_references in main() in server$corba.c \n");
    }

    /* Change to IDLC-generated source.
       Use a switch to chose the correct com_unisys_os2200_rdms_jdbc_XAJDBCServernn_servant (where nn is the xaServerNumber) to activate. */
    switch (sgd.xaServerNumber) {
        case 1: {
            PortableServer_POA_activate_object(poa_ref, (PortableServer_Servant)&com_unisys_os2200_rdms_jdbc_XAJDBCServer1_servant, &env);

            if(env._major != 0) {
                userlog("Error in PortableServer_POA_activate_object():  XAServer number %d\n, env._major = %d", sgd.xaServerNumber, env._major);
                return(-1);
            }
            break;
        }

        case 2: {
            PortableServer_POA_activate_object(poa_ref, (PortableServer_Servant)&com_unisys_os2200_rdms_jdbc_XAJDBCServer2_servant, &env);

            if(env._major != 0) {
                userlog("Error in PortableServer_POA_activate_object():  XAServer number %d\n, env._major = %d", sgd.xaServerNumber, env._major);
                return(-1);
            }
            break;
        }

        case 3: {
            PortableServer_POA_activate_object(poa_ref, (PortableServer_Servant)&com_unisys_os2200_rdms_jdbc_XAJDBCServer3_servant, &env);

            if(env._major != 0) {
                userlog("Error in PortableServer_POA_activate_object():  XAServer number %d\n, env._major = %d", sgd.xaServerNumber, env._major);
                return(-1);
            }
            break;
        }

        case 4: {
            PortableServer_POA_activate_object(poa_ref, (PortableServer_Servant)&com_unisys_os2200_rdms_jdbc_XAJDBCServer4_servant, &env);

            if(env._major != 0) {
                userlog("Error in PortableServer_POA_activate_object():  XAServer number %d\n, env._major = %d", sgd.xaServerNumber, env._major);
                return(-1);
            }
            break;
        }

        case 5: {
            PortableServer_POA_activate_object(poa_ref, (PortableServer_Servant)&com_unisys_os2200_rdms_jdbc_XAJDBCServer5_servant, &env);

            if(env._major != 0) {
                userlog("Error in PortableServer_POA_activate_object():  XAServer number %d\n, env._major = %d", sgd.xaServerNumber, env._major);
                return(-1);
            }
            break;
        }

        case 6: {
            PortableServer_POA_activate_object(poa_ref, (PortableServer_Servant)&com_unisys_os2200_rdms_jdbc_XAJDBCServer6_servant, &env);

            if(env._major != 0) {
                userlog("Error in PortableServer_POA_activate_object():  XAServer number %d\n, env._major = %d", sgd.xaServerNumber, env._major);
                return(-1);
            }
            break;
        }

        case 7: {
            PortableServer_POA_activate_object(poa_ref, (PortableServer_Servant)&com_unisys_os2200_rdms_jdbc_XAJDBCServer7_servant, &env);

            if(env._major != 0) {
                userlog("Error in PortableServer_POA_activate_object():  XAServer number %d\n, env._major = %d", sgd.xaServerNumber, env._major);
                return(-1);
            }
            break;
        }

        case 8: {
            PortableServer_POA_activate_object(poa_ref, (PortableServer_Servant)&com_unisys_os2200_rdms_jdbc_XAJDBCServer8_servant, &env);

            if(env._major != 0) {
                userlog("Error in PortableServer_POA_activate_object():  XAServer number %d\n, env._major = %d", sgd.xaServerNumber, env._major);
                return(-1);
            }
            break;
        }

        case 9: {
            PortableServer_POA_activate_object(poa_ref, (PortableServer_Servant)&com_unisys_os2200_rdms_jdbc_XAJDBCServer9_servant, &env);

            if(env._major != 0) {
                userlog("Error in PortableServer_POA_activate_object():  XAServer number %d\n, env._major = %d", sgd.xaServerNumber, env._major);
                return(-1);
            }
            break;
        }

        case 10: {
            PortableServer_POA_activate_object(poa_ref, (PortableServer_Servant)&com_unisys_os2200_rdms_jdbc_XAJDBCServer10_servant, &env);

            if(env._major != 0) {
                userlog("Error in PortableServer_POA_activate_object():  XAServer number %d\n, env._major = %d", sgd.xaServerNumber, env._major);
                return(-1);
            }
            break;
        }

        case 11: {
            PortableServer_POA_activate_object(poa_ref, (PortableServer_Servant)&com_unisys_os2200_rdms_jdbc_XAJDBCServer11_servant, &env);

            if(env._major != 0) {
                userlog("Error in PortableServer_POA_activate_object():  XAServer number %d\n, env._major = %d", sgd.xaServerNumber, env._major);
                return(-1);
            }
            break;
        }

        case 12: {
            PortableServer_POA_activate_object(poa_ref, (PortableServer_Servant)&com_unisys_os2200_rdms_jdbc_XAJDBCServer12_servant, &env);

            if(env._major != 0) {
                userlog("Error in PortableServer_POA_activate_object():  XAServer number %d\n, env._major = %d", sgd.xaServerNumber, env._major);
                return(-1);
            }
            break;
        }

        case 13: {
            PortableServer_POA_activate_object(poa_ref, (PortableServer_Servant)&com_unisys_os2200_rdms_jdbc_XAJDBCServer13_servant, &env);

            if(env._major != 0) {
                userlog("Error in PortableServer_POA_activate_object():  XAServer number %d\n, env._major = %d", sgd.xaServerNumber, env._major);
                return(-1);
            }
            break;
        }

        case 14: {
            PortableServer_POA_activate_object(poa_ref, (PortableServer_Servant)&com_unisys_os2200_rdms_jdbc_XAJDBCServer14_servant, &env);

            if(env._major != 0) {
                userlog("Error in PortableServer_POA_activate_object():  XAServer number %d\n, env._major = %d", sgd.xaServerNumber, env._major);
                return(-1);
            }
            break;
        }

        case 15: {
            PortableServer_POA_activate_object(poa_ref, (PortableServer_Servant)&com_unisys_os2200_rdms_jdbc_XAJDBCServer15_servant, &env);

            if(env._major != 0) {
                userlog("Error in PortableServer_POA_activate_object():  XAServer number %d\n, env._major = %d", sgd.xaServerNumber, env._major);
                return(-1);
            }
            break;
        }

        case 16: {
            PortableServer_POA_activate_object(poa_ref, (PortableServer_Servant)&com_unisys_os2200_rdms_jdbc_XAJDBCServer16_servant, &env);

            if(env._major != 0) {
                userlog("Error in PortableServer_POA_activate_object():  XAServer number %d\n, env._major = %d", sgd.xaServerNumber, env._major);
                return(-1);
            }
            break;
        }

        default: {
            userlog("Error attempting PortableServer_POA_activate_object(), bad XAServer number: %d\n", sgd.xaServerNumber);
            return(-1);
        }
    }

    /* Change to IDLC-generated source.
       Temporarily added printf. */
    if (sgd.debugXA) {
        printf("Returned from activate_object calls in main() in server$corba.c \n");
        printf("Calling CORBA_ORB_run in main() in server$corba.c \n");
    }

    CORBA_ORB_run(orb, &env);

    if (env._major != 0) {
        userlog("Error status from CORBA_ORB_run():  %d\n", env._major);
        return(-1);
    }

    /* Change to IDLC-generated source.
       Temporarily added printf. */
    if (sgd.debugXA) {
        printf("Exiting XA Server main() in server$corba.c \n");
    }

    return 0;
}
