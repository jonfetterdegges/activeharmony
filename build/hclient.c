/*
 * Copyright 2003-2011 Jeffrey K. Hollingsworth
 *
 * This file is part of Active Harmony.
 *
 * Active Harmony is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Active Harmony is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Active Harmony.  If not, see <http://www.gnu.org/licenses/>.
 */

/******************************
 *
 * Author: Cristian Tapus
 * History:
 *   Dec 22, 2000 - comments and updates added by Cristian Tapus
 *   Nov 20, 2000 - Dejan added some features to the existing functions
 *   Sept 22, 2000 - comments added and debug moded added
 *   July 9, 2000 - first version
 *   July 15, 2000 - comments and update added by Cristian Tapus
 *   2010 : fix for multiple server connection by George Teodoro
 *   2004-2010 : various fixes and additions by Ananta Tiwari
 *******************************/


/***
 * include other user written headers
 ***/
#include "hclient.h"
#include "hmesgs.h"
#include "hsockutil.h"

#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <assert.h>

/* For mmap() support */
#include <sys/mman.h>
#include <sys/stat.h>

/* For polling support */
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>

enum harmony_state_t {
    HARMONY_STATE_UNKNOWN,
    HARMONY_STATE_INIT,
    HARMONY_STATE_CONNECTED,
    HARMONY_STATE_CONFIGURED,
    HARMONY_STATE_READY,
    HARMONY_STATE_TESTING,
    HARMONY_STATE_CONVERGED,

    HARMONY_STATE_MAX
};

enum bundle_flag_t {
    BUNDLE_FLAG_ALLOC  = 0x01,
    BUNDLE_FLAG_RANGED = 0x02
};

typedef struct {
    int min;
    int max;
    int step;
} int_range_t;

typedef struct {
    double min;
    double max;
    double step;
} real_range_t;

typedef struct {
    char **set;
    unsigned long set_len;
    unsigned long set_capacity;
} enum_range_t;

typedef union {
    int_range_t  i;
    real_range_t r;
    enum_range_t e;
} range_u;

typedef struct {
    bundle_scope_t scope;
    int flags;
    VarDef val;
    range_u range;
} bundle_t;

struct hdesc_t {
    char *name;
    harmony_state_t state;
    harmony_iomethod_t iomethod;

    int socket;
    int client_id;
    int codeserver;    /* Flag used to determine if code server is in use.
                          To be removed upon overhaul of server. */
    int timestamp;     /* Used by the simplex minimization method to decide
                          if the performance reflects the changes of the
                          parameters */
    int code_timestep; /* Used to track the number of requests for new code
                          the client has made. */

    bundle_t *bundle;  /* this list of VarDef keeps track of all the variables
                          registered by the application to the server. */
    unsigned long bundle_len;
    unsigned long bundle_capacity;
    char *best_set;

    char **constraint;
    unsigned long constraint_len;
    unsigned long constraint_capacity;
};

/* -------------------------------------------------------------------
 * Private asynchronous (signal based) I/O functions
 */
static int debug_mode = 0;
static int huse_signals;
static int *async_fd;
static int  async_fds_len = 0;

/* Space to preserve original sigio handler. */
void (*app_sigio_handler)(int);

/* -------------------------------------------------------------------
 * Private asynchronous (signal based) I/O functions
 */
static void hsigio_handler(int s)
{
#if 0
    /***
     * This functionality is currently broken.  Fix this later.
     ***/

    /* the purpose of this set of file descriptors is to see if the SIGIO
     * was generated by information received on the harmony communication
     * socket
     */
    fd_set modif_sock;

    if (debug_mode)
        printf("***** Got SIGIO %d !\n", s);

    while (1) {
      again:
        
        /* initialize the set of modified file descriptor to check for
         * harmony socket only
         */
        FD_ZERO(&modif_sock);
        FD_SET(hclient_socket[currentIndex], &modif_sock);

        /* set tv for polling */
        struct timeval tv =  {0, 0};
        int res;

        if ((res = select(hclient_socket[currentIndex]+1,&modif_sock, NULL, NULL, &tv)) < 0) {
            if (errno = EINTR)
                goto again;
            perror("Error in hsigio_handler() during select()");
        }

        if (!res)
            break;

        /* if the harmony communication socket is one of the modified
         * sockets, then we have a packet from the server
         */
        if (FD_ISSET(hclient_socket[currentIndex],&modif_sock)) {
            /* check the message from the server
             *
             * we can read data from the socket. define for this a message
             * we know that from the server we only get update messages
             */
            HMessage *mesg = receive_message(hclient_socket[currentIndex]);
            HUpdateMessage *m = dynamic_cast<HUpdateMessage *>(mesg);
            assert(m && "Server returned wrong message type!");

            /* print the content of the message if in debug mode */
            if (debug_mode)
                m->print();

            process_update(m);
            delete m;
        }
        else
            app_sigio_handler(s);
    }
#endif
}

int hsigio_blocked = 0;
/*********
 *        block_sigio(), unblock_sigio()
 * Description: (un)blocks sigio signals
 * Used from:   routines that send messages asynchronously to the server
 *
 ***********/
