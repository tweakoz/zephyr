/*
 * Copyright (c) 2018 the CivetWeb developers
 * MIT License
 */
//#include <zephyr.h>

#include <kernel.h>

#include <net/socket.h>
#include <pthread.h>
#include <stdio.h>

/* Simple demo of a REST callback. */
//#include <stdlib.h>
//#include <string.h>
//#include <time.h>

//#include "cJSON.h"
#include "civetweb.h"

/* XXX needed for debug */
#define DEBUG_TRACE(fmt, ...) \
	do {                  \
	} while (0)

#define MGSQLEN (20) /* count */

/* Enum const for all options must be in sync with
 * static struct mg_option config_options[]
 * This is tested in the unit test (test/private.c)
 * "Private Config Options"
 */
enum {
	/* Once for each server */
	LISTENING_PORTS,
	NUM_THREADS,
	RUN_AS_USER,
	CONFIG_TCP_NODELAY, /* Prepended CONFIG_ to avoid conflict with the
	                     * socket option typedef TCP_NODELAY. */
	MAX_REQUEST_SIZE,
	LINGER_TIMEOUT,
#if defined(__linux__)
	ALLOW_SENDFILE_CALL,
#endif
#if defined(_WIN32)
	CASE_SENSITIVE_FILES,
#endif
	THROTTLE,
	ACCESS_LOG_FILE,
	ERROR_LOG_FILE,
	ENABLE_KEEP_ALIVE,
	REQUEST_TIMEOUT,
	KEEP_ALIVE_TIMEOUT,
#if defined(USE_WEBSOCKET)
	WEBSOCKET_TIMEOUT,
	ENABLE_WEBSOCKET_PING_PONG,
#endif
	DECODE_URL,
#if defined(USE_LUA)
	LUA_BACKGROUND_SCRIPT,
	LUA_BACKGROUND_SCRIPT_PARAMS,
#endif
#if defined(USE_TIMERS)
	CGI_TIMEOUT,
#endif

	/* Once for each domain */
	DOCUMENT_ROOT,
	CGI_EXTENSIONS,
	CGI_ENVIRONMENT,
	PUT_DELETE_PASSWORDS_FILE,
	CGI_INTERPRETER,
	PROTECT_URI,
	AUTHENTICATION_DOMAIN,
	ENABLE_AUTH_DOMAIN_CHECK,
	SSI_EXTENSIONS,
	ENABLE_DIRECTORY_LISTING,
	GLOBAL_PASSWORDS_FILE,
	INDEX_FILES,
	ACCESS_CONTROL_LIST,
	EXTRA_MIME_TYPES,
	SSL_CERTIFICATE,
	SSL_CERTIFICATE_CHAIN,
	URL_REWRITE_PATTERN,
	HIDE_FILES,
	SSL_DO_VERIFY_PEER,
	SSL_CA_PATH,
	SSL_CA_FILE,
	SSL_VERIFY_DEPTH,
	SSL_DEFAULT_VERIFY_PATHS,
	SSL_CIPHER_LIST,
	SSL_PROTOCOL_VERSION,
	SSL_SHORT_TRUST,

#if defined(USE_LUA)
	LUA_PRELOAD_FILE,
	LUA_SCRIPT_EXTENSIONS,
	LUA_SERVER_PAGE_EXTENSIONS,
#if defined(MG_EXPERIMENTAL_INTERFACES)
	LUA_DEBUG_PARAMS,
#endif
#endif
#if defined(USE_DUKTAPE)
	DUKTAPE_SCRIPT_EXTENSIONS,
#endif

#if defined(USE_WEBSOCKET)
	WEBSOCKET_ROOT,
#endif
#if defined(USE_LUA) && defined(USE_WEBSOCKET)
	LUA_WEBSOCKET_EXTENSIONS,
#endif

