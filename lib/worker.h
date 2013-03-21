#ifndef LIBNAGIOS_worker_h__
#define LIBNAGIOS_worker_h__
#include <errno.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "libnagios.h"

/**
 * @file worker.h
 * @brief Worker implementation along with various helpers
 *
 * This code isn't really in the "library" category, but it's tucked
 * in here to provide a good resource for writing remote workers and
 * as an example on how to use the API's found here.
 */

#ifndef ETIME
#define ETIME ETIMEDOUT
#endif

struct worker_process;

/** Worker job data */
typedef struct worker_job {
	int id;         /**< job id */
	int type;       /**< internal only */
	time_t timeout; /**< timeout, in absolute time */
	char *command;  /**< command string for this job */
	struct worker_process *wp; /**< worker process running this job */
	void *arg;      /**< any random argument */
} worker_job;

/** A worker process as seen from its controller */
typedef struct worker_process {
	const char *type; /**< identifying typename of this worker */
	char *source_name; /**< check-source name of this worker */
	int sd;    /**< communication socket */
	pid_t pid; /**< pid */
	int max_jobs; /**< Max number of jobs we can handle */
	int jobs_running; /**< jobs running */
	int jobs_started; /**< jobs started */
	struct timeval start; /**< worker start time */
	iocache *ioc; /**< iocache for reading from worker */
	worker_job **jobs; /**< array of jobs */
	int job_index; /**< round-robin slot allocator (this wraps) */
	struct worker_process *prev_wp; /**< previous worker in list */
	struct worker_process *next_wp; /**< next worker in list */
} worker_process;

typedef struct iobuf {
	int fd;
	unsigned int len;
	char *buf;
} iobuf;

typedef struct execution_information execution_information;

typedef struct child_process {
	unsigned int id, timeout;
	char *cmd;
	int ret;
	struct kvvec *request;
	iobuf outstd;
	iobuf outerr;
	execution_information *ei;
} child_process;

/**
 * Callback for enter_worker that simply runs a command
 */
extern int start_cmd(child_process *cp);

/**
 * Spawn a helper with a specific process name
 * The first entry in the argv parameter will be the name of the
 * new process, unless the process changes the name itself.
 * @param path The path to the executable (can be $PATH relative)
 * @param argv Argument vector for the helper to spawn
 */
extern int spawn_named_helper(char *path, char **argv);

/**
 * Spawn any random helper process. Uses spawn_named_helper()
 * @param argv The (NULL-sentinel-terminated) argument vector
 * @return 0 on success, < 0 on errors
 */
extern int spawn_helper(char **argv);

/**
 * To be called when a child_process has completed to ship the result to nagios
 * @param cp The child_process that describes the job
 * @param reason 0 if everything was OK, 1 if the job was unable to run
 * @return 0 on success, non-zero otherwise
 */
extern int finish_job(child_process *cp, int reason);

/**
 * Start to poll the socket and call the callback when there are new tasks
 * @param sd A socket descriptor to poll
 * @param cb The callback to call upon completion
 */
extern void enter_worker(int sd, int (*cb)(child_process*));

/**
 * Build a buffer from a key/value vector buffer.
 * The resulting kvvec-buffer is suitable for sending between
 * worker and master in either direction, as it has all the
 * right delimiters in all the right places.
 * @param kvv The key/value vector to build the buffer from
 * @return NULL on errors, a newly allocated kvvec buffer on success
 */
extern struct kvvec_buf *build_kvvec_buf(struct kvvec *kvv);

/**
 * Send a key/value vector as a bytestream through a socket
 * @param[in] sd The socket descriptor to send to
 * @param kvv The key/value vector to send
 * @return The number of bytes sent, or -1 on errors
 */
extern int worker_send_kvvec(int sd, struct kvvec *kvv);

/** @deprecated Use worker_send_kvvec() instead */
extern int send_kvvec(int sd, struct kvvec *kvv)
	NAGIOS_DEPRECATED(4.1.0, "worker_send_kvvec()");

/**
 * Grab a worker message from an iocache buffer
 * @param[in] ioc The io cache
 * @param[out] size Out buffer for buffer length
 * @param[in] flags Currently unused
 * @return A buffer from the iocache on succes; NULL on errors
 */
extern char *worker_ioc2msg(iocache *ioc, unsigned long *size, int flags);

/**
 * Parse a worker message to a preallocated key/value vector
 *
 * @param[in] kvv Key/value vector to fill
 * @param[in] buf The buffer to parse
 * @param[in] len Length of 'buf'
 * @param[in] kvv_flags Flags for buf2kvvec()
 * @return 0 on success, < 0 on errors
 */
extern int worker_buf2kvvec_prealloc(struct kvvec *kvv, char *buf, unsigned long len, int kvv_flags);

/**
 * Set some common socket options
 * @param[in] sd The socket to set options for
 * @param[in] bufsize Size to set send and receive buffers to
 * @return 0 on success. < 0 on errors
 */
extern int set_socket_options(int sd, int bufsize);
#endif /* INCLUDE_worker_h__ */