static void block_sigio()
{
#if defined(SOLARIS) || defined(__linux)
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGIO);
    sigaddset(&set, SIGALRM);
    sigprocmask(SIG_BLOCK, &set, NULL);
#else
    sigblock(sigmask(SIGIO));
#endif  /*SOLARIS*/
    hsigio_blocked = 1;
}

static void unblock_sigio()
{
#ifdef DEBUG_SIGIO
    if (!hsigio_blocked) {
        fprintf(stderr, "WARNING: Improper state or use of unblock_sigio\n");
    }
#endif  /*DEBUG_SIGIO*/
    hsigio_blocked = 0;
#ifdef SOLARIS
    sigset_t    set;
    sigemptyset(&set);
    sigprocmask(SIG_SETMASK, &set, NULL);
#else
    sigsetmask(0);
#endif  /*SOLARIS*/
}

static int enable_async_io(hdesc_t *hdesc)
{
    /* install the SIGIO signal handler
       and set which file descriptor to produce the SIGIO */
#undef DONT_SAVE_OLD_HANDLER
#ifdef DONT_SAVE_OLD_HANDLER
    signal(SIGIO, hsigio_handler);
#else
    struct sigaction ha_act, app_act;

    ha_act.sa_handler = hsigio_handler;
    sigemptyset(&ha_act.sa_mask);
    sigaddset(&ha_act.sa_mask, SIGALRM);
#if defined(__sun) && ! defined(__SVR4)
    ha_act.sa_flags = 0;
#else
    ha_act.sa_flags = SA_RESTART;
#endif
    if (sigaction(SIGIO, &ha_act, &app_act)){
        perror("sigaction() error during harmony_connect()");
        return -1;
    }

    /* save app handler to call it if SIGIO is not for Harmony */
    int fileflags;
    app_sigio_handler = app_act.sa_handler;
#endif
    if (fcntl(hdesc->socket, F_SETOWN, getpid()) < 0) {
        perror("fcntl(F_SETOWN) error during harmony_connect()");
        return -1;
    }

    if (fileflags = fcntl(hdesc->socket, F_GETFL) == -1) {
        perror("fcntl(F_GETFL) error during harmony_connect()");
        return -1;
    }

#ifdef LINUX
    /* this does not work on SOLARIS */
    if (fcntl(hdesc->socket, F_SETFL, fileflags | FASYNC) == -1) {
        perror("fcntl(F_SETFL) error during harmony_connect()");
        return -1;
    }
#endif

    return 0;
}

/* -------------------------------------------------------------------
 * Private internal helper functions
 */
static int grow_array(void *old_buf,
                      unsigned long *old_capacity,
                      unsigned long elem_size)
{
    void *new_buf;
    unsigned long new_capacity = 8;

    if (*old_capacity > 0) {
        new_capacity = *old_capacity << 1;
    }

    new_buf = realloc(*(void **)old_buf, new_capacity * elem_size);
    if (new_buf == NULL) {
        return -1;
    }

    *(void **)old_buf = new_buf;
    *old_capacity = new_capacity;
    return 0;
}

static int tcp_connect(hdesc_t *hdesc, const char *host, int port)
{
    struct sockaddr_in address;
    struct hostent *hostaddr;

    if (hdesc->state != HARMONY_STATE_INIT) {
        fprintf(stderr, "Invalid hdesc_t during harmony_connect()\n");
        return -1;
    }

    hdesc->socket = socket(AF_INET, SOCK_STREAM, 0);
    if (hdesc->socket < 0) {
        perror("socket() error during harmony_connect()");
        return -1;
    }

    /* connect to server */
    address.sin_family = AF_INET;

    /* look for the HARMONY_S_PORT environment variable
       to set the port of connection to the machine where the harmony
       server is running */
    if (port == 0) {
        if (getenv("HARMONY_S_PORT") == NULL) {
            fprintf(stderr, "HARMONY_S_PORT variable not set!\n");
            return -1;
        }
        port = atoi(getenv("HARMONY_S_PORT"));
    }
    address.sin_port = htons(port);

    /* look for the HARMONY_S_HOST environment variable
       to set the host where the harmony server resides */
    if (host == NULL) {
        host = getenv("HARMONY_S_HOST");
        if (host == NULL) {
            fprintf(stderr, "HARMONY_S_HOST variable not set!\n");
            return -1;
        }
    }

    hostaddr = gethostbyname(host);
    if (hostaddr == NULL) {
        fprintf(stderr, "Could not resolve '%s'.\n", host);
        return -1;
    }

    /* set up the host address we will connect to */
    memcpy(&address.sin_addr,
           hostaddr->h_addr_list[0],
           sizeof(struct in_addr));

    /* try to connect to the server */
    int ret = connect(hdesc->socket,
                      (struct sockaddr *)&address,
                      sizeof(address));
    if (ret < 0) {
        perror("connect() error during harmony_connect()");
        return -1;
    }
    hdesc->state = HARMONY_STATE_CONNECTED;

    return 0;
}