	ACCESS_CONTROL_ALLOW_ORIGIN,
	ACCESS_CONTROL_ALLOW_METHODS,
	ACCESS_CONTROL_ALLOW_HEADERS,
	ERROR_PAGES,
#if !defined(NO_CACHING)
	STATIC_FILE_MAX_AGE,
#endif
#if !defined(NO_SSL)
	STRICT_HTTPS_MAX_AGE,
#endif
	ADDITIONAL_HEADER,
	ALLOW_INDEX_SCRIPT_SUB_RES,

	NUM_OPTIONS
};

#define PORT "8089"
#define HOST_INFO "http://localhost:8089"

#define EXAMPLE_URI "/example"
#define EXIT_URI "/exit"

int exitNow = 0;

/* Unified socket address. For IPv6 support, add IPv6 address structure in
 * the
 * union u. */
union usa {
	struct sockaddr sa;
	struct sockaddr_in sin;
#if defined(USE_IPV6)
	struct sockaddr_in6 sin6;
#endif
};

/* Describes a string (chunk of memory). */
struct vec {
	const char *ptr;
	size_t len;
};

struct mg_file_stat {
	/* File properties filled by mg_stat: */
	uint64_t size;
	time_t last_modified;
	int is_directory; /* Set to 1 if mg_stat is called for a directory */
	int is_gzipped;   /* Set to 1 if the content is gzipped, in which
	                   * case we need a "Content-Eencoding: gzip" header */
	int location;     /* 0 = nowhere, 1 = on disk, 2 = in memory */
};

struct mg_file_in_memory {
	char *p;
	uint32_t pos;
	char mode;
};

struct mg_file_access {
	/* File properties filled by mg_fopen: */
	/* XXX */
	//FILE *fp;
	int fp;
#if defined(MG_USE_OPEN_FILE)
	/* TODO (low): Remove obsolete "file in memory" implementation.
	 * In an "early 2017" discussion at Google groups
	 * https://groups.google.com/forum/#!topic/civetweb/h9HT4CmeYqI
	 * we decided to get rid of this feature (after some fade-out
	 * phase). */
	const char *membuf;
#endif
};

struct mg_file {
	struct mg_file_stat stat;
	struct mg_file_access access;
};








struct socket {
	//SOCKET sock;             /* Listening socket */
	int sock;

	union usa lsa;           /* Local socket address */
	union usa rsa;           /* Remote socket address */
	unsigned char is_ssl;    /* Is port SSL-ed */
	unsigned char ssl_redir; /* Is port supposed to redirect everything to SSL
	                          * port */
	unsigned char in_use;    /* Is valid */
};

struct mg_context {

	/* Part 1 - Physical context:
	 * This holds threads, ports, timeouts, ...
	 * set for the entire server, independent from the
	 * addressed hostname.
	 */

	/* Connection related */
	int context_type; /* See CONTEXT_* above */

	struct socket *listening_sockets;
	struct pollfd *listening_socket_fds;
	unsigned int num_listening_sockets;

	struct mg_connection *worker_connections; /* The connection struct, pre-
	                                           * allocated for each worker */

#if defined(USE_SERVER_STATS)
	int active_connections;
	int max_connections;
	int64_t total_connections;
	int64_t total_requests;
	int64_t total_data_read;
	int64_t total_data_written;
#endif

	/* Thread related */
	volatile int stop_flag;       /* Should we stop event loop */
	pthread_mutex_t thread_mutex; /* Protects (max|num)_threads */

	pthread_t masterthreadid; /* The master thread ID */
	unsigned int
	    cfg_worker_threads;      /* The number of configured worker threads. */
	pthread_t *worker_threadids; /* The worker thread IDs */

/* Connection to thread dispatching */
#if defined(ALTERNATIVE_QUEUE)
	struct socket *client_socks;
	void **client_wait_events;
#else
	struct socket queue[MGSQLEN]; /* Accepted sockets */
	volatile int sq_head;         /* Head of the socket queue */
	volatile int sq_tail;         /* Tail of the socket queue */
	pthread_cond_t sq_full;       /* Signaled when socket is produced */
	pthread_cond_t sq_empty;      /* Signaled when socket is consumed */
#endif

	/* Memory related */
	unsigned int max_request_size; /* The max request size */

#if defined(USE_SERVER_STATS)
	struct mg_memory_stat ctx_memory;
#endif

