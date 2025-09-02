#ifndef _ZMQ_PROXY_H_
#define _ZMQ_PROXY_H_

#include <zmq.h>
#include <pthread.h>

#define CHECK_NULL(val, info_format, ...)   do {        \
if( val == NULL )                               \
fprintf(stderr, info_format, __VA_ARGS__ ); \
} while(0);

#define CHECK_FALSE(val, info_format, ...)   do {       \
if( val )                                       \
fprintf(stderr, info_format, __VA_ARGS__ ); \
} while(0);

typedef struct zmq_message_list zmq_message_list;
struct zmq_message_list {
    zmq_msg_t msg;

    zmq_message_list* first; // First message ever
    zmq_message_list* last;  // Last nessage ever
    zmq_message_list* next;  // Next message
    zmq_message_list* prev;  // Previous message
};

typedef struct zmq_conn_data zmq_conn_data;
struct zmq_conn_data {
    void*            zmq_context;    // zmq context for current thread
    void*            src_socket;     // src socket
    void*            dst_socket;     // dst socket
    const char*      src;            // source address + port, (e.g tcp://0.0.0.0:6000)
    const char*      dst;            // destination address + port
    pthread_t        task_thread_id; // Thread id of connection data
    zmq_message_list messages;       // ZMQ Message List
};

typedef struct zmq_data_thread zmq_data_thread;
struct zmq_data_thread {
    const char* src;
    const char* dst;
    bool       bind;
};

internal zmq_conn_data start_zmq_job( const char* src, const char* dst, bool bind );

internal void thread_start_zmq_job( const char* src, const char* dst, bool bind );

internal void * zmq_main_loop( void* data_thread );

#endif

#ifdef _ZMQ_PROXY_IMPL_

/**
 * @todo Add as an argument a pointer to a function that processes the data
 */
internal void *
zmq_main_loop( void* data_thread_ctx ) {

    zmq_conn_data* data = (zmq_conn_data*) data_thread_ctx;


    /* Subscriber (RX) */
    void * subscriber = zmq_socket(data->zmq_context, ZMQ_SUB);
    i32 ret = zmq_connect(subscriber, data->src);
    if (ret == -1) {
        fprintf(stderr, "[ERROR] Could not connect to socket %s\n", data->src);
        return NULL;
    }

    ret = zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0);
    if (ret == -1) {
        fprintf(stderr, "[ERROR] Could not set socket options to socket %s\n", data->src);
        return NULL;
    }

    while (1) {
        zmq_msg_t msg;
        zmq_msg_init_size(&msg, 1024);

        fprintf(stdout, "[INFO] ZeroMQ waiting for package, listening to port: %s\n", data->src);
        if (zmq_msg_recv(&msg, subscriber, 0) < 0) {
            zmq_msg_close(&msg);
            fprintf( stderr, "[ERROR] ZMQ: %s, %s\n", zmq_strerror(zmq_errno()), data->src);
            continue;
        }

        /**
         * @todo Probably add as an argument a pointer to a function
         * to process the incoming data
         */

        zmq_msg_close(&msg);
    }

    return NULL;
}

internal void*
proxy_start( void* data_thread ) {
    zmq_conn_data* data = (zmq_conn_data*) data_thread;

    zmq_proxy( data->src_socket, data->dst_socket, NULL );

    return NULL;
}

internal void
thread_start_zmq_job(  const char* src, const char* dst, bool bind ) {
    Arena* arena = ArenaAllocDefault();
    pthread_t task;

    zmq_conn_data data = start_zmq_job( src, dst, bind );

    zmq_conn_data* data_thread = PushArray( arena, zmq_conn_data, 1);
    memcpy( data_thread, &data, sizeof(data) );

    if( pthread_create( &task, NULL, zmq_main_loop, (void*)data_thread ) != 0 ) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    // start proxy
    //
    if (bind) {
        if( pthread_create( &task, NULL, proxy_start, (void*)data_thread ) != 0 ) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
}

internal zmq_conn_data
start_zmq_job( const char* src, const char* dst, bool bind )
{
    zmq_conn_data zmq_job;

    zmq_job.zmq_context = zmq_ctx_new();
    CHECK_NULL(zmq_job.zmq_context, "[ERROR] Could not create zmq context %s\n", __FILE__ );

    void* ctx = zmq_job.zmq_context;

    zmq_job.src_socket = zmq_socket(ctx, ZMQ_XSUB);
    zmq_job.dst_socket = zmq_socket(ctx, ZMQ_XPUB);
    zmq_job.src        = src;
    zmq_job.dst        = dst;

    if( bind ) {
        i32 ret = zmq_bind( zmq_job.src_socket, src);
        CHECK_FALSE(ret, "[ERROR] Could not bind socket %s\n", src);
        ret     = zmq_bind( zmq_job.dst_socket, dst);
        CHECK_FALSE(ret, "[ERROR] Could not bind socket %s\n", dst);
    }

    return zmq_job;
}

#endif