static int bundle_add_check(hdesc_t *hdesc, const char *name, void *ptr)
{
    int i;

    for (i = 0; i < hdesc->bundle_len; ++i) {
        if (strcmp(name, hdesc->bundle[i].val.getName()) == 0) {
            fprintf(stderr, "Variable %s already exists.\n", name);
            return -1;
        }
        if (ptr != NULL && ptr == hdesc->bundle[i].val.getPointer()) {
            fprintf(stderr, "Memory for variable %s (0x%p) already"
                    "registered by variable %s.\n", name, ptr,
                    hdesc->bundle[i].val.getName());
            return -1;
        }
    }

    if (hdesc->bundle_len == hdesc->bundle_capacity) {
        if (grow_array(&hdesc->bundle,
                       &hdesc->bundle_capacity,
                       sizeof(bundle_t)) < 0) {
            fprintf(stderr, "Memory allocation error.\n");
            return -1;
        }

        /* Make sure constructor is called on newly allocated memory. */
        for (i = hdesc->bundle_len; i < hdesc->bundle_capacity; ++i) {
            memset(&hdesc->bundle[i].val, 0, sizeof(VarDef));
            hdesc->bundle[i].val = VarDef();
        }
    }

    return 0;
}

static bundle_t *bundle_find(hdesc_t *hdesc, const char *name)
{
    int i;
    for (i = 0; i < hdesc->bundle_len; ++i) {
        if (strcmp(name, hdesc->bundle[i].val.getName()) == 0) {
            return &hdesc->bundle[i];
        }
    }
    return NULL;
}

/* -------------------------------------------------------------------
 * Old client API compatibility functions.
 *
 * These functions map new client API functionality to the old API
 * so that little to no changes are necessary to the harmony server
 * or client/server protocol.
 *
 * They should be removed as the server and client/server protocol
 * get overhauled.
 */

/*
 * get the value of any tcl backend variable - for ex. next_iteration
 *  this handles int variables. NOT EXPOSED.
 */
static char *request_tcl_variable(hdesc_t *hdesc, const char *variable)
{
    VarDef var(variable, VAR_STR);
    HUpdateMessage m(HMESG_TCLVAR_REQ, 0);

    /* set a dummy value */
    var.setValue("tcl_var");
    m.set_timestamp(hdesc->timestamp);
    m.set_var(var);

    if (debug_mode)
        printf("tcl_request: %d %s \n", hdesc, variable);

    send_message(&m, hdesc->socket);

    /* wait for answer */
    HMessage *mesg = receive_message(hdesc->socket);
    if (mesg->get_type() == HMESG_FAIL) {
        delete mesg;
        /* this needs to be checked thoroughly. */
        fprintf(stderr, "Error during Tcl variable retrieval.\n");
        return NULL;
    }

    HUpdateMessage *um = dynamic_cast<HUpdateMessage *>(mesg);
    assert(um && "Server returned wrong message type!");

    char *str = (char *)um->get_var(0)->getPointer();
    char *retval = (char *)malloc( strlen(str) + 1 );
    strcpy(retval, str);
    delete um;

    return retval;
}

/*
 * Create a harmony application file from the registered bundles.
 * This should be removed in favor of a "signature" connection message.
 * Such a signature message might contain the following info:
 *
 *	[appName, bundles, constraints]
 */
static char *simulate_application_file(hdesc_t *hdesc)
{
    int i, j, seen_global = 0;
    char *buf;
    const char *chunk_str;
    unsigned long capacity = 4096;
    int len, chunk, subchunk;
    VarDef *val;

    buf = (char *) malloc(capacity);
    if (buf == NULL) {
        return NULL;
    }

  top:
    len = snprintf(buf, capacity, "harmonyApp %s {\n", hdesc->name);
    if (len >= capacity) {
        if (grow_array(&buf, &capacity, sizeof(char)) < 0) {
            return NULL;
        }
        goto top;
    }

    for (i = 0; i < hdesc->bundle_len; ++i) {
        val = &hdesc->bundle[i].val;

        switch (val->getType()) {
        case VAR_INT:
            chunk = snprintf(buf + len, capacity - len,
                             "{ harmonyBundle %s { int { %d %d %d",
                             val->getName(),
                             hdesc->bundle[i].range.i.min,
                             hdesc->bundle[i].range.i.max,
                             hdesc->bundle[i].range.i.step);
            break;
        case VAR_STR:
            chunk = snprintf(buf + len, capacity - len,
                             "{ harmonyBundle %s { enum {",
                             val->getName());
            for (j = 0; j < hdesc->bundle[i].range.e.set_len; ++j) {
                if (len + chunk >= capacity) {
                    break;
                }
                chunk += snprintf(buf + len + chunk, capacity - len - chunk,
                                  " \"%s\"", hdesc->bundle[i].range.e.set[j]);
            }
            break;
        case VAR_REAL:
            chunk = snprintf(buf + len, capacity - len,
                             "{ harmonyBundle %s { real { "
                             "%.17lg %.17lg %.17lg",
                             val->getName(),
                             hdesc->bundle[i].range.r.min,
                             hdesc->bundle[i].range.r.max,
                             hdesc->bundle[i].range.r.step);
            break;
        }

        if (capacity > len + chunk) {
            if (hdesc->bundle[i].scope == BUNDLE_SCOPE_GLOBAL) {
                chunk_str = "} global } }\n";
                seen_global = 1;
            }
            else {
                chunk_str = "} } }\n";
            }
            chunk += snprintf(buf + len + chunk, capacity - len - chunk,
                              chunk_str);
        }

        if (capacity <= len + chunk) {
            if (grow_array(&buf, &capacity, sizeof(char)) < 0) {
                return NULL;
            }
            buf[len] = '\0';
            --i;
            continue;
        }

        len += chunk;
    }

    if (seen_global == 1) {
        chunk_str = " { obsGoodness 0 0 global } { predGoodness 0 0 } }";
    }
    else {
        chunk_str = " { obsGoodness 0 0 } { predGoodness 0 0 } }";
    }

    while (capacity <= len + strlen(chunk_str)) {
        if (grow_array(&buf, &capacity, sizeof(char)) < 0) {
            return NULL;
        }
    }
    strcat(buf, chunk_str);

    return buf;
}