	/* Operating system related */
	char *systemName;  /* What operating system is running */
	time_t start_time; /* Server start time, used for authentication
	                    * and for diagnstics. */

#if defined(USE_TIMERS)
	struct ttimers *timers;
#endif

/* Lua specific: Background operations and shared websockets */
#if defined(USE_LUA)
	void *lua_background_state;
#endif

	/* Server nonce */
	pthread_mutex_t nonce_mutex; /* Protects nonce_count */

	/* Server callbacks */
	struct mg_callbacks callbacks; /* User-defined callback function */
	void *user_data;               /* User-defined data */

	/* Part 2 - Logical domain:
	 * This holds hostname, TLS certificate, document root, ...
	 * set for a domain hosted at the server.
	 * There may be multiple domains hosted at one physical server.
	 * The default domain "dd" is the first element of a list of
	 * domains.
	 */
	struct mg_domain_context dd; /* default domain */
};


struct mg_connection {
	int connection_type; /* see CONNECTION_TYPE_* above */

	struct mg_request_info request_info;
	struct mg_response_info response_info;

	struct mg_context *phys_ctx;
	struct mg_domain_context *dom_ctx;

#if defined(USE_SERVER_STATS)
	int conn_state; /* 0 = undef, numerical value may change in different
	                 * versions. For the current definition, see
	                 * mg_get_connection_info_impl */
#endif

	const char *host;         /* Host (HTTP/1.1 header or SNI) */
	//SSL *ssl;                 /* SSL descriptor */
	//SSL_CTX *client_ssl_ctx;  /* SSL context for client connections */
	struct socket client;     /* Connected client */
	time_t conn_birth_time;   /* Time (wall clock) when connection was
	                           * established */
	struct timespec req_time; /* Time (since system start) when the request
	                           * was received */
	int64_t num_bytes_sent;   /* Total bytes sent to client */
	int64_t content_len;      /* Content-Length header value */
	int64_t consumed_content; /* How many bytes of content have been read */
	int is_chunked;           /* Transfer-Encoding is chunked:
	                           * 0 = not chunked,
	                           * 1 = chunked, do data read yet,
	                           * 2 = chunked, some data read,
	                           * 3 = chunked, all data read
	                           */
	size_t chunk_remainder;   /* Unread data from the last chunk */
	char *buf;                /* Buffer for received data */
	char *path_info;          /* PATH_INFO part of the URL */

	int must_close;       /* 1 if connection must be closed */
	int accept_gzip;      /* 1 if gzip encoding is accepted */
	int in_error_handler; /* 1 if in handler for user defined error
	                       * pages */
#if defined(USE_WEBSOCKET)
	int in_websocket_handling; /* 1 if in read_websocket */
#endif
	int handled_requests; /* Number of requests handled by this connection
	                       */
	int buf_size;         /* Buffer size */
	int request_len;      /* Size of the request + headers in a buffer */
	int data_len;         /* Total size of data in a buffer */
	int status_code;      /* HTTP reply status code, e.g. 200 */
	int throttle;         /* Throttling, bytes/sec. <= 0 means no
	                       * throttle */

	time_t last_throttle_time;   /* Last time throttled data was sent */
	int64_t last_throttle_bytes; /* Bytes sent this second */
	struct k_mutex mutex;       /* Used by mg_(un)lock_connection to ensure
	                              * atomic transmissions for websockets */
#if defined(USE_LUA) && defined(USE_WEBSOCKET)
	void *lua_websocket_state; /* Lua_State for a websocket connection */
#endif

	int thread_index; /* Thread index within ctx */
};













int
mg_printf(struct mg_connection *conn, const char *fmt, ...)
{
	va_list ap;

	(void)conn;

	va_start(ap, fmt);
	printk(fmt, ap);
	va_end(ap);

	return 0;
}

#define MG_BUF_LEN (1024 * 8)


