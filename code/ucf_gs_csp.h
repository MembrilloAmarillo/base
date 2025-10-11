#include <X11/Xlib.h>
#include <X11/keysymdef.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

// OMG GOMSPACE LIBRARY IS IMPOSSIBLE TO WORK WITH
//
#define GS_PARAM_INTERNAL_USE 1
#include <gs/param/internal/types.h>
#include <gs/param/types.h>


#include <gs/param/table.h>
#include <gs/param/rparam.h>
#include <gs/param/pp/pp.h>
#include <gs/csp/csp.h>
#include <gs/csp/drivers/kiss/kiss.h>
#include <gs/csp/drivers/can/can.h>
#include <gs/csp/command.h>
#include <gs/csp/service_dispatcher.h>
#include <gs/csp/linux/command_line.h>
#include <gs/csp/router.h>
#include <gs/aardvark/command_line.h>
#include <gs/ftp/client.h>
#include <gs/gosh/gscript/client.h>
#include <gs/rgosh/rgosh.h>
#include <gs/util/clock.h>
#include <gs/hk/client.h>
#include <gs/fp/fp_client.h>
#include <gs/util/linux/command_line.h>
#include <gs/util/gosh/command.h>
#include <gs/util/gosh/console.h>
#include <gs/util/crc32.h>
#include <gs/util/timestamp.h>

#include <csp/csp_types.h>
#include <gs/ftp/client.h>

#include <csp/interfaces/csp_if_zmqhub.h>
#include <csp/interfaces/csp_if_kiss.h>
#include <csp/interfaces/csp_if_can.h>

#include <csp_conn.h>
#include <csp/csp.h>
#include <csp/csp_buffer.h>
#include <csp/csp_types.h>
#include <csp/csp_crc32.h>
#include <gs/util/byteorder.h>

#include <stdlib.h>

#include "types.h"
#include "memory.h"
#include "allocator.h"
#include "zmq_proxy.h"
#include "ccsds_packet.h"
#include "commanding.h"
#include "net.h"
#include "HashTable.h"
#include "hk/hk.h"
#include "vector.h"
#include "queue.h"
#include "files.h"
#include "third-party/stb_image.h"
#include "third-party/stb_truetype.h"
#include "load_font.h"
#include "vk_render.h"
#include "new_ui.h"
#include "strings.h"
#include "ui_viewer.h"
#include "ui_render.h"

#define MEMORY_IMPL
#include "memory.h"

#define ALLOCATOR_IMPL
#include "allocator.h"

#define _ZMQ_PROXY_IMPL_
#include "zmq_proxy.h"

#define UCF_CCSDS_PACKET_IMPL
#include "ccsds_packet.h"

#include "commanding.h"

#define MY_NET_IMPL
#include "net.h"

#define STRINGS_IMPL
#include "strings.h"

#include "HashTable.h"
#include "HashTable.c"

#define HK_IMPL
#include "hk/hk.h"

#define VECTOR_IMPL
#include "vector.h"

#define QUEUE_IMPL
#include "queue.h"

#define FILES_IMPL
#include "files.h"

#define STB_IMAGE_IMPLEMENTATION
#include "third-party/stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "third-party/stb_truetype.h"

#define LOAD_FONT_IMPL
#include "load_font.h"

#define VK_RENDER_IMPL
#include "vk_render.h"

#define SP_UI_IMPL
#include "new_ui.h"

#define UI_RENDER_IMPL
#include "ui_render.h"

#define UI_VIEWER_IMPL
#include "ui_viewer.h"

#include "third-party/xxhash.c"

/**
 * @struct ftp_user_info_t
 * @brief  ftp progress information
 *
 * struct composed of user-related information while processing a file through ftp
 *
 *
 */
typedef struct ftp_user_info_t ftp_user_info_t;

struct ftp_user_info_t {
    FILE*       file_dst;   ///< File destination for the ftp action
    timestamp_t last_speed; ///< Duration of the last action performed (progress speed)
    timestamp_t last;       ///< Duration of the last whole function action
    F64         bps;        ///< Bits per second
    u32         last_chunk; ///< Last chunk processed
    gs_command_context_t* ctx;
    const char* LocalPath;
    const char* DstPath;
};

/**
 * @brief      csp zmqhub for tx, its an interface to work with that protocol
 *
 * @param      interface  The interface
 * @param      packet     The packet
 * @param[in]  timeout    The timeout
 *
 * @return     if packet was successfully transmitted, 0 on error
 */