int application_setup(hdesc_t *hdesc, const char *description, unsigned len)
{
    if (len == 0) {
        len = strlen(description);
    }

    HDescrMessage mesg(HMESG_APP_DESCR, description, len);
    send_message(&mesg, hdesc->socket);

    /* wait for confirmation */
    HMessage *m = receive_message(hdesc->socket);
    if (m->get_type() != HMESG_CONFIRM) {
        /* something went wrong on the server side! */
        delete m;
        fprintf(stderr, "Server reports error in application description.\n");
        return -1;
    }

    delete m;
    return 0;
}

/*
 * Register all harmony bundles with server.  This should be removed
 * in favor of a "signature" connection message.  Such a signature
 * message might contain the following info:
 *
 *	[appName, bundles, constraints]
 */
static int register_bundles(hdesc_t *hdesc)
{
    int i = 0;

    for (i =0; i < hdesc->bundle_len; ++i) {
        bundle_t *bun = hdesc->bundle + i;

        /* send variable registration message */
        HDescrMessage m(HMESG_VAR_DESCR,
                        bun->val.getName(),
                        strlen(bun->val.getName()));
        send_message(&m, hdesc->socket);

        /* wait for confirmation
           the confirmation message also contains the initial
           value of the variable */
        HMessage *mesg = receive_message(hdesc->socket);

        if (debug_mode)
            mesg->print();

        if (mesg->get_type() == HMESG_FAIL) {
            /* failed to register variable with server */
            fprintf(stderr, "Server reports error during registration of "
                            "variable %s from application %s\n",
                    bun->val.getName(), hdesc->name);
            delete mesg;
            /*unblock_sigio();*/
            return NULL;
        }

        HUpdateMessage *umesg = dynamic_cast<HUpdateMessage *>(mesg);
        assert(umesg && "Server returned wrong message type!");

        delete umesg;
    }
}

int code_generation_complete(hdesc_t *hdesc)
{
    /* first update the timestamp */
    VarDef var("code_completion", VAR_INT);
    var.setValue(hdesc->code_timestep);

    /* define a message and send it */
    HUpdateMessage m(HMESG_CODE_COMPLETION, 0);
    m.set_timestamp(hdesc->code_timestep);
    m.set_var(var);
    send_message(&m, hdesc->socket);

    ++hdesc->code_timestep;

    /* wait for answer */
    HMessage *mesg = receive_message(hdesc->socket);
    if (mesg->get_type() == HMESG_FAIL) {
        /* the server could not return the value of the variable */
        fprintf(stderr, "Server reports error during code completion check.\n");
        delete mesg;
        return -1;
    }

    HUpdateMessage *umesg = dynamic_cast<HUpdateMessage *>(mesg);
    assert(umesg && "Server returned wrong message type!");

    VarDef *v = umesg->get_var(0);
    int result =  *(int *)umesg->get_var(0)->getPointer();
    delete umesg;
    return result;
}

int set_to_best(hdesc_t *hdesc)
{
    int i = 0;
    int change_flag = 0;
    char *best_str, *curr, *end;

    best_str = harmony_best_config(hdesc);
    if (best_str == NULL || best_str[0] == '\0') {
        if (best_str != NULL) {
            free(best_str);
        }
        return 0;
    }

    curr = end = best_str;
    while (*end != '\0') {
        if (i == hdesc->bundle_len) {
            ++i;
            break;
        }
        VarDef *val = &hdesc->bundle[i++].val;
        end = strchr(curr, '_');
        if (end == NULL) {
            end = curr + strlen(curr);
        }

        switch (val->getType()) {
        case VAR_INT:
            if (*(int*)val->getPointer() != atoi(curr)) {
                val->setValue(atoi(curr));
                change_flag = 1;
            }
            break;
        case VAR_STR:
            if (end != '\0') {
                *end = '\0';
                if (strcmp(curr, (char *)val->getPointer()) != 0) {
                    val->setValue(curr);
                    change_flag = 1;
                }
                *end = '_';
            }
            else {
                if (strcmp(curr, (char *)val->getPointer()) != 0) {
                    val->setValue(curr);
                    change_flag = 1;
                }
            }
            break;
        case VAR_REAL:
            if (*(double*)val->getPointer() != atof(curr)) {
                val->setValue(atof(curr));
                change_flag = 1;
            }
            break;
        }
        curr = end + 1;
    }

    if (i != hdesc->bundle_len) {
        printf("*** Warning: Invalid best string received from server.\n");
    }

    free(best_str);
    return change_flag;
}