/* XXX depends on MG_USE_OPEN_FILE */
#define STRUCT_FILE_INITIALIZER                                                \
	{                                                                          \
		{(uint64_t)0, (time_t)0, 0, 0, 0},                                     \
		{                                                                      \
			/* (FILE *)NULL                                                       \ */ \
			0 \
		}                                                                      \
	}


static int
mg_send_http_error_impl(struct mg_connection *conn,
                        int status,
                        const char *fmt,
                        va_list args)
{
	char errmsg_buf[MG_BUF_LEN];
	char path_buf[PATH_MAX];
	va_list ap;
	int len, i, page_handler_found, scope, truncated, has_body;
	char date[64];
	time_t curtime = time(NULL);
	const char *error_handler = NULL;
	struct mg_file error_page_file = STRUCT_FILE_INITIALIZER;
	const char *error_page_file_ext, *tstr;
	int handled_by_callback = 0;

	const char *status_text = mg_get_response_code_text(conn, status);

	if ((conn == NULL) || (fmt == NULL)) {
		return -2;
	}

	/* Set status (for log) */
	conn->status_code = status;

	/* Errors 1xx, 204 and 304 MUST NOT send a body */
	has_body = ((status > 199) && (status != 204) && (status != 304));

	/* Prepare message in buf, if required */
	if (has_body
	    || (!conn->in_error_handler
	        && (conn->phys_ctx->callbacks.http_error != NULL))) {
		/* Store error message in errmsg_buf */
		va_copy(ap, args);
		mg_vsnprintf(conn, NULL, errmsg_buf, sizeof(errmsg_buf), fmt, ap);
		va_end(ap);
		/* In a debug build, print all html errors */
		DEBUG_TRACE("Error %i - [%s]", status, errmsg_buf);
	}

	/* If there is a http_error callback, call it.
	 * But don't do it recursively, if callback calls mg_send_http_error again.
	 */
	if (!conn->in_error_handler
	    && (conn->phys_ctx->callbacks.http_error != NULL)) {
		/* Mark in_error_handler to avoid recursion and call user callback. */
		conn->in_error_handler = 1;
		handled_by_callback =
		    (conn->phys_ctx->callbacks.http_error(conn, status, errmsg_buf)
		     == 0);
		conn->in_error_handler = 0;
	}

	if (!handled_by_callback) {
		/* Check for recursion */
		if (conn->in_error_handler) {
			DEBUG_TRACE(
			    "Recursion when handling error %u - fall back to default",
			    status);
		} else {
			/* Send user defined error pages, if defined */
			error_handler = conn->dom_ctx->config[ERROR_PAGES];
			error_page_file_ext = conn->dom_ctx->config[INDEX_FILES];
			page_handler_found = 0;

			if (error_handler != NULL) {
				for (scope = 1; (scope <= 3) && !page_handler_found; scope++) {
					switch (scope) {
					case 1: /* Handler for specific error, e.g. 404 error */
						mg_snprintf(conn,
						            &truncated,
						            path_buf,
						            sizeof(path_buf) - 32,
						            "%serror%03u.",
						            error_handler,
						            status);
						break;
					case 2: /* Handler for error group, e.g., 5xx error
					         * handler
					         * for all server errors (500-599) */
						mg_snprintf(conn,
						            &truncated,
						            path_buf,
						            sizeof(path_buf) - 32,
						            "%serror%01uxx.",
						            error_handler,
						            status / 100);
						break;
					default: /* Handler for all errors */
						mg_snprintf(conn,
						            &truncated,
						            path_buf,
						            sizeof(path_buf) - 32,
						            "%serror.",
						            error_handler);
						break;
					}

					/* String truncation in buf may only occur if
					 * error_handler is too long. This string is
					 * from the config, not from a client. */
					(void)truncated;

					len = (int)strlen(path_buf);

					tstr = strchr(error_page_file_ext, '.');

					while (tstr) {
						for (i = 1;
						     (i < 32) && (tstr[i] != 0) && (tstr[i] != ',');
						     i++) {
							/* buffer overrun is not possible here, since
							 * (i < 32) && (len < sizeof(path_buf) - 32)
							 * ==> (i + len) < sizeof(path_buf) */
							path_buf[len + i - 1] = tstr[i];
						}
						/* buffer overrun is not possible here, since
						 * (i <= 32) && (len < sizeof(path_buf) - 32)
						 * ==> (i + len) <= sizeof(path_buf) */
						path_buf[len + i - 1] = 0;

						if (mg_stat(conn, path_buf, &error_page_file.stat)) {
							DEBUG_TRACE("Check error page %s - found",
							            path_buf);
							page_handler_found = 1;
							break;
						}
						DEBUG_TRACE("Check error page %s - not found",
						            path_buf);

						tstr = strchr(tstr + i, '.');
					}
				}
			}

			if (page_handler_found) {
				conn->in_error_handler = 1;
				handle_file_based_request(conn, path_buf, &error_page_file);
				conn->in_error_handler = 0;
				return 0;
			}
		}

		/* No custom error page. Send default error page. */
		gmt_time_string(date, sizeof(date), &curtime);

		conn->must_close = 1;
		mg_printf(conn, "HTTP/1.1 %d %s\r\n", status, status_text);
		send_no_cache_header(conn);
		send_additional_header(conn);
		if (has_body) {
			mg_printf(conn,
			          "%s",
			          "Content-Type: text/plain; charset=utf-8\r\n");
		}
		mg_printf(conn,
		          "Date: %s\r\n"
		          "Connection: close\r\n\r\n",
		          date);

		/* HTTP responses 1xx, 204 and 304 MUST NOT send a body */
		if (has_body) {
			/* For other errors, send a generic error message. */
			mg_printf(conn, "Error %d: %s\n", status, status_text);
			mg_write(conn, errmsg_buf, strlen(errmsg_buf));

		} else {
			/* No body allowed. Close the connection. */
			DEBUG_TRACE("Error %i", status);
		}
	}
	return 0;
}