int csp_zmqhub_tx(csp_iface_t * interface, csp_packet_t * packet, uint32_t timeout);

/**
 * @brief      Initialization of socketcan to work along the csp protocol
 *
 * @param[in]  ifc      The ifc
 * @param[in]  bitrate  The bitrate
 * @param[in]  promisc  The promisc
 *
 * @return     Pointer to an interface of that protocol
 */
csp_iface_t * csp_can_socketcan_init(const char * ifc, int bitrate, int promisc);


/**
 * @struct user_cmd
 * @brief  command line utiliites for user
 */
typedef struct user_cmd user_cmd;
struct user_cmd {
    hash_table OptionTable;
    hash_table OptionFunctions;
    hash_table OptionArgs;
};

/**
 * @brief Packet Error Rate Testing function for FLATSAT
 * @param[in] n_iterations        number of packets you want to send
 * @param[out] n_packet_success   number of packets that have been successfully received
 * @param[out] n_packet_fail      number of packets that have not been received correctly
 *
 * @note both n_packet_success and n_packet_fail have to be initialized by the
 * function caller (user).
 * @author Sascha Paez
 */
void PER_Testing(u64 n_iterations, u64* n_packet_success, u64* n_packet_fail);


/**
 * @brief Packet Error Rate Testing function for FLATSAT using File Transfer Protocol (over RDP)
 * @param[in] n_iterations      number of packets you want to send
 * @param[out]n_packet_success  number of packets that have been successfully received
 * @param[out]n_packet_fail     number of packets that have not been received correctly
 * @param[in] arena             Arena memory structure to handle memory [de]commiting
 * @param[in] set_remove        If set to true, it does not download and starts removing every file
 * @note: both n_packet_success and n_packet_fail have to bee initializeed by the function caller (user).
 *
 * @author Sascha Paez
*/
void  PER_TestingFTP(u64 n_iterations, u64* n_packet_success, u64* n_packet_fail, Arena* arena, bool set_remove);

/**
 * @brief Information callback for ftp transmission showing general status of the action
 * performed
 * @param[in] info Content info necessary to display the information
 *
 *
 */
void ftp_info_callback(const gs_ftp_info_t * info);

/**
 * @brief Progress information while FTP action
 *
 * @param current_chunk   Current chunk being processed
 * @param total_chuks     Number of chunks the file is composed of
 * @param info            Contains user defined information
 */
void progress_info(u32 current_chunk, u32 total_chunks, u32 chunk_size, ftp_user_info_t * info);

/**
 * @brief Shows the active configuration of a given parameter table id of a given node
 * @param Allocator Pointer to an Stack_Allocator struct for memory handling
 * @param [out]n_rows number of rows of that table
 * @param node Node of the system the table we want to get
 * @param TableId Id of the table we want to get
 * @return gs_param_table_row_t With the information about that table
 */
internal gs_param_table_row_t* ParameterTableList( Stack_Allocator* Allocator, i32* n_rows, u8 node, gs_param_table_id_t TableID );

// NOTE:
// NO MORE THAN 256 FILES TO DOWNLOAD AT ONCE
//
typedef struct ftp_hk_list ftp_hk_list;
struct ftp_hk_list {
    char     names[256][256];
    uint32_t current_idx;
    uint32_t size;
};

typedef struct debug_terminal debug_terminal;
struct debug_terminal {
    U8_String data;
    debug_terminal* Next;
    debug_terminal* Prev;
};

typedef struct debug_command debug_command;
struct debug_command {
    U8_String Command;
    debug_terminal Arguments;
};

typedef struct app_state app_state;
struct app_state {
    Arena*                    MainArena;
    Stack_Allocator           Allocator;
    Stack_Allocator           TempAllocator;
    ftp_hk_list               FTP_HK_DATA;
    char*                     SubZMQ_Str;
    char*                     PubZMQ_Str;
    gs_command_context_t      CommandCtx;
    udp_socket                UDP_Conn;
    gs_param_table_instance_t Instance;
    hk_list                   HK_Data;
    bool                      DebugMode;
    debug_terminal            DebugTerminal;
    debug_command             DebugCommand;
    U8_String                 CurrentRemotePath;
    U8_String                 CurrentFileToDownload;
    ui_context                UI_Context;
};