/* -------------------------------------------------------------------
 * Public Client API Implementations
 */

hdesc_t *harmony_init(const char *name, harmony_iomethod_t iomethod)
{
    hdesc_t *retval = (hdesc_t *) malloc(sizeof(hdesc_t));
    if (!retval) {
        perror("malloc() error during harmony_init()");
        return NULL;
    }
    memset(retval, sizeof(hdesc_t), 0);

    retval->name = (char *) malloc(strlen(name) + 1);
    if (!retval->name) {
        perror("malloc() error during harmony_init()");
        return NULL;
    }
    strcpy(retval->name, name);

    retval->iomethod = iomethod;
    retval->state = HARMONY_STATE_INIT;
    retval->timestamp = -1;
    retval->code_timestep = 1;

    return retval;
}

int harmony_connect(hdesc_t *hdesc, const char *host, int port)
{
    int i;
    HRegistMessage *m;
    HMessage *mesg;
    char *app_desc, *codegen;

    /* A couple sanity checks before we connect to the server. */
    if (hdesc->bundle_len == 0) {
        fprintf(stderr, "No registered bundles to harmonize.\n");
        goto cleanup;
    }

    for (i = 0; i < hdesc->bundle_len; ++i) {
        if (hdesc->bundle[i].flags & BUNDLE_FLAG_RANGED == 0) {
            fprintf(stderr, "Range uninitialized for variable %s\n",
                    hdesc->bundle[i].val.getName());
            goto cleanup;
        }
    }

    if (tcp_connect(hdesc, host, port) < 0) {
        goto cleanup;
    }

    /* send the client registration message */
    m = new HRegistMessage(HMESG_CLIENT_REG, 0);
    send_message(m, hdesc->socket);
    delete m;

    /* wait for confirmation */
    mesg = receive_message(hdesc->socket);
    m = dynamic_cast<HRegistMessage *>(mesg);
    assert(m && "Server returned wrong message type!");

    hdesc->client_id = m->get_id();
    assert(hdesc->client_id > 0);
    delete m;

    /* send an application registration message */
    app_desc = simulate_application_file(hdesc);
    if (app_desc == NULL || application_setup(hdesc, app_desc, 0) < 0) {
        fprintf(stderr, "Error registering application description.\n");
        goto cleanup;
    }
    free(app_desc);

    /* register all known bundles */
    if (register_bundles(hdesc) < 0) {
        fprintf(stderr, "Error registering bundle description.\n");
        goto cleanup;
    }

    if (hdesc->iomethod == HARMONY_IO_ASYNC) {
        if (enable_async_io(hdesc) < 0) {
            fprintf(stderr, "Error enabling asynchronous I/O.\n");
            goto cleanup;
        }
    }

    /* Hopefully, this can be removed as parts of harmony get overhauled. */
    codegen = harmony_query(hdesc, "codegen");
    if (codegen == NULL) {
        fprintf(stderr, "Error requesting codegen status.\n");
        goto cleanup;
    }

    i = 0;
    while (codegen[i] != '\0') {
        codegen[i] = tolower(codegen[i]);
        ++i;
    }

    if (strncmp(codegen, "none", sizeof(codegen)) != 0) {
        hdesc->codeserver = 1;
    }

    free(codegen);
    return hdesc->client_id;

  cleanup:
    if (hdesc->state == HARMONY_STATE_CONNECTED) {
        if (close(hdesc->socket) < 0) {
            perror("Ignoring close() error during"
                   " harmony_connect() cleanup");
        }
        hdesc->state = HARMONY_STATE_INIT;
    }
    return -1;
}