int
mg_send_http_error(struct mg_connection *conn, int status, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = mg_send_http_error_impl(conn, status, fmt, ap);
	va_end(ap);

	return ret;
}

# if 0
static int
SendJSON(struct mg_connection *conn, cJSON *json_obj)
{
	char *json_str = cJSON_PrintUnformatted(json_obj);
	size_t json_str_len = strlen(json_str);

	/* Send HTTP message header */
	mg_send_http_ok(conn, "application/json; charset=utf-8", json_str_len);

	/* Send HTTP message content */
	mg_write(conn, json_str, json_str_len);

	/* Free string allocated by cJSON_Print* */
	cJSON_free(json_str);

	return (int)json_str_len;
}

static unsigned request = 0; /* demo data: request counter */

static int
ExampleGET(struct mg_connection *conn)
{
	cJSON *obj = cJSON_CreateObject();

	if (!obj) {
		/* insufficient memory? */
		mg_send_http_error(conn, 500, "Server error");
		return 500;
	}


	cJSON_AddStringToObject(obj, "version", CIVETWEB_VERSION);
	cJSON_AddNumberToObject(obj, "request", ++request);
	SendJSON(conn, obj);
	cJSON_Delete(obj);

	return 200;
}

static int
ExampleDELETE(struct mg_connection *conn)
{
	request = 0;
	mg_send_http_error(conn,
	                   204,
	                   "%s",
	                   ""); /* Return "deleted" = "204 No Content" */

	return 204;
}

static int
ExamplePUT(struct mg_connection *conn)
{
	char buffer[1024];
	int dlen = mg_read(conn, buffer, sizeof(buffer) - 1);
	cJSON *obj, *elem;
	unsigned newvalue;

	if ((dlen < 1) || (dlen >= sizeof(buffer))) {
		mg_send_http_error(conn, 400, "%s", "No request body data");
		return 400;
	}
	buffer[dlen] = 0;

	obj = cJSON_Parse(buffer);
	if (obj == NULL) {
		mg_send_http_error(conn, 400, "%s", "Invalid request body data");
		return 400;
	}

	elem = cJSON_GetObjectItemCaseSensitive(obj, "request");

	if (!cJSON_IsNumber(elem)) {
		cJSON_Delete(obj);
		mg_send_http_error(conn,
		                   400,
		                   "%s",
		                   "No \"request\" number in body data");
		return 400;
	}

	newvalue = (unsigned)elem->valuedouble;

	if ((double)newvalue != elem->valuedouble) {
		cJSON_Delete(obj);
		mg_send_http_error(conn,
		                   400,
		                   "%s",
		                   "Invalid \"request\" number in body data");
		return 400;
	}

	request = newvalue;
	cJSON_Delete(obj);

	mg_send_http_error(conn, 201, "%s", ""); /* Return "201 Created" */

	return 201;
}