int harmony_reconnect(hdesc_t *hdesc, const char *host, int port, int cid)
{
    int i;
    HRegistMessage *m;
    HMessage *mesg;
    char *app_desc, *codegen;

    if (hdesc->state == HARMONY_STATE_INIT) {
        if (tcp_connect(hdesc, host, port) < 0) {
            goto cleanup;
        }

        /* send an application registration message */
        app_desc = simulate_application_file(hdesc);
        if (app_desc == NULL || application_setup(hdesc, app_desc, 0) < 0) {
            fprintf(stderr, "Error registering application description.\n");
            goto cleanup;
        }
        free(app_desc);

        /* register all known bundles */
        if (register_bundles(hdesc) < 0) {
            fprintf(stderr, "Error registering bundle description.\n");
            goto cleanup;
        }
    }

    /* send the registration message */
    m = new HRegistMessage(HMESG_CLIENT_REG, 0);
    m->set_id(cid);
    send_message(m, hdesc->socket);
    delete m;

    /* wait for confirmation */
    mesg = receive_message(hdesc->socket);
    m = dynamic_cast<HRegistMessage *>(mesg);
    assert(m && "Server returned wrong message type!");

    hdesc->client_id = m->get_id();
    assert(hdesc->client_id > 0);
    delete m;

    if (hdesc->iomethod == HARMONY_IO_ASYNC) {
        if (enable_async_io(hdesc) < 0) {
            fprintf(stderr, "Error enabling asynchronous I/O.\n");
            goto cleanup;
        }
    }

    /* Hopefully, this can be removed as parts of harmony get overhauled. */
    codegen = harmony_query(hdesc, "codegen");
    if (codegen == NULL) {
        fprintf(stderr, "Error requesting codegen status.\n");
        goto cleanup;
    }

    i = 0;
    while (codegen[i] != '\0') {
        codegen[i] = tolower(codegen[i]);
        ++i;
    }

    if (strncmp(codegen, "none", sizeof(codegen)) != 0) {
        hdesc->codeserver = 1;
    }

    free(codegen);
    return hdesc->client_id;

  cleanup:
    if (hdesc->state == HARMONY_STATE_CONNECTED) {
        if (close(hdesc->socket) < 0) {
            perror("Ignoring close() error during"
                   " harmony_reconnect() cleanup");
        }
        hdesc->state = HARMONY_STATE_INIT;
    }
    return -1;
}

int harmony_disconnect(hdesc_t *hdesc)
{
    /* send the unregister message to the server */
    HRegistMessage m(HMESG_CLIENT_UNREG, 0);
    send_message(&m, hdesc->socket);

    /* wait for server confirmation */
    HMessage *mesg = receive_message(hdesc->socket);
    if (mesg->get_type() != HMESG_CONFIRM) {
        delete mesg;
        fprintf(stderr, "Server reports error during unregister.\n");
        return -1;
    }
    delete mesg;

    /* close the actual connection */
    hdesc->state = HARMONY_STATE_INIT;
    if (close(hdesc->socket) < 0) {
        perror("Ignoring close() error during harmony_disconnect()");
    }

    /* We should re-install the original sigio handler here, right? */
    return 0;
}

char *harmony_query(hdesc_t *hdesc, const char *key)
{
    char *value;

    if (hdesc->state < HARMONY_STATE_CONNECTED) {
        fprintf(stderr, "Cannot query until connected.\n");
        return NULL;
    }

    HDescrMessage mesg(HMESG_CFG_REQ, key, strlen(key));
    send_message(&mesg, hdesc->socket);

    /* Wait for reply. */
    HMessage *reply = receive_message(hdesc->socket);
    if (reply->get_type() != HMESG_CFG_REQ) {
        fprintf(stderr, "Server reports error during config lookup.\n");
        delete reply;
        return NULL;
    }

    HDescrMessage *msg = dynamic_cast<HDescrMessage *>(reply);
    assert(msg && "Server returned wrong message type!");

    value = (char *)malloc(strlen(msg->get_descr()) + 1);
    if (value == NULL) {
        fprintf(stderr, "Memory allocation error.\n");
        return NULL;
    }
    strcpy(value, msg->get_descr());
    delete reply;

    /* User is responsible for freeing this memory. */
    return value;
}

int harmony_bundle_file(hdesc_t *hdesc, const char *filename)
{
    int fd, retval = -1;
    char *buf;
    unsigned buflen;
    struct stat sb;

    fd = open(filename, O_RDONLY);
    if (fd == -1) {
        printf("[AH]: Error opening file %s: %s\n", filename, strerror(errno));
        goto cleanup;
    }

    /* Obtain file size */
    if (fstat(fd, &sb) == -1) {
        printf("[AH]: Error during fstat(): %s\n", strerror(errno));
        goto cleanup_open;
    }
    buflen = sb.st_size;

    buf = (char *)mmap(NULL, buflen, PROT_READ, MAP_PRIVATE, fd, 0);
    if (buf == (char *)-1) {
        printf("[AH]: Error during mmap(): %s\n", strerror(errno));
        goto cleanup_open;
    }

    retval = application_setup(hdesc, buf, buflen);

    if (munmap(buf, buflen) != 0) {
        printf("[AH]: Error during munmap(): %s. Ignoring.\n",
               strerror(errno));
    }

  cleanup_open:
    if (close(fd) != 0) {
        printf("[AH]: Error during close(): %s. Ignoring.\n",
               strerror(errno));
    }

  cleanup:
    return retval;
}

int harmony_fetch(hdesc_t *hdesc)
{
    int i;

    if (hdesc->state < HARMONY_STATE_CONNECTED) {
        fprintf(stderr, "Cannot fetch until connected.\n");
        return -1;
    }

    /* here we also have to differentiate between use of signals
       and no signals */
    if (hdesc->iomethod == HARMONY_IO_ASYNC) {
        /* we use signals, so we just have to copy the shadow
           to the actual value */
        for (i = 0; i < hdesc->bundle_len; ++i) {
            void *value = hdesc->bundle[i].val.getShadow();
            switch (hdesc->bundle[i].val.getType()) {
            case VAR_INT:
                hdesc->bundle[i].val.setValue(*(int *)value);
                break;
            case VAR_STR:
                hdesc->bundle[i].val.setValue((char *)value);
                break;
            case VAR_REAL:
                hdesc->bundle[i].val.setValue(*(double *)value);
                break;
            }
        }
        return 1;
    }

    /* Ask server if new values are ready.  They only reason they wouldn't
     * be is if the code server is in use.  Check this first.
     */
    if (hdesc->codeserver == 1 && code_generation_complete(hdesc) != 1) {
        /* New results aren't ready yet.  Use the best results thus far. */
        return set_to_best(hdesc);
    }

    /* if signals are not used, send request message */
    HUpdateMessage *m = new HUpdateMessage(HMESG_VAR_REQ, 0);

    for (i = 0; i < hdesc->bundle_len; ++i)
        m->set_var(hdesc->bundle[i].val);

    if (debug_mode)
        m->print();

    send_message(m, hdesc->socket);
    delete m;

    /* wait for the answer from the harmony server */
    HMessage *mesg = receive_message(hdesc->socket);
    if (mesg->get_type() == HMESG_FAIL) {
        /* the server could not return the value of the variable */
        delete mesg;
        fprintf(stderr, "Server reports error during variable request.\n");
	return -1;
    }

    m = dynamic_cast<HUpdateMessage *>(mesg);
    assert(m && "Server returned wrong message type!");

    /* Updating the timestamp */
    hdesc->timestamp = m->get_timestamp();

    if (debug_mode) {
        m->print();
    }

    /* Updating the variables from the content of the message */
    for (i = 0; i < hdesc->bundle_len; ++i) {
        void *value = m->get_var(i)->getPointer();
        switch (hdesc->bundle[i].val.getType()) {
        case VAR_INT:
            hdesc->bundle[i].val.setValue(*(int *)value);
            break;
        case VAR_STR:
            hdesc->bundle[i].val.setValue((char *)value);
            break;
        case VAR_REAL:
            hdesc->bundle[i].val.setValue(*(double *)value);
            break;
        }
    }
    delete m;

    hdesc->state = HARMONY_STATE_TESTING;
    return 1;
}

char *harmony_best_config(hdesc_t *hdesc)
{
    if (hdesc->state < HARMONY_STATE_CONNECTED) {
        fprintf(stderr, "Cannot receive best config until connected.\n");
        return NULL;
    }

    char *retval = request_tcl_variable(hdesc, "best_coord_so_far");

    if (retval != NULL) {
        for (int i = 0; retval[i] != '\0'; ++i) {
            if (retval[i] == ' ') {
                retval[i] = '_';
            }
        }
    }

    /* make sure you free the return val in the client side. */
    return retval;
}

int harmony_report(hdesc_t *hdesc, double value)
{
    if (hdesc->state < HARMONY_STATE_CONNECTED) {
        fprintf(stderr, "Cannot report until connected.\n");
        return -1;
    }

    /* Eventually, we'll want to do something different if the user
       reports performance before they request variable values, which
       implies a default value report.

       For now, just send the observed goodness message. */
    /*
    if (hdesc->state < HARMONY_STATE_TESTING) {
        return 0;
    }
    */

    /* create a new variable */
    VarDef *v = new VarDef("obsGoodness", VAR_REAL);
    v->setValue(value);
    HUpdateMessage *m = new HUpdateMessage(HMESG_PERF_UPDT, 0);

    if (debug_mode)
        printf("socket %d timestamp %d \n", hdesc->socket, hdesc->timestamp);

    /* first set the timestamp to be sent to the server */
    m->set_timestamp(hdesc->timestamp);
    m->set_var(*v);

    if (debug_mode)
        m->print();

    send_message(m, hdesc->socket);
    delete m;

    /* wait for the answer from the harmony server */
    HMessage *mesg = receive_message(hdesc->socket);
    if (mesg->get_type() == HMESG_FAIL) {
        /* could not send performance function */
        delete mesg;
        fprintf(stderr, "Server error during performance update.\n");
        return -1;
    }
    delete mesg;

    hdesc->state = HARMONY_STATE_CONNECTED;
    return 0;
}

int harmony_converged(hdesc_t *hdesc)
{
    if (hdesc->state < HARMONY_STATE_CONNECTED) {
        fprintf(stderr, "Cannot check convergence until connected.\n");
        return -1;
    }

    char *str = request_tcl_variable(hdesc, "search_done");
    if (str == NULL) {
        fprintf(stderr, "Error during convergence check.\n");
        return -1;
    }

    int retval = atoi(str);
    free(str);

    if (debug_mode)
        printf("harmony_check_convergence: %s\n", retval);

    if (retval != 0)
        return 1;

    return 0;
}