static int
ExamplePOST(struct mg_connection *conn)
{
	/* In this example, do the same for PUT and POST */
	return 0; //ExamplePUT(conn);
}

static int
ExamplePATCH(struct mg_connection *conn)
{
	/* In this example, do the same for PUT and PATCH */
	return 0; //ExamplePUT(conn);
}
# endif


static int
ExampleHandler(struct mg_connection *conn, void *cbdata)
{

	(void)cbdata; /* currently unused */
# if 0
	const struct mg_request_info *ri = mg_get_request_info(conn);

	if (0 == strcmp(ri->request_method, "GET")) {
		return ExampleGET(conn);
	}
	if (0 == strcmp(ri->request_method, "PUT")) {
		return ExamplePUT(conn);
	}
	if (0 == strcmp(ri->request_method, "POST")) {
		return ExamplePOST(conn);
	}
	if (0 == strcmp(ri->request_method, "DELETE")) {
		return ExampleDELETE(conn);
	}
	if (0 == strcmp(ri->request_method, "PATCH")) {
		return ExamplePATCH(conn);
	}
# endif

	/* this is not a GET request */
	mg_send_http_error(
	    conn, 405, "Only GET, PUT, POST, DELETE and PATCH method supported");
	return 405;
}


int
ExitHandler(struct mg_connection *conn, void *cbdata)
{
	mg_printf(conn,
	          "HTTP/1.1 200 OK\r\nContent-Type: "
	          "text/plain\r\nConnection: close\r\n\r\n");
	mg_printf(conn, "Server will shut down.\n");
	mg_printf(conn, "Bye!\n");
	exitNow = 1;
	return 1;
}


int
log_message(const struct mg_connection *conn, const char *message)
{
	printf("%s\n", message);
	return 1;
}


int
main(int argc, char *argv[])
{
	const char *options[] = {"listening_ports",
	                         PORT,
	                         "request_timeout_ms",
	                         "10000",
	                         "error_log_file",
	                         "error.log",
	                         "enable_auth_domain_check",
	                         "no",
	                         0};

	struct mg_callbacks callbacks;
	struct mg_context *ctx;
	int err = 0;

/* Check if libcivetweb has been built with all required features. */
	mg_init_library(0);

	if (err) {
		printf("Cannot start CivetWeb - inconsistent build.\n");
		return EXIT_FAILURE;
	}


	/* Callback will print error messages to console */
	memset(&callbacks, 0, sizeof(callbacks));
	callbacks.log_message = log_message;

	/* Start CivetWeb web server */
	ctx = mg_start(&callbacks, 0, options);

	/* Check return value: */
	if (ctx == NULL) {
		printf("Cannot start CivetWeb - mg_start failed.\n");
		return EXIT_FAILURE;
	}

	/* Add handler EXAMPLE_URI, to explain the example */
	mg_set_request_handler(ctx, EXAMPLE_URI, ExampleHandler, 0);
	mg_set_request_handler(ctx, EXIT_URI, ExitHandler, 0);

	/* Show sone info */
	printf("Start example: %s%s\n", HOST_INFO, EXAMPLE_URI);
	printf("Exit example:  %s%s\n", HOST_INFO, EXIT_URI);


	/* Wait until the server should be closed */
	while (!exitNow) {
		k_sleep(1000);
	}

	/* Stop the server */
	mg_stop(ctx);

	printf("Server stopped.\n");
	printf("Bye!\n");

	return EXIT_SUCCESS;
}