void *harmony_allocate(hdesc_t *hdesc, vartype_t type,
                       bundle_scope_t scope, const char *name)
{
    int idx;

    if (bundle_add_check(hdesc, name, NULL) < 0) {
        fprintf(stderr, "Add variable check failed.\n");
        return NULL;
    }

    idx = hdesc->bundle_len;
    hdesc->bundle[idx].scope = scope;
    hdesc->bundle[idx].flags = BUNDLE_FLAG_ALLOC;
    hdesc->bundle[idx].val.setName(name);
    hdesc->bundle[idx].val.setType((int)type);
    hdesc->bundle[idx].val.alloc();

    ++hdesc->bundle_len;
    return hdesc->bundle[idx].val.getPointer();
}

int harmony_register_int(hdesc_t *hdesc, const char *name, void *ptr,
                         int min, int max, int step)
{
    int idx;

    if (bundle_add_check(hdesc, name, ptr) < 0) {
        fprintf(stderr, "Add variable check failed.\n");
        return -1;
    }

    idx = hdesc->bundle_len;
    hdesc->bundle[idx].scope = BUNDLE_SCOPE_GLOBAL;
    hdesc->bundle[idx].flags = 0x0;
    hdesc->bundle[idx].val.setName(name);
    hdesc->bundle[idx].val.setType(VARTYPE_INT);
    hdesc->bundle[idx].val.setPointer(ptr);

    hdesc->bundle[idx].range.i.min = min;
    hdesc->bundle[idx].range.i.max = max;
    hdesc->bundle[idx].range.i.step = step;
    hdesc->bundle[idx].flags |= BUNDLE_FLAG_RANGED;

    ++hdesc->bundle_len;
    return 0;
}

int harmony_register_real(hdesc_t *hdesc, const char *name, void *ptr,
                          double min, double max, double step)
{
    int idx;

    if (bundle_add_check(hdesc, name, ptr) < 0) {
        fprintf(stderr, "Add variable check failed.\n");
        return -1;
    }

    idx = hdesc->bundle_len;
    hdesc->bundle[idx].scope = BUNDLE_SCOPE_GLOBAL;
    hdesc->bundle[idx].flags = 0x0;
    hdesc->bundle[idx].val.setName(name);
    hdesc->bundle[idx].val.setType(VARTYPE_REAL);
    hdesc->bundle[idx].val.setPointer(ptr);

    hdesc->bundle[idx].range.r.min = min;
    hdesc->bundle[idx].range.r.max = max;
    hdesc->bundle[idx].range.r.step = step;
    hdesc->bundle[idx].flags |= BUNDLE_FLAG_RANGED;

    ++hdesc->bundle_len;
    return 0;
}

int harmony_register_enum(hdesc_t *hdesc, const char *name, void *ptr)
{
    int idx;

    if (bundle_add_check(hdesc, name, ptr) < 0) {
        fprintf(stderr, "Add variable check failed.\n");
        return -1;
    }

    idx = hdesc->bundle_len;
    hdesc->bundle[idx].scope = BUNDLE_SCOPE_GLOBAL;
    hdesc->bundle[idx].flags = 0x0;
    hdesc->bundle[idx].val.setName(name);
    hdesc->bundle[idx].val.setType(VARTYPE_STR);
    hdesc->bundle[idx].val.setPointer(ptr);

    ++hdesc->bundle_len;
    return 0;
}

int harmony_range_enum(hdesc_t *hdesc, const char *name, const char *value)
{
    int i;
    bundle_t *bun;

    bun = bundle_find(hdesc, name);
    if (bun == NULL) {
        fprintf(stderr, "Harmony variable %s does not exist.\n", name);
        return -1;
    }

    for (i = 0; i < bun->range.e.set_len; ++i) {
        if (strcmp(value, bun->range.e.set[i]) == 0) {
            fprintf(stderr, "Enumeration set %s already contains %s\n",
                    bun->val.getName(), value);
            return -1;
        }
    }

    if (i == bun->range.e.set_capacity) {
        if (grow_array(&bun->range.e.set,
                       &bun->range.e.set_capacity, sizeof(char *)) < 0) {
            fprintf(stderr, "Memory allocation error.\n");
            return -1;
        }
    }

    char *s = (char *) malloc(strlen(value) + 1);
    strcpy(s, value);

    bun->flags |= BUNDLE_FLAG_RANGED;
    bun->range.e.set[i] = s;
    ++bun->range.e.set_len;

    return 0;
}

int harmony_unregister(hdesc_t *hdesc, const char *name)
{
    int i;
    bundle_t *bun;

    bun = bundle_find(hdesc, name);
    if (bun == NULL) {
        fprintf(stderr, "Harmony variable %s does not exist.\n", name);
        return -1;
    }

    /* Free allocated memory if this bundle was an enum. */
    if (bun->val.getType() == VAR_STR) {
        for(i = 0; i < bun->range.e.set_len; ++i) {
            free(bun->range.e.set[i]);
        }
        free(bun->range.e.set);
        bun->range.e.set_len = 0;
        bun->range.e.set_capacity = 0;
    }

    i = bun - hdesc->bundle;
    while (++i < hdesc->bundle_len) {
        hdesc->bundle[i - 1] = hdesc->bundle[i];
    }
    --hdesc->bundle_len;
    return 0;
}
