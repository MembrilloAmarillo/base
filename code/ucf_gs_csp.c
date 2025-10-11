/**
 * UCAnFly middleware for FLATSAT testing consisting
 * on csp functionalities in order to command and retreive
 * information from OBC and transceiver.
 *
 * @author Sascha Paez
 * @version 1.4.2
 * @since 2025
 *
 */

/** LOG -- Information
* 15/09/2025. ADCS Data processing fixed. Also fixed how the param set was being
processed.
* 12/09/2025. Fixed processing for HK data and validated HK processing. Still
problems with ADCS as it is not byte-aligned.
* 12/09/2025. Now the HK processing is done per bit-offset. That is due to the
nature of packed structures and non-aligned-memory data. With this it processes
the HK data more granularly. However more testing shall be done.
* 08/09/2025. Fixing how the processor checks each HK instance, still problems
with the date.
* 08/09/2025. Finished first version of HK file processor, each hk instance on
the file is processed and then packed into a ccsds packet of service PUS[3, 25]
to be sent to YAMCS. Further testing on FLATSAT shall be done
* 08/09/2025. Added processor to get the parameter set of each housekeeping file
so that is known which parameters are currently enable on each system.
* 08/09/2025. Added a stop condition for gpu rendering and main thread
computation while no event for the user is being given or an unfocusing event
was set. All separate network threads are still on work for packet processing.
* 05/09/2025. Fixed Param table listing of OBC. Also added Auto-Remove
functionality in the GUI
* 04/09/2025. Refactoring, added documentation page, and erased DownlaodOBC_Hk
function.
*             Also added ParameterTableList function to show the active config
parameter table id
*             of the node (e.g. OBC, Transceiver).
* 02/09/2025. Finished render and ui functionality from last version.
* 28/08/2025. Added compute and graphics pipeline for testing
* 28/08/2025. Fixed memory issues. Added new memory allocator strategy (Stack
Allocator).
* 27/08/2025. Completely removed graphics library to a custom high performance
one.
* 13/08/2025. When downloading HK data through FTP, process it and actually
create a ccsds tm packet that is being sent to YAMCS.
*             * Fixed errors
*             * Added better font
* 08/08/2025. Added rparam download functionality to download OBC telemetry
directly from CSP. Still needs testing on hardware -- DownloadOBC_Hk
*
*/

/**
1769960393.631931 E csp: RDP 0xd0124924: Timeout during send
1769960393.635223 E ftp: Data transaction failed
1769960398.406801 N default: ADCS data collected.
1769960404.406852 N default: ADCS data collected.
1769960407.162902 N default: Beacon generated.
1769960410.407006 N default: ADCS data collected.
1769960415.884662 N default: Transceiver data collected.
1769960416.169042 N default: EPS PBU data collected.
1769960416.289712 N default: Solar Panel data collected.
1769960416.475559 N default: EPS PDU data collected.
1769960416.507175 N default: EPS PIU data collected.
1769960416.601683 N default: EPS System Status data collected.
1769960416.683496 N default: ADCS data collected.
1769960421.423461 E csp: RDP 0xd0124d74: Invalid ACK number! 0 not between 17256
and 17269 1769960422.406965 N default: ADCS data collected. 1769960428.406999 N
default: ADCS data collected. 1769960434.407015 N default: ADCS data collected.
1769960437.175063 N default: Beacon generated.
1769960440.407022 N default: ADCS data collected.
1769960443.374954 W csp: RDP 0xd0124d74: Found a lost connection (now: 2227393,
ts: 2197360, to: 30000), closing 1769960445.880074 N default: Transceiver data
collected. 1769960446.178982 N default: EPS PBU data collected.
1769960446.294689 N default: Solar Panel data collected.

*/
#include "ucf_gs_csp.h"

global app_state AppState;

/* ======================================================================================================
 */

static void csp_service_dispatcher(void) {
  static const gs_csp_service_handler_t handlers[31] = {
      GS_CSP_BASIC_SERVICE_HANDLERS,
  };

  // Configuration for service handler - must remain valid as long as service
  // handler is running
  static const gs_csp_service_dispatcher_conf_t conf = {
      .name = "CSPCLIENTSRV",
      .bind_any = true, // bind to any un-bounded ports, used for logging
                        // unexpected connections
      .disable_watchdog = true,
      .handler_array = handlers,
      .handler_array_size = GS_ARRAY_SIZE(handlers),
  };

  gs_csp_service_dispatcher_create(&conf, 0, GS_THREAD_PRIORITY_NORMAL, NULL);
}

/* ======================================================================================================
 */

static void *udp_transactions_client() {
  for (u64 i = 0; i < U64_MAX; i += 1) {
    csp_conn_t *conn = csp_connect(0, 1, 10, 1000, CSP_O_CRC32);

    bool is_ftp = false;

    ssize_t n = recvfrom(AppState.UDP_Conn.sockfd, AppState.UDP_Conn.buffer,
                         UDP_BUFSIZE - 1, // Dejar espacio para null terminator
                         0, (struct sockaddr *)&AppState.UDP_Conn.server_addr,
                         &AppState.UDP_Conn.ser_len);

    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        fprintf(stderr, "[UDP | WARN] No data available, retrying...\n");
        continue;
      } else {
        fprintf(stderr, "[UDP | ERROR] recvfrom failed: %s\n", strerror(errno));
        break;
      }
    } else if (n == 0) {
      fprintf(stderr, "[UDP | WARN] Received empty packet\n");
      continue;
    }

    AppState.UDP_Conn.buffer[n] = '\0';

    fprintf(stdout, "[UDP | REC] Received %zd bytes: ", n);
    for (ssize_t i = 0; i < n; i++) {
      if (isprint(AppState.UDP_Conn.buffer[i])) {
        fputc(AppState.UDP_Conn.buffer[i], stdout);
      } else {
        fprintf(stdout, "\\x%02x", (unsigned char)AppState.UDP_Conn.buffer[i]);
      }
    }
    fprintf(stdout, "\n");

    csp_packet_t *packet = csp_buffer_get(n);
    if (packet == NULL) {
      fprintf(stderr, "[ERROR] Could not allocate CSP packet\n");
      continue;
    }

    memcpy(packet->data, AppState.UDP_Conn.buffer, n);
    packet->length = n;

#if 0
        ucf_ccsds_packet_tc_t tc;
        memcpy(&tc, AppState.UDP_Conn.buffer, n);

        fprintf(stdout, "Version: %d\nType: %d\nSecondary Header Flag: %d\nApid: %d\nLength: %d\nType: %d\nSubtype: %d\n",
            tc.version,
            tc.type,
            tc.secondary_header_flag,
            tc.apid,
            tc.length,
            tc.secondary_header.service_type,
            tc.secondary_header.service_subtype);
#endif
    memset(AppState.UDP_Conn.buffer, 0, UDP_BUFSIZE);

    // Enviar packet
    if (csp_send(conn, packet, 1000) == 0) {
      fprintf(stderr, "[ERROR] Could not send packet to GNU radio\n");
      csp_buffer_free(packet);
    }

    csp_packet_t *p;
    while ((p = csp_read(conn, 500)) != NULL) {
      printf("Packet received\n");
      // Enviar por UDP
      ssize_t sent_bytes =
          send(AppState.UDP_Conn.cli_sockfd, p->data, p->length, 0);
    }
    if (p != NULL) {
      csp_buffer_free(p);
    }
    if (packet != NULL) {
      csp_buffer_free(packet);
    }
    csp_close(conn);
  }
  return NULL;
}

/* ======================================================================================================
 */

static void *udp_transactions_server() {
  csp_socket_t *sock = csp_socket(CSP_O_CRC32);
  if (sock == NULL) {
    fprintf(stderr, "[ERROR] Socket creation failed\n");
    return NULL;
  }

  int err = csp_bind(sock, CSP_ANY);
  if (err == -1) {
    fprintf(stderr, "[ERROR] Bind failed\n");
    return NULL;
  }

  i32 err_listen = csp_listen(sock, 10);
  if (err_listen == -1) {
    fprintf(stderr, "[ERROR] Listen failed\n");
    return NULL;
  }

  /* Pointer to current connection and packet */
  csp_packet_t *p;
  for (u64 i = 0; i < U64_MAX; i += 1) {

    csp_conn_t *conn;
    /* Wait for connection, 10000 ms timeout */
    if ((conn = csp_accept(sock, UINT32_MAX)) == NULL) {
      fprintf(stdout, "Connection not accepted\n");
      continue;
    }
    if ((p = csp_read(conn, 1000)) != NULL) {
      fprintf(stdout, "\nRead packet %p\n", p);
      switch (csp_conn_dport(conn)) {
      case 20: {
        // int err_crc32 = csp_crc32_verify(p, true);
        // if( err_crc32 != CSP_ERR_NONE ) {
        //     fprintf(stderr, "[ERROR] CRC32 Error from incoming packet\n");
        // }
        //  Enviar por UDP
        ssize_t sent_bytes =
            send(AppState.UDP_Conn.cli_sockfd, p->data, p->length, 0);

        csp_buffer_free(p);
      } break;
      default:
        /* Let the service handler reply pings, buffer use, etc. */
        // csp_service_handler(conn, packet);
        fprintf(stdout, "Comming from dport: %d\n", csp_conn_dport(conn));
        break;
      }
    } else {
      fprintf(stdout, "Could not read packet\n");
    }

    csp_close(conn);
  }

  return NULL;
}

/* ======================================================================================================
 */

static double timespec_diff(timestamp_t *start, timestamp_t *end) {
  struct timespec temp;
  if (((double)end->tv_nsec - (double)start->tv_nsec) < 0) {
    temp.tv_sec = end->tv_sec - start->tv_sec - 1;
    temp.tv_nsec = 1000000000 + end->tv_nsec - start->tv_nsec;
  } else {
    temp.tv_sec = end->tv_sec - start->tv_sec;
    temp.tv_nsec = end->tv_nsec - start->tv_nsec;
  }
  return (double)(temp.tv_sec + (double)temp.tv_nsec / 1000000000);
}

/* ======================================================================================================
 */

static char *string_concat(char *str1, char *str2) {
  char *string_concat = (char *)malloc(UCF_Strlen(str1) + UCF_Strlen(str2) + 1);

  memcpy(string_concat, str1, UCF_Strlen(str1));
  memcpy(string_concat + UCF_Strlen(str1), str2, UCF_Strlen(str2));

  string_concat[UCF_Strlen(str1) + UCF_Strlen(str2)] = '\0';

  return string_concat;
}

/* ======================================================================================================
 */

void progress_info(u32 current_chunk, u32 total_chunks, u32 chunk_size,
                   ftp_user_info_t *info) {
  double sec, speed_sec, alpha = 0.25;
  char buf[200] = "";
  int idx = 0;
  uint32_t eta_sec = 0;
  timestamp_t now;

  /* Only print progress to TTY */
  if (!isatty(fileno(info->file_dst))) {
    return;
  }
  gs_clock_get_monotonic(&now);
  sec = timespec_diff(&info->last, &now);
  speed_sec = timespec_diff(&info->last_speed, &now);

  if (sec > 1.0 || current_chunk == total_chunks - 1) {

    /* Update speed and ETA */
    if (speed_sec >= 1.0) {
      /* Exponentially smoothed bytes per second */
      double this_bps =
          (((current_chunk + 1 - info->last_chunk) * chunk_size) / speed_sec);
      info->bps =
          info->bps > 0 ? this_bps * alpha + info->bps * (1 - alpha) : this_bps;

      /* Remaining time */
      eta_sec = ((total_chunks - current_chunk + 1) * chunk_size) / info->bps;

      /* Update last time */
      info->last_chunk = current_chunk;
      info->last_speed = now;
    }

    info->last = now;
  }

  printf("*---------------------------------------*\n");
  printf("| FTP INFO                              |\n");
  printf("| File bps   : %6lf               |\n", info->bps);
  printf("| File speed : %2d                    |\n", info->last_speed.tv_sec);
  printf("| Last chunk : %6d                |\n", info->last_chunk);
  printf("*---------------------------------------*\n");
}

/* ======================================================================================================
 */

void ftp_info_callback(const gs_ftp_info_t *info) {
  ftp_user_info_t *user = info->user_data;

  switch (info->type) {
  case GS_FTP_INFO_COMPLETED: {
    uint32_t complete = info->u.completed.completed_chunks;
    uint32_t total = info->u.completed.total_chunks;
    double percent;
    fprintf(stdout, "Transfer Status: ");
    if ((complete == 0) && (total == 0)) {
      percent = 100;
    } else {
      percent = (double)complete * 100 / (double)total;
    }
    fprintf(stdout, "%" PRIu32 " of %" PRIu32 " (%.2f%%)\r\n", complete, total,
            percent);
  } break;
  case GS_FTP_INFO_FILE: {
    uint32_t size = info->u.file.size;
    uint32_t checksum = info->u.file.crc;
    fprintf(stdout, "File size is %" PRIu32 "\r\n", size);
    fprintf(stdout, "Checksum: 0x%" PRIx32 "\r\n", checksum);
  } break;
  case GS_FTP_INFO_CRC: {
    uint32_t remote = info->u.crc.remote;
    uint32_t local = info->u.crc.local;
    fprintf(stdout, "CRC Remote: 0x%" PRIx32 ", Local: 0x%" PRIx32 "\r\n",
            remote, local);
    if (local != remote) {
      fprintf(stdout, "CRC mismatch\r\n");
    }
  } break;
  case GS_FTP_INFO_PROGRESS:
    progress_info(info->u.progress.current_chunk, info->u.progress.total_chunks,
                  info->u.progress.chunk_size, info->user_data);
    break;
  }
}

/* ======================================================================================================
 */

void PER_Testing(u64 n_iterations, u64 *n_packet_success, u64 *n_packet_fail) {
  // make sure it is initialized to zero
  //
  *n_packet_fail = 0;
  *n_packet_success = 0;

  for (u64 it = 0; it < 300; it += 1) {
    // Transceiver ping
    //
    i32 ping_reply = csp_ping(1, 1000, 211, CSP_O_NONE);
    printf("*----------------------    PRE-TESTING   "
           "--------------------------*\n");
    if (ping_reply >= 0) {
      fprintf(stdout,
              "|\x1b[32mPing reply in: %d                                      "
              "   \x1b[0m\n",
              ping_reply);
      *n_packet_success += 1;
    } else {
      fprintf(stdout, "|\x1b[31mNo Ping                                        "
                      "           \x1b[0m\n");
      *n_packet_fail += 1;
    }
    printf("| N Success: %lu                                                   "
           "              \n",
           *n_packet_success);
    printf("| N Fail   : %lu                                                   "
           "              \n",
           *n_packet_fail);
    printf("*------------------------------------------------------------------"
           "*\n");
  }

  *n_packet_fail = 0;
  *n_packet_success = 0;

  for (u64 it = 0; it < n_iterations; it += 1) {
    i32 ping_reply = csp_ping(5, 1000, 200, CSP_O_NONE);
    printf("*------------------------------------------------------------------"
           "*\n");
    if (ping_reply >= 0) {
      fprintf(stdout,
              "|\x1b[32mPing reply in: %d                                      "
              "   \x1b[0m\n",
              ping_reply);
      *n_packet_success += 1;
    } else {
      fprintf(stdout, "|\x1b[31mNo Ping                                        "
                      "           \x1b[0m\n");
      *n_packet_fail += 1;
    }
    printf("| N Success: %lu                                                   "
           "              \n",
           *n_packet_success);
    printf("| N Fail   : %lu                                                   "
           "              \n",
           *n_packet_fail);
    printf("*------------------------------------------------------------------"
           "*\n");
  }
}

/* ======================================================================================================
 */

/**
 * @brief Function in charge of storing the name of the file being currently
 * processed on the ftp_list
 * @param entries   Size in bytes
 * @param entry     Struct containing file/directory info
 * @param user_data Pointer to a data structure containing user defined info
 */
static int FTP_print_list_callback(uint16_t entries,
                                   const gs_ftp_list_entry_t *entry,
                                   void *user_data) {
  ftp_hk_list *ctx = user_data;

  // gs_string_bytesize(entry->path, ctx->names[ctx->current_idx],
  // sizeof(ctx->names[ctx->current_idx]));

  memset(ctx->names[ctx->current_idx], 0, 256);
  memcpy(ctx->names[ctx->current_idx], entry->path, strlen(entry->path));

  fprintf(stdout, "%6d %s%s\r\n", entry->size, ctx->names[ctx->current_idx],
          entry->type ? "/" : "");

  ctx->current_idx += 1;

  return GS_OK;
}

/* ======================================================================================================
 */

/**
 *  @todo Right now microui crashes as the callback is not in another thread, so
 * the windows keep stacking up but there is no render
 *
 *  @brief Info callback implementation. Get Info during ftp upload / download
 * process
 *  @param[in] type Type of info, can be one of FTP_INFO_FILE, FTP_INFO_CRC adn
 * FTP_INFO_STATUS, FTP_INFO_PROGRESS
 *  @param[in] arg1 First info argument
 *  @param[in] arg2 Second info argument
 *  @param[in] arg3 Third info argument
 *  @param[in] user_data User data provided when registering the callback
 */
void FTP_DownloadCallback(const gs_ftp_info_t *type) {
  ftp_user_info_t *UserData = (ftp_user_info_t *)type->user_data;
  switch (type->type) {
  case GS_FTP_INFO_FILE: {
    fprintf(stdout, "[DownloadCallback|FILE] Filse size: %d, CRC: %d\n",
            type->u.file.size, type->u.file.crc);
  } break;
  case GS_FTP_INFO_CRC: {
    fprintf(stdout, "[DownloadCallback|CRC] Remote file: %u, local: %u\n",
            type->u.crc.remote, type->u.crc.local);
  } break;
  case GS_FTP_INFO_COMPLETED: {
    fprintf(
        stdout,
        "[DownloadCallback|COMPLETED] Completed chunks: %d, Total chunks: %d\n",
        type->u.completed.completed_chunks, type->u.completed.total_chunks);
  } break;
  case GS_FTP_INFO_PROGRESS: {
    fprintf(stdout,
            "[DownloadCallback|PROGRESS %f] Current chunk: %d of size %d, of "
            "%d total chunks\n",
            type->u.progress.current_chunk /
                (float)type->u.progress.total_chunks,
            type->u.progress.current_chunk, type->u.progress.chunk_size,
            type->u.progress.total_chunks);
  } break;
  }
}

/* ======================================================================================================
 */

static void FtpToHK(const char *DstFile, Stack_Allocator *Allocator) {
  // Open downloaded file and read the contents
  //
  f_file File = OpenFile(DstFile, RDONLY);
  i32 FileSize = FileLength(&File);
  U8 *Buffer = mmap(NULL, FileSize, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, File.Fd, 0);
  SetFileData(&File, Buffer);
  i32 r_len = FileRead(&File);
  if (r_len == -1) {
    fprintf(stderr, "[ERROR] Could not read file %s\n", DstFile);
    fprintf(stderr, "       %s\n", strerror(errno));
    return;
  }

  //
  // @author Sascha Paez
  // @todo Test shall be done
  //
  U8_String Data = StringNew(Buffer, FileSize, Allocator);
  // has to be in bits
  //
  u64 offset = 0;
  U8_String ParameterData = HK_GetParameterSetStart(&Data, &offset, Allocator);
  while (ParameterData.data != NULL && offset < Data.len * 8) {
    U8_String Instance =
        HK_FileGetInstance(&Data, &ParameterData, &offset, Allocator);
    if (Instance.data == NULL) {
      ParameterData.data = NULL;
      fprintf(stderr, "[HK] No parameter data set\n");
    } else {
      hk_list *InstanceData = stack_push(&AppState.Allocator, hk_list, 1);
      InstanceData->String =
          StringNew(Instance.data, Instance.len, &AppState.Allocator);
      InstanceData->ParamsEnabled =
          StringNew(ParameterData.data, ParameterData.len, &AppState.Allocator);
      DLIST_INSERT(&AppState.HK_Data, InstanceData);
      // @todo Add timestamp from packet
      //
      ucf_ccsds_packet_tm_t packet =
          ucf_ccsds_create_tm_packet(0, 3, 26, 10, Instance.data, Instance.len);
      ssize_t sent_bytes =
          send(AppState.UDP_Conn.cli_sockfd, &packet, packet.length, 0);
    }
    u64 NextOffset = offset;
    U8_String NewParamSet =
        HK_GetParameterSetStart(&Data, &NextOffset, Allocator);
    if (NewParamSet.data != NULL && (NextOffset <= offset + 8)) {
      ParameterData = NewParamSet;
    }
  }

  munmap(Buffer, FileSize);
  CloseFile(&File);
}

/* ======================================================================================================
 */

void DownloadFTP(const char *remote_path, const char *local_path,
                 Stack_Allocator *Allocator) {
  gs_error_t err_return;
  gs_ftp_settings_t ftp_settings;

  err_return = gs_ftp_default_settings(&ftp_settings);

  ftp_settings.chunk_size = 185;
  // OBC csp node address
  //
  ftp_settings.host = 1;

  ftp_settings.port = gs_ftp_get_csp_port(&ftp_settings);
  ftp_settings.timeout = 300000;

  printf("[INFO] ftp host: %d\n", ftp_settings.host);
  printf("[INFO] ftp port: %d\n", ftp_settings.port);
  ftp_user_info_t info_data;
  timestamp_t now;
  timestamp_t finish;
  gs_clock_get_monotonic(&now);
  err_return = gs_ftp_download(&ftp_settings, local_path, remote_path,
                               FTP_DownloadCallback, (void *)&info_data);

  gs_clock_get_monotonic(&finish);
  double sec = timespec_diff(&now, &finish);
  FtpToHK(local_path, Allocator);

  fprintf(stdout, "[%s] -> [%f]\n", local_path, sec);
  if (err_return != GS_OK) {
    fprintf(stderr, "[ERROR] FTP %s could not be made: %d %s\n", remote_path,
            err_return, gs_error_string(err_return));
  }
}

/* ======================================================================================================
 */

internal void *DownloadFTP_Thread(void *arg) {
  u8 buffer[2056];
  Stack_Allocator Allocator;
  stack_init(&Allocator, buffer, 2056);
  ftp_user_info_t *Info = (ftp_user_info_t *)arg;

  DownloadFTP(Info->DstPath, Info->LocalPath, &Allocator);

  return NULL;
}

/* ======================================================================================================
 */

internal gs_param_table_row_t *ParameterTableList(Stack_Allocator *Allocator,
                                                  i32 *n_rows, u8 node,
                                                  gs_param_table_id_t TableID) {
  u8 Node = node;
  gs_param_table_id_t TableId = TableID;
  u16 Checksum = 0;
  u32 Timeout_ms = 40000;
  gs_error_t error;

  // (s.p) First we have to download the table specification, and its checksum.
  // file name set to null because we do not need to store it on disk.
  //
  error = gs_rparam_download_table_spec(&AppState.Instance, NULL, Node, TableId,
                                        Timeout_ms, &Checksum);

  if (error != GS_OK) {
    fprintf(stderr, "[ERROR] Could not download Table ID: %d, from Node: %d",
            TableId, Node);
  }

  // (s.p) Now actually get the information stored in memory for that table
  //
  error = gs_rparam_get_full_table(&AppState.Instance, Node, TableId, Checksum,
                                   Timeout_ms);

  if (error != GS_OK) {
    fprintf(stderr, "[ERROR] Could not get Table ID: %d, from Node: %d",
            TableId, Node);
  }

  // fprintf(stderr, "[INFO] Telemetry info:\n%s\n", Instance.stores);

  gs_param_table_row_t *ParamTable =
      AppState.Instance.rows; // stack_push(Allocator, gs_param_table_row_t,
                              // Instance.row_count);

  for (u32 i = 0; i < AppState.Instance.row_count; i += 1) {
    const gs_param_table_row_t *row = &AppState.Instance.rows[i];
    if (row->array_size <= 1) {
      fprintf(stdout, "type %s with name %s of size %d\n",
              gs_param_type_to_string(row->type), row->name, row->size);
    } else {
      fprintf(stdout, "Array type %s with name %s of size %d with %d values\n",
              gs_param_type_to_string(row->type), row->name, row->size,
              row->array_size);
    }
  }

  *n_rows = AppState.Instance.row_count;

  gs_param_list_to_stream(&AppState.Instance, true, 0, stdout);

  return ParamTable;
}

/* ======================================================================================================
 */

void Upload_FTP(const char *dst_path, const char *src_path, Arena *arena) {
  gs_error_t err_return;
  gs_ftp_settings_t ftp_settings;

  err_return = gs_ftp_default_settings(&ftp_settings);

  // OBC csp node address
  //
  ftp_settings.host = 1;

  ftp_settings.port = gs_ftp_get_csp_port(&ftp_settings);
  ftp_settings.timeout = 300000;

  printf("[INFO] ftp host: %d\n", ftp_settings.host);
  printf("[INFO] ftp port: %d\n", ftp_settings.port);
  ftp_user_info_t info_data;
  timestamp_t now;
  timestamp_t finish;
  gs_clock_get_monotonic(&now);
  err_return = gs_ftp_upload(&ftp_settings, src_path, dst_path,
                             FTP_DownloadCallback, (void *)&info_data);
  gs_clock_get_monotonic(&finish);
  double sec = timespec_diff(&finish, &now);
  fprintf(stdout, "[%s] -> [%f]\n", src_path, sec);
  if (err_return != GS_OK) {
    fprintf(stderr, "[ERROR] FTP %s could not be made: %d %s\n", dst_path,
            err_return, gs_error_string(err_return));
  }
}

internal void FileRemoveAll(const char *path, Stack_Allocator *Alloc) {
  gs_error_t err_return;
  gs_ftp_settings_t ftp_settings;

  err_return = gs_ftp_default_settings(&ftp_settings);

  // OBC csp node address
  //
  ftp_settings.host = 1;

  ftp_settings.port = gs_ftp_get_csp_port(&ftp_settings);
  ftp_settings.timeout = 30000;

  printf("[INFO] ftp host: %d\n", ftp_settings.host);
  printf("[INFO] ftp port: %d\n", ftp_settings.port);

  if (err_return != GS_OK) {
    fprintf(stderr, "[ERROR] Could not specify ftp default settings\n");
  } else {
    fprintf(stdout, "[INFO] Starting file download\n");
    gs_error_t err_return;

    err_return = gs_ftp_list(&ftp_settings, path, FTP_print_list_callback,
                             &AppState.FTP_HK_DATA);

    if (err_return != GS_OK) {
      fprintf(stderr, "[ERROR] FTP list could not be made: %d %s\n", err_return,
              gs_error_string(err_return));
    }

    // *FIXED* fixme(s.p): It seems that repeatedly calling gs_ftp_download
    // creates an `E csp: No more free connections`. It has to be studied why
    // connections do not seem to close after performing a download.
    //
    for (int i = 0; i < AppState.FTP_HK_DATA.current_idx; i += 1) {
      ftp_user_info_t info_data;
      char *src_file = string_concat(path, AppState.FTP_HK_DATA.names[i]);

      err_return = gs_ftp_remove(&ftp_settings, src_file);

      if (err_return != GS_OK) {
        fprintf(stderr, "[ERROR] FTP %s could not be made: %d %s\n", src_file,
                err_return, gs_error_string(err_return));
      } else {
        fprintf(stdout, "[INFO] Removed: %s\n", src_file);
      }
      free(src_file);
    }
  }
}

/* ======================================================================================================
 */

void PER_TestingFTP(u64 n_iterations, u64 *n_packet_success, u64 *n_packet_fail,
                    Arena *arena, bool set_remove) {
  // make sure it is initialized to zero
  //
  *n_packet_fail = 0;
  *n_packet_success = 0;

  printf("[PER Testing FPT] Init\n");

  // The testing will have a life cycle inside the scope of the function
  // so memory is handled through a memory arena
  //
  Temp arena_temp = TempBegin(arena);

  Stack_Allocator TempAllocator;
  void *stack_data = PushArray(arena_temp.arena, u8, mebibyte(12));
  stack_init(&TempAllocator, stack_data, mebibyte(12));

  gs_error_t err_return;
  gs_ftp_settings_t ftp_settings;

  err_return = gs_ftp_default_settings(&ftp_settings);

  // OBC csp node address
  //
  ftp_settings.host = 1;

  ftp_settings.port = gs_ftp_get_csp_port(&ftp_settings);
  ftp_settings.timeout = 300000;

  printf("[INFO] ftp host: %d\n", ftp_settings.host);
  printf("[INFO] ftp port: %d\n", ftp_settings.port);

  if (err_return != GS_OK) {
    fprintf(stderr, "[ERROR] Could not specify ftp default settings\n");
  } else {
    fprintf(stdout, "[INFO] Starting file download\n");
    gs_error_t err_return;

    memset(&AppState.FTP_HK_DATA, 0, sizeof(AppState.FTP_HK_DATA));

    err_return = gs_ftp_list(&ftp_settings, "/flash0/hk/",
                             FTP_print_list_callback, &AppState.FTP_HK_DATA);
    if (err_return != GS_OK) {
      fprintf(stderr, "[ERROR] FTP list could not be made: %d %s\n", err_return,
              gs_error_string(err_return));
    }

    // *FIXED* fixme(s.p): It seems that repeatedly calling gs_ftp_download
    // creates an `E csp: No more free connections`. It has to be studied why
    // connections do not seem to close after performing a download.
    //
    for (int i = 0; i < AppState.FTP_HK_DATA.current_idx; i += 1) {
      ftp_user_info_t info_data;
      // TODO(s.p): Maybe give the user a way to specify which directory to
      // download onto
      //
      char *dst_file =
          string_concat("./HK_OP_MODES/", AppState.FTP_HK_DATA.names[i]);
      char *src_file =
          string_concat("/flash0/hk/", AppState.FTP_HK_DATA.names[i]);

      if (!set_remove) {
        timestamp_t now;
        timestamp_t finish;
        gs_clock_get_monotonic(&now);
        err_return = gs_ftp_download(&ftp_settings, dst_file, src_file,
                                     FTP_DownloadCallback, (void *)&info_data);
        gs_clock_get_monotonic(&finish);

        double sec = timespec_diff(&finish, &now);

        fprintf(stdout, "[%s] -> [%f]\n", src_file, sec);

        if (err_return != GS_OK) {
          fprintf(stderr, "[ERROR] FTP %s could not be made: %d %s\n", dst_file,
                  err_return, gs_error_string(err_return));
        }
      }

      sleep(2);

      // Process the file and send it to yamcs
      FtpToHK(dst_file, &TempAllocator);

      if (set_remove) {
        err_return = gs_ftp_remove(&ftp_settings, src_file);
        if (err_return != GS_OK) {
          fprintf(stderr, "[ERROR] FTP %s could not be made: %d %s\n", src_file,
                  err_return, gs_error_string(err_return));
        } else {
          fprintf(stdout, "[INFO] Removed: %s\n", src_file);
        }
      }
      free(dst_file);
      free(src_file);
    }

    // err_return = gs_ftp_upload(&ftp_settings,
    // "file:///home/polaris/Desktop/prueba.txt", "file:///flash0/prueba1.txt",
    // NULL, NULL );
  }
  TempEnd(arena_temp);
  printf("[PER Testing FPT] End\n");
}

/* ======================================================================================================
 */

/**
 * @brief Main function
 * @param[in] argc Number of arguments passed by the command line
 * @param[in] argv The actual arguments passed by the command line
 * @return int status
 */
int main(int argc, char *argv[]) {
  Arena *main_arena = ArenaAllocDefault();
  u8 *BackBuffer = PushArray(main_arena, u8, mebibyte(126));
  u8 *TempBuffer = PushArray(main_arena, u8, mebibyte(126));

  AppState.MainArena = main_arena;
  stack_init(&AppState.Allocator, BackBuffer, mebibyte(126));
  stack_init(&AppState.TempAllocator, TempBuffer, mebibyte(126));
  DLIST_INIT(&AppState.HK_Data);
  DLIST_INIT(&AppState.DebugTerminal);
  AppState.DebugCommand.Command = StringCreate(512, &AppState.Allocator);
  AppState.CurrentRemotePath = StringCreate(512, &AppState.Allocator);
  AppState.CurrentFileToDownload = StringCreate(512, &AppState.Allocator);
  AppState.SubZMQ_Str = "tcp://0.0.0.0:7000";
  AppState.PubZMQ_Str = "tcp://0.0.0.0:6000";

  user_cmd UserCmd;
  HashTableInit(&UserCmd.OptionTable, &AppState.Allocator, 256, NULL);
  HashTableInit(&UserCmd.OptionFunctions, &AppState.Allocator, 256, NULL);
  HashTableInit(&UserCmd.OptionArgs, &AppState.Allocator, 256, NULL);

  gs_ftp_settings_t ftp_settings;
  char *default_rtable = "1/5 ZMQHUB, 5/0 ZMQHUB";
  char *route_table = PushArray(main_arena, char, 256);
  bool zmq_proxy_enable = false;
  bool PER_test_enable = false;
  bool PER_test_ftp_enable = false;
  bool hpc_test_enable = false;
  bool udp_enable = false;
  bool can_enable = false;
  bool data_rate_ftp_enable = false;
  bool command_line_enable = false;
  bool set_files_remove = false;
  bool help_enable = false;
  bool gui_enable = false;

  memset(route_table, 0, 256);

  HashTableAdd(&UserCmd.OptionTable, (char *)"--zmq", (void *)&zmq_proxy_enable,
               1);
  HashTableAdd(&UserCmd.OptionTable, (char *)"--per-test",
               (void *)&PER_test_enable, 1);
  HashTableAdd(&UserCmd.OptionTable, (char *)"--ftp=download-all",
               (void *)&PER_test_ftp_enable, 1);
  HashTableAdd(&UserCmd.OptionTable, (char *)"--hpc-test",
               (void *)&hpc_test_enable, 1);
  HashTableAdd(&UserCmd.OptionTable, (char *)"--udp-enable",
               (void *)&udp_enable, 1);
  HashTableAdd(&UserCmd.OptionTable, (char *)"--can-enable",
               (void *)&can_enable, 1);
  HashTableAdd(&UserCmd.OptionTable, (char *)"--datarate-ftp",
               (void *)&data_rate_ftp_enable, 1);
  HashTableAdd(&UserCmd.OptionTable, (char *)"--ftp=autoremove",
               (void *)&set_files_remove, 1);
  HashTableAdd(&UserCmd.OptionTable, (char *)"--cmd",
               (void *)&command_line_enable, 1);
  HashTableAdd(&UserCmd.OptionTable, (char *)"--gui", (void *)&gui_enable, 1);
  HashTableAdd(&UserCmd.OptionTable, (char *)"--set-rtable",
               (void *)route_table, 1);
  HashTableAdd(&UserCmd.OptionTable, (char *)"--help", (void *)&help_enable, 1);

  HashTableAdd(&UserCmd.OptionArgs, (char *)"--zmq",
               (char *)"Starts a proxy intercepting zmq traffic", 3);
  HashTableAdd(&UserCmd.OptionArgs, (char *)"--per-test",
               (char *)"Starts a packet error rate test with the OBC", 3);
  HashTableAdd(&UserCmd.OptionArgs, (char *)"--ftp=download-all",
               (char *)"Downloads all HK packets inside flash0", 3);
  HashTableAdd(&UserCmd.OptionArgs, (char *)"--hpc-test",
               (char *)"Enables the HPC test resetting EPS", 3);
  HashTableAdd(&UserCmd.OptionArgs, (char *)"--udp-enable",
               (char *)"Enables UDP ports for connecting TC/TM with YAMCS", 3);
  HashTableAdd(&UserCmd.OptionArgs, (char *)"--can-enable",
               (char *)"Enables CAN interface", 3);
  HashTableAdd(&UserCmd.OptionArgs, (char *)"--datarate-ftp",
               (char *)"Performs a data rate of download using FTP", 3);
  HashTableAdd(&UserCmd.OptionArgs, (char *)"--ftp=autoremove",
               (char *)"Autoremoves packets after downloading that packet. "
                       "Warning, non reversible",
               3);
  HashTableAdd(&UserCmd.OptionArgs, (char *)"--cmd",
               (char *)"Enables a command line for some ftp functionality", 3);
  HashTableAdd(&UserCmd.OptionArgs, (char *)"--gui",
               (char *)"Enables a Graphical UI for some ftp functionality", 3);
  HashTableAdd(
      &UserCmd.OptionArgs, (char *)"--set-rtable",
      (char *)"Takes a route table and uses it for csp interfacing, if not "
              "set, a default one is used\n \te.g. --set-rtable \"1/5 CAN, 5/0 "
              "ZMQHUB, 2/0 KISS, 3/0 UART\" ",
      3);
  HashTableAdd(&UserCmd.OptionArgs, (char *)"--help",
               (char *)"Shows this prompt", 3);

  for (i32 a_it = 1; a_it < argc; a_it += 1) {
    entry *Entry = HashTableFindPointer(&UserCmd.OptionTable, argv[a_it], 1);
    if (Entry == NULL) {
      fprintf(stderr, "[ERROR] %s not set, see --help\n", argv[a_it]);
      continue;
    }
    if (Entry->Next->Value == (void *)route_table) {
      fprintf(stdout, "%s\n", argv[a_it + 1]);
      if ((a_it + 1) == argc) {
        fprintf(stderr, "[ERROR] --set-rtable used but no route table "
                        "provided, see --help\n");
        return -1;
      }
      memcpy(route_table, argv[a_it + 1], UCF_Strlen(argv[a_it + 1]));
      a_it += 1;
    } else {
      *(bool *)Entry->Next->Value = true;
    }
  }

  // Print help
  //
  {
    entry *Entry =
        HashTableFindPointer(&UserCmd.OptionTable, (char *)"--help", 1);
    if (*(bool *)Entry->Next->Value == true) {
      printf("Options available: \n");
      for (U64 i = 0; i < 256; i += 1) {
        entry it = UserCmd.OptionTable.Entries[i];
        entry it_info = UserCmd.OptionArgs.Entries[i];
        if (it.HashId != 0 && it_info.HashId != 0) {
          printf("  %-*s%s\n", 24, it.Id, it_info.Next->Value);
        }
      }

      return 0;
    }
  }

  // Start zmq job
  //
  if (false) {
    thread_start_zmq_job(AppState.SubZMQ_Str, AppState.PubZMQ_Str,
                         zmq_proxy_enable);
  }

  // Initialize CSP
  {
    gs_csp_conf_t conf;
    gs_csp_conf_get_defaults_server(&conf);

    conf.hostname = "ucf-middleware-client";
    conf.model = "UCF_Middleware_Client";
    conf.revision = "0.1";

    csp_rdp_set_opt(12, 30000, 16000, 1, 8000, 1);

    conf.address = 10;

    gs_csp_init(&conf);

    // Start CSP router task
    gs_csp_router_task_start(512, GS_THREAD_PRIORITY_HIGH);

    csp_iflist_add(&csp_if_zmqhub);

    if (can_enable) {
      csp_iface_t *can_iface = csp_can_socketcan_init("can0", 1000000, 0);
      csp_iflist_add(can_iface);
    }

    if (route_table[0] == '\0') {
      memcpy(route_table, default_rtable, UCF_Strlen(default_rtable));
    }
    // gs_error_t error = gs_csp_can_init(0, 10, CSP_CAN_MTU, "can0",
    // &can_iface); csp_zmqhub_init(255, "0.0.0.0");

    // Configure routes - add default routes or use command line option
    gs_csp_rtable_load(route_table, true, true);

    csp_rtable_print();

    csp_zmqhub_init_w_endpoints(255, AppState.PubZMQ_Str, AppState.SubZMQ_Str);

    csp_iflist_print();
    // csp_service_dispatcher();
  }

  if (udp_enable) {
    AppState.UDP_Conn = udp_socket_init();
    pthread_t task_serv;
    pthread_t task_cli;
    pthread_create(&task_serv, NULL, udp_transactions_server, NULL);
    pthread_create(&task_cli, NULL, udp_transactions_client, NULL);
  }

  if (data_rate_ftp_enable && argc >= 4) {
    DownloadFTP(argv[2], argv[3], &AppState.TempAllocator);
  } else if (data_rate_ftp_enable && argc < 4) {
    fprintf(stderr, "DATA_RATE_FTP enabled but no source and destination file "
                    "provided:\n\t ./ucf_gs_csp DATA_RATE_FTP "
                    "/path/to/destfile /path/to/srcfile\n");
  }

  if (PER_test_enable) {
    u64 success = 0;
    u64 fail = 0;
    PER_Testing(10000, &success, &fail);

    f32 ratio = (f32)fail / (f32)success;

    printf("*------------------------------------------------------------------"
           "*\n");
    if (ratio > 26) {
      printf("\x1b[31m|[PER TEST] Fail ratio: %f percentage of packets lost    "
             "          |\x1b[0m\n",
             ratio);
    } else {
      printf("\x1b[32m|[PER TEST] Fail ratio: %f percentage of packets lost    "
             "          |\x1b[0m\n",
             ratio);
    }
    printf("|                                                                  "
           "|\n");
    printf("|By req. maximum loss of packets should be\x1b[34m 26\x1b[0m for "
           "every 10.000     |\n");
    printf("*------------------------------------------------------------------"
           "*\n");
  }

  if (PER_test_ftp_enable) {
    u64 success = 0;
    u64 fail = 0;
    PER_TestingFTP(10000, &success, &fail, main_arena, set_files_remove);

    f32 ratio = (f32)fail / (f32)success;

    printf("*------------------------------------------------------------------"
           "*\n");
    if (ratio > 26) {
      printf("\x1b[31m|[PER TEST] Fail ratio: %f percentage of packets lost    "
             "          |\x1b[0m\n",
             ratio);
    } else {
      printf("\x1b[32m|[PER TEST] Fail ratio: %f percentage of packets lost    "
             "          |\x1b[0m\n",
             ratio);
    }
    printf("|                                                                  "
           "|\n");
    printf("|By req. maximum loss of packets should be\x1b[34m 26\x1b[0m for "
           "every 10.000     |\n");
    printf("*------------------------------------------------------------------"
           "*\n");
  }

  if (hpc_test_enable) {
    /**
     * (NOTE)(s.p): This ping is done due to ax100 seamingly not being able to
     * process the first command you send. Could be also a problem with how
     * GNU-Radio handles buffers. Modulating the data with some left-over
     * garbage.
     */
    i32 ping_reply = csp_ping(5, 700, 1, CSP_O_NONE);

    csp_packet_t *packet = csp_buffer_get(sizeof(csp_packet_t) + 1);
    packet->data[0] = 0xA6;
    packet->length = 1;
    int status = csp_sendto(0, 0, 0x1E, 0x2A, 0, packet, 1000);

    if (status == -1) {
      fprintf(stderr, "Error with packet %d\n", status);
      csp_buffer_free(packet);
    } else {
      fprintf(stderr, "[INFO] HPC Packet sent with status %d\n", status);
    }
  }

  if (gui_enable) {
    vulkan_base VkBase = VulkanInit();
    
    Arena *main_arena = VkBase.Arena;
    Temp arena_temp = TempBegin(main_arena);
    
    Stack_Allocator Allocator;
    Stack_Allocator TempAllocator;
    u8 *Buffer = PushArray(main_arena, u8, gigabyte(1));
    u8 *TempBuffer = PushArray(main_arena, u8, mebibyte(256));
    stack_init(&Allocator, Buffer, gigabyte(1));
    stack_init(&TempAllocator, TempBuffer, mebibyte(256));
    
    ui_context *UI_Context = PushArray(main_arena, ui_context, 1);

    // @todo: Change how to add fonts this is highly inconvenient
    //
    u8 *BitmapArray = stack_push(&Allocator, u8, 2100 * 1200);
    FontCache DefaultFont =
        F_BuildFont(22, 2100, 60, BitmapArray, "./data/RobotoMono.ttf");
    FontCache TitleFont = F_BuildFont(30, 2100, 60, BitmapArray + (2100 * 60),
                                      "./data/LiterationMono.ttf");
    FontCache TitleFont2 = F_BuildFont(30, 2100, 60, BitmapArray + (2100 * 120),
                                       "./data/TinosNerdFontPropo.ttf");
    FontCache BoldFont = F_BuildFont(22, 2100, 60, BitmapArray + (2100 * 180),
                                     "./data/LiterationMonoBold.ttf");
    FontCache ItalicFont = F_BuildFont(22, 2100, 60, BitmapArray + (2100 * 240),
                                       "./data/LiterationMonoItalic.ttf");
    DefaultFont.BitmapOffset = Vec2Zero();
    TitleFont.BitmapOffset = (vec2){0, 60};
    TitleFont2.BitmapOffset = (vec2){0, 120};
    BoldFont.BitmapOffset = (vec2){0, 180};
    ItalicFont.BitmapOffset = (vec2){0, 240};

    UI_Graphics gfx = UI_GraphicsInit(&VkBase, BitmapArray, 2100, 1200);

    UI_Init(UI_Context, &gfx, &Allocator, &TempAllocator);


    U8_String RemotePath = StringCreate(256, UI_Context->Allocator);
    StringAppend(&RemotePath, "/flash0/");
    int listViewScrollIndex = 0;
    int listViewActive = -1;
    char RemovePath[256] = {};
    char DownloadRemotePath[256] = {};
    char DownloadLocalPath[256] = {};
    char UploadLocalPath[256] = {};
    char UploadRemotePath[256] = {};
    bool EditRemotePath = false;
    bool EditRemovePath = false;
    bool EditDownloadRemotePath = false;
    bool EditLocalRemotePath = false;
    bool EditCompressPath = false;
    memcpy(RemovePath, "/flash0", 8);
    memcpy(DownloadRemotePath, "/flash0", 8);
    memcpy(DownloadLocalPath, "/home/polaris/Downloads", 23);
    memcpy(UploadLocalPath, "/home/ucanfly/", 17);
    memcpy(UploadRemotePath, "/flash0", 8);

    rgba bd = HexToRGBA(0x2D2D2DFF);
    rgba ib = HexToRGBA(0x2F2F2FFF);
    rgba fd = RgbaNew(240, 236, 218, 255);
    rgba fg = RgbaNew(21, 39, 64, 255);
    rgba bg2 = HexToRGBA(0xA13535FF);
    rgba bc = HexToRGBA(0x3B764CFF);

    object_theme DefaultTheme = {.Border = RgbaToNorm(bc),
                                 .Background = RgbaToNorm(bd),
                                 .Foreground = RgbaToNorm(fd),
                                 .Radius = 6,
                                 .BorderThickness = 2,
                                 .Font = &DefaultFont};

    object_theme TitleTheme = DefaultTheme;
    TitleTheme.Font = &TitleFont;

    object_theme ButtonTheme = {.Border = RgbaToNorm(bc),
                                .Background = RgbaToNorm(bd),
                                .Foreground = RgbaToNorm(fd),
                                .Radius = 6,
                                .BorderThickness = 0,
                                .Font = &BoldFont};

    object_theme PanelTheme = {.Border = RgbaToNorm(bc),
                               .Background = RgbaToNorm(bg2),
                               .Foreground = RgbaToNorm(fd),
                               .Radius = 6,
                               .BorderThickness = 0,
                               .Font = &BoldFont};

    object_theme InputTheme = {.Border = RgbaToNorm(bc),
                               .Background = RgbaToNorm(ib),
                               .Foreground = RgbaToNorm(fd),
                               .Radius = 2,
                               .BorderThickness = 2,
                               .Font = &DefaultFont};

    object_theme LabelTheme = {.Border = RgbaToNorm(bc),
                               .Background = RgbaToNorm(bd),
                               .Foreground = RgbaToNorm(fd),
                               .Radius = 0,
                               .BorderThickness = 0,
                               .Font = &ItalicFont};

    object_theme ScrollbarTheme = {.Border = RgbaToNorm(bc),
                                   .Background = RgbaToNorm(fd),
                                   .Foreground = RgbaToNorm(bc),
                                   .Radius = 6,
                                   .BorderThickness = 2,
                                   .Font = NULL};

    UI_Context->DefaultTheme = (ui_theme){.Window = TitleTheme,
                                          .Button = ButtonTheme,
                                          .Panel = PanelTheme,
                                          .Input = InputTheme,
                                          .Label = LabelTheme,
                                          .Scrollbar = ScrollbarTheme};

    XEvent ev;
    bool running = true;
    struct timespec ts2, ts1;

    double posix_dur = 0;
    for (; running;) {
      clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts1);
      u32 window_width = gfx.base->Swapchain.Extent.width;
      u32 window_height = gfx.base->Swapchain.Extent.height;

      UI_Begin(UI_Context);

      ui_input Input = UI_LastEvent(UI_Context);

      if (Input == StopUI) {
        fprintf(stderr, "[INFO] Finished Application\n");
        running = false;
        break;
      }
      if (Input & F1) {
        AppState.DebugMode = !AppState.DebugMode;
      }

      if (AppState.DebugMode) {
        UI_WindowBegin(
            UI_Context,
            (rect_2d){{5, 5}, {window_width - 10, (window_height / 2) - 10}},
            "Debug Terminal",
            UI_AlignCenter | UI_Select | UI_Drag | UI_Resize | UI_NoTitle);
        {
          UI_PushNextLayoutPadding(UI_Context, (vec2){20, 0});
          for (debug_terminal *p = AppState.DebugTerminal.Next;
               p != &AppState.DebugTerminal && p != NULL; p = p->Next) {
            char buf[126];
            snprintf(buf, 126, "%s_%p", p->data, p);
            UI_LabelWithKey(UI_Context, buf, p->data.data);
          }

          if (UI_TextBox(UI_Context, "Debug Command") & Return) {
            debug_terminal *p =
                stack_push(&AppState.Allocator, debug_terminal, 1);
            U8_String *NewCommand =
                UI_GetTextFromBox(UI_Context, "Debug Command");
            p->data = StringNew(NewCommand->data, NewCommand->idx,
                                &AppState.Allocator);
            DLIST_INSERT(&AppState.DebugTerminal, p);

            U8_String Command = SplitFirst(NewCommand, ' ');
            if (UCF_Streqn(Command.data, "Simulate", Command.idx)) {
              u64 N_Commands = GetCountOfChar(NewCommand, ' ');
              N_Commands += 1;
              U8_String *Arguments =
                  stack_push(&AppState.TempAllocator, U8_String, N_Commands);
              SplitMultiple(Arguments, N_Commands, NewCommand, ' ');

              if (N_Commands == 3 && UCF_Streqn(Arguments[1].data, "HK", 2)) {
                U8_String Data =
                    StringCreate(Arguments[2].idx + 1, &AppState.TempAllocator);
                StringCpyStr(&Data, &Arguments[2]);
                FtpToHK(Data.data, &AppState.TempAllocator);
              }
            }
          }
        }
        UI_WindowEnd(UI_Context);
      }

      UI_WindowBegin(
          UI_Context,
          (rect_2d){{5, 5}, {(window_width / 3) - 10, window_height - 10}},
          "UCAnFly Panel",
          UI_AlignCenter | UI_Select | UI_Drag | UI_Resize | UI_NoTitle);
      {
        UI_PushNextLayoutPadding(UI_Context, (vec2){20, 10});
        if (UI_BeginTreeNode(UI_Context, "Command Panel") & ActiveObject) {
          ui_window *CurrentWindow = StackGetFront(&UI_Context->Windows);
          const int Columns[] = {CurrentWindow->WindowRect.Size.x / 2,
                                 CurrentWindow->WindowRect.Size.x / 2};
          UI_PushNextLayoutColumn(UI_Context, 3, Columns);
          UI_BeginColumn(UI_Context);
          {
            UI_Label(UI_Context, "File Transfer List");
            if (UI_TextBox(UI_Context, "Remote Path: (/flash0/)") & Return) {
              memset(&AppState.FTP_HK_DATA, 0, sizeof(AppState.FTP_HK_DATA));
              U8_String *Remote =
                  UI_GetTextFromBox(UI_Context, "Remote Path: (/flash0/)");
              if (Remote->data[Remote->idx - 1] != '/') {
                StringAppend(Remote, "/");
              }
              StringCpyStr(&RemotePath, Remote);

              StringCpyStr(&AppState.CurrentRemotePath, Remote);

              gs_error_t err_return;
              gs_ftp_settings_t ftp_settings;

              err_return = gs_ftp_default_settings(&ftp_settings);
              ftp_settings.chunk_size = 251;
              // OBC csp node address
              //
              ftp_settings.host = 1;

              ftp_settings.port = gs_ftp_get_csp_port(&ftp_settings);
              ftp_settings.timeout = 30000;

              printf("[INFO] ftp host: %d\n", ftp_settings.host);
              printf("[INFO] ftp port: %d\n", ftp_settings.port);

              if (err_return != GS_OK) {
                fprintf(stderr,
                        "[ERROR] Could not specify ftp default settings\n");
              } else {
                fprintf(stdout, "[INFO] Starting file download\n");
                gs_error_t err_return;

                err_return =
                    gs_ftp_list(&ftp_settings, RemotePath.data,
                                FTP_print_list_callback, &AppState.FTP_HK_DATA);
              }
            }
          }
          UI_EndColumn(UI_Context);
          if (AppState.FTP_HK_DATA.current_idx > 0) {
            if (UI_BeginTreeNode(UI_Context, "Response") & ActiveObject) {
              UI_BeginScrollbarView(UI_Context);
              for (u32 _it = 0; _it < AppState.FTP_HK_DATA.current_idx;
                    _it += 1) {
                UI_Label(UI_Context, AppState.FTP_HK_DATA.names[_it]);
              }
              UI_EndScrollbarView(UI_Context);
            }
            UI_EndTreeNode(UI_Context);
          }
        }
        UI_EndTreeNode(UI_Context);

        if (UI_BeginTreeNode(UI_Context, "Download List") & ActiveObject) {
          UI_BeginScrollbarViewEx(UI_Context, (vec2){10, 300});
          for (u32 _it = 0; _it < AppState.FTP_HK_DATA.current_idx; _it += 1) {
            if (UI_Button(UI_Context, AppState.FTP_HK_DATA.names[_it]) &
                LeftClickPress) {
              StringEraseUntil(&AppState.CurrentFileToDownload, 0);
              StringCpy(&AppState.CurrentFileToDownload,
                        &AppState.FTP_HK_DATA.names[_it]);
            }
          }
          UI_EndScrollbarView(UI_Context);
          if (UI_TextBox(UI_Context, "/path/to/folder/") & Return) {
          }

          if (UI_Button(UI_Context, "Submit Download") & LeftClickPress) {
            U8_String *Local =
                UI_GetTextFromBox(UI_Context, "/path/to/folder/");
            if (Local->data[Local->idx - 1] != '/') {
              StringAppend(Local, "/");
            }
            StringAppendStr(Local, &AppState.CurrentFileToDownload);
            const char *LocalPath = Local->data;

            char Path[256] = {0};
            
            //StringAppendStr(&RemotePath, &AppState.CurrentFileToDownload);
            memcpy(Path, RemotePath.data, RemotePath.idx);
            memcpy(Path + RemotePath.idx, AppState.CurrentFileToDownload.data, AppState.CurrentFileToDownload.idx);

            DownloadFTP(Path, LocalPath, &AppState.TempAllocator);
          }
        }
        UI_EndTreeNode(UI_Context);

        if (UI_BeginTreeNode(UI_Context, "Param Manager") & ActiveObject) {
          ui_window *CurrentWindow = StackGetFront(&UI_Context->Windows);
          const int Columns[] = {CurrentWindow->WindowRect.Size.x / 3,
                                 CurrentWindow->WindowRect.Size.x / 3,
                                 CurrentWindow->WindowRect.Size.x / 3};
          UI_PushNextLayoutColumn(UI_Context, 3, Columns);
          local_persist gs_param_table_row_t *TableData = 0;
          local_persist i32 HK_Rows = 0;
          {
            UI_PushNextLayoutOption(UI_Context, UI_AlignCenter);
            UI_Label(UI_Context, "-- On-Board Computer --");
            UI_PushNextLayoutDisableOption(UI_Context, UI_AlignCenter);
          }
          UI_BeginColumn(UI_Context);
          {
            UI_Button(UI_Context, "OBC Table");
            UI_TextBox(UI_Context, "Table Number (OBC)");
            if (UI_Button(UI_Context, "Fetch OBC Table") & LeftClickPress) {
              U8_String *TableNumber =
                  UI_GetTextFromBox(UI_Context, "Table Number (OBC)");
              TableData =
                  ParameterTableList(UI_Context->TempAllocator, &HK_Rows, 1,
                                     atoi(TableNumber->data));
            }
          }
          UI_EndColumn(UI_Context);

          {
            UI_PushNextLayoutOption(UI_Context, UI_AlignCenter);
            UI_Label(UI_Context, "-- Transceiver --");
            UI_PushNextLayoutDisableOption(UI_Context, UI_AlignCenter);
          }

          UI_BeginColumn(UI_Context);
          {
            UI_Button(UI_Context, "Transceiver Table");
            UI_TextBox(UI_Context, "Table Number (Transceiver)");
            if (UI_Button(UI_Context, "Fetch Transcevier Table") &
                LeftClickPress) {
              U8_String *TableNumber =
                  UI_GetTextFromBox(UI_Context, "Table Number (Transceiver)");
              TableData =
                  ParameterTableList(UI_Context->TempAllocator, &HK_Rows, 5,
                                     atoi(TableNumber->data));
            }
          }
          UI_EndColumn(UI_Context);

          UI_BeginColumn(UI_Context);
          UI_Label(UI_Context, "Type");
          UI_Label(UI_Context, "Name");
          UI_Label(UI_Context, "Size");
          UI_EndColumn(UI_Context);
          UI_BeginScrollbarViewEx(UI_Context, (vec2){10, 300});
          for (u32 _it = 0; _it < HK_Rows; _it += 1) {
            gs_param_table_row_t *row = &TableData[_it];
            const char *type = gs_param_type_to_string(row->type);
            i32 buf_len = UCF_Strlen(type);
            char RowSize[64];
            snprintf(RowSize, 64, "%3" PRIu8 "", row->size);
            char KeyType[64];
            snprintf(KeyType, 64, "key_%s_%s", row->name, type);
            char KeySize[64];
            snprintf(KeySize, 64, "key_%s_%s", row->name, RowSize);
            UI_BeginColumn(UI_Context);
            UI_LabelWithKey(UI_Context, KeyType, type);
            UI_Label(UI_Context, row->name);
            UI_LabelWithKey(UI_Context, KeySize, RowSize);
            UI_EndColumn(UI_Context);
          }
          UI_EndScrollbarView(UI_Context);
        }
        UI_EndTreeNode(UI_Context);

        {
          ui_window *CurrentWindow = StackGetFront(&UI_Context->Windows);
          const int Columns[] = {CurrentWindow->WindowRect.Size.x / 2,
                                 CurrentWindow->WindowRect.Size.x / 2};
          UI_PushNextLayoutColumn(UI_Context, 2, Columns);
          UI_BeginColumn(UI_Context);
          {
            UI_Label(UI_Context, "Recursive Remove");

            if (UI_TextBox(UI_Context, "/flash0/") & Return) {
              U8_String* path = UI_GetTextFromBox(UI_Context, "/flash0/");
              if (path->data[path->idx - 1] != '/') {
                StringAppend(path, "/");
              }
              FileRemoveAll(path->data, UI_Context->TempAllocator);
            }
          }
          UI_EndColumn(UI_Context);
        }
        {
          ui_window *CurrentWindow = StackGetFront(&UI_Context->Windows);
          const int Columns[] = {CurrentWindow->WindowRect.Size.x / 2,
                                 CurrentWindow->WindowRect.Size.x / 2};
          UI_PushNextLayoutColumn(UI_Context, 2, Columns);
          UI_BeginColumn(UI_Context);
          {
            UI_Label(UI_Context, "Recursive Download");

            if (UI_Button(UI_Context, "Downoad All HK Data") & LeftClickPress) {
              u64 n_packet_success = 0;
              u64 n_packet_fail    = 0;
              bool set_remove      = false;
              PER_TestingFTP(0, &n_packet_success, &n_packet_fail, arena_temp.arena, set_remove);
            }
          }
          UI_EndColumn(UI_Context);
        }
      }
      UI_WindowEnd(UI_Context);

      UI_WindowBegin(UI_Context,
                     (rect_2d){{(window_width / 3) + 5, 5},
                               {(window_width - window_width / 3) - 10,
                                window_height - 10}},
                     "UCAnFly HK",
                     UI_AlignCenter | UI_Select | UI_Drag | UI_Resize);
      {
        UI_PushNextLayoutPadding(UI_Context, (vec2){20, 0});
        ui_window *w = StackGetFront(&UI_Context->Windows);
        int ColSize = w->WindowRect.Size.x / 5;
        const int Columns[] = {ColSize, ColSize, ColSize, ColSize, ColSize};
        UI_PushNextLayoutColumn(UI_Context, 5, Columns);
        UI_PushNextLayoutOption(UI_Context, UI_AlignCenter);
        UI_BeginColumn(UI_Context);
        UI_Label(UI_Context, "ID");
        UI_Label(UI_Context, "Timestamp");
        UI_Label(UI_Context, "Name");
        UI_Label(UI_Context, "Value");
        UI_Label(UI_Context, "Disabled");
        UI_EndColumn(UI_Context);
        UI_BeginScrollbarView(UI_Context);
        for (hk_list *p = AppState.HK_Data.Next;
             p != NULL && p != &AppState.HK_Data; p = p->Next) {
          U8_String Data = p->String;
          u64 TimeSize = sizeof(u64);
          u8 Hk_ID = 0;
          u64 Time = 0;
          memcpy(&Time, Data.data, TimeSize);
          memcpy(&Hk_ID, Data.data + TimeSize, sizeof(u8));

          u8 buf[64] = {0};
          u8 TimeBuf[126] = {0};
          u8 StructName[256] = {0};
          u8 Id_Key[126] = {0};
          u8 Time_Key[126] = {0};
          format_unix_nanotime(TimeBuf, 126, Time);

          sprintf(buf, "%" PRIu8 "", Hk_ID);

          snprintf(Id_Key, 126, "%" PRIu8 "_%p", Hk_ID, p);
          snprintf(Time_Key, 126, "%lu_%p", Time, p);

          if (Hk_ID == UCF_STRUCT_ADCS_SAFE_ID ||
              Hk_ID == UCF_STRUCT_ADCS_NOM_ID ||
              Hk_ID == UCF_STRUCT_ADCS_OPER_ID ||
              Hk_ID == UCF_STRUCT_ADCS_ACT_ID) 
          {
            ui_window *w = StackGetFront(&UI_Context->Windows);
            ucf_adcs_hk_t data;
            memcpy(&data, Data.data, Data.idx);
            char ValueBuf[64], DisabledBuf[64];

            UI_PushNextLayoutColumn(UI_Context, ArrayCount(Columns), Columns);
            snprintf(StructName, 256, "ADCS_%p", p);
            UI_BeginColumn(UI_Context);
            UI_LabelWithKey(UI_Context, Id_Key, buf);
            UI_LabelWithKey(UI_Context, Time_Key, TimeBuf);
            UI_LabelWithKey(UI_Context, StructName, "ADCS");
            snprintf(ValueBuf,    64, "-_%p", p); UI_LabelWithKey(UI_Context, ValueBuf, "-");
            snprintf(DisabledBuf, 64, "-_%p", p); UI_LabelWithKey(UI_Context, DisabledBuf, "-");
            UI_EndColumn(UI_Context);
          } else if (Hk_ID == UCF_STRUCT_EPS_PBU_SAFE_ID ||
                     Hk_ID == UCF_STRUCT_EPS_PBU_NOM_ID ||
                     Hk_ID == UCF_STRUCT_EPS_PBU_OPER_ID ||
                     Hk_ID == UCF_STRUCT_EPS_PBU_ACT_ID) {
            ui_window *w = StackGetFront(&UI_Context->Windows);
            ucf_eps_pbu_hk_t data;
            memcpy(&data, Data.data, Data.idx);
            char ValueBuf[64], DisabledBuf[64];

            UI_PushNextLayoutColumn(UI_Context, ArrayCount(Columns), Columns);
            snprintf(StructName, 256, "EPS_PBU_%p", p);
            UI_BeginColumn(UI_Context);
            UI_LabelWithKey(UI_Context, Id_Key, buf);
            UI_LabelWithKey(UI_Context, Time_Key, TimeBuf);
            UI_LabelWithKey(UI_Context, StructName, "EPS_PBU");
            snprintf(ValueBuf,    64, "-_%p", p); UI_LabelWithKey(UI_Context, ValueBuf, "-");
            snprintf(DisabledBuf, 64, "-_%p", p); UI_LabelWithKey(UI_Context, DisabledBuf, "-");
            UI_EndColumn(UI_Context);
          } else if (Hk_ID == UCF_STRUCT_EPS_PDU_SAFE_ID ||
                     Hk_ID == UCF_STRUCT_EPS_PDU_NOM_ID ||
                     Hk_ID == UCF_STRUCT_EPS_PDU_OPER_ID ||
                     Hk_ID == UCF_STRUCT_EPS_PDU_ACT_ID) {
            ui_window *w = StackGetFront(&UI_Context->Windows);
            ucf_eps_pdu_hk_t data;
            memcpy(&data, Data.data, Data.idx);
            char ValueBuf[64], DisabledBuf[64];

            UI_PushNextLayoutColumn(UI_Context, ArrayCount(Columns), Columns);
            snprintf(StructName, 256, "EPS_PDU_%p", p);
            UI_BeginColumn(UI_Context);
            UI_LabelWithKey(UI_Context, Id_Key, buf);
            UI_LabelWithKey(UI_Context, Time_Key, TimeBuf);
            UI_LabelWithKey(UI_Context, StructName, "EPS_PDU");
            snprintf(ValueBuf,    64, "-_%p", p); UI_LabelWithKey(UI_Context, ValueBuf, "-");
            snprintf(DisabledBuf, 64, "-_%p", p); UI_LabelWithKey(UI_Context, DisabledBuf, "-");
            UI_EndColumn(UI_Context);
          } else if (Hk_ID == UCF_STRUCT_EPS_PIU_SAFE_ID ||
                     Hk_ID == UCF_STRUCT_EPS_PIU_NOM_ID ||
                     Hk_ID == UCF_STRUCT_EPS_PIU_OPER_ID ||
                     Hk_ID == UCF_STRUCT_EPS_PIU_ACT_ID) {
            ui_window *w = StackGetFront(&UI_Context->Windows);
            ucf_eps_piu_hk_t data;
            memcpy(&data, Data.data, Data.idx);
            char ValueBuf[64], DisabledBuf[64];

            UI_PushNextLayoutColumn(UI_Context, ArrayCount(Columns), Columns);
            snprintf(StructName, 256, "EPS_PIU_%p", p);
            UI_BeginColumn(UI_Context);
            UI_LabelWithKey(UI_Context, Id_Key, buf);
            UI_LabelWithKey(UI_Context, Time_Key, TimeBuf);
            UI_LabelWithKey(UI_Context, StructName, "EPS_PIU");
            snprintf(ValueBuf,    64, "-_%p", p); UI_LabelWithKey(UI_Context, ValueBuf, "-");
            snprintf(DisabledBuf, 64, "-_%p", p); UI_LabelWithKey(UI_Context, DisabledBuf, "-");
            UI_EndColumn(UI_Context);
          } else if (Hk_ID == UCF_STRUCT_EPS_SYS_SAFE_ID ||
                     Hk_ID == UCF_STRUCT_EPS_SYS_NOM_ID ||
                     Hk_ID == UCF_STRUCT_EPS_SYS_OPER_ID ||
                     Hk_ID == UCF_STRUCT_EPS_SYS_ACT_ID) {
            ui_window *w = StackGetFront(&UI_Context->Windows);
            ucf_eps_system_status_hk_t data;
            char ValueBuf[64], DisabledBuf[64];
            UI_PushNextLayoutColumn(UI_Context, ArrayCount(Columns), Columns);
            memcpy(&data, Data.data, Data.idx);
            snprintf(StructName, 256, "EPS_SYS_%p", p);
            UI_BeginColumn(UI_Context);
            UI_LabelWithKey(UI_Context, Id_Key, buf);
            UI_LabelWithKey(UI_Context, Time_Key, TimeBuf);
            UI_LabelWithKey(UI_Context, StructName, "EPS_SYS");
            snprintf(ValueBuf,    64, "-_%p", p); UI_LabelWithKey(UI_Context, ValueBuf, "-");
            snprintf(DisabledBuf, 64, "-_%p", p); UI_LabelWithKey(UI_Context, DisabledBuf, "-");
            UI_EndColumn(UI_Context);
            #if 0
            for(i32 _it = 2; _it < ArrayCount(EpsSysHkAttributes); _it += 1) {
              UI_BeginTreeNode(UI_Context, "Inspect Data");
              UI_BeginColumn(UI_Context);
              UI_LabelWithKey(UI_Context, Id_Key, buf);
              UI_LabelWithKey(UI_Context, Time_Key, TimeBuf);
              char* AttrName = EpsSysHkAttributes[_it];
              u32 offset = EPS_System_Status_ListOfSizes[_it] / 8;
              snprintf(StructName, 256, "%s_%p", AttrName, p);                  UI_LabelWithKey(UI_Context, StructName, AttrName);
              snprintf(ValueBuf,    64, "%d_%p", *(&data + offset), p);         UI_LabelWithKey(UI_Context, ValueBuf, ValueBuf);
              snprintf(DisabledBuf, 64, "%c_%p", p->ParamsEnabled.data[_it],p); UI_LabelWithKey(UI_Context, DisabledBuf, DisabledBuf);
              UI_EndColumn(UI_Context);
              UI_EndTreeNode(UI_Context);
            }
            #endif
          } else if (Hk_ID == UCF_STRUCT_TRA_SAFE_ID ||
                     Hk_ID == UCF_STRUCT_TRA_NOM_ID ||
                     Hk_ID == UCF_STRUCT_TRA_OPER_ID ||
                     Hk_ID == UCF_STRUCT_TRA_ACT_ID) {
            ui_window *w = StackGetFront(&UI_Context->Windows);
            ucf_transceiver_hk_t data;
            memcpy(&data, Data.data, Data.idx);
            char ValueBuf[64], DisabledBuf[64];

            UI_PushNextLayoutColumn(UI_Context, ArrayCount(Columns), Columns);
            snprintf(StructName, 256, "TRANSCEIVER_%p", p);
            UI_BeginColumn(UI_Context);
            UI_LabelWithKey(UI_Context, Id_Key, buf);
            UI_LabelWithKey(UI_Context, Time_Key, TimeBuf);
            UI_LabelWithKey(UI_Context, StructName, "TRANSCEIVER");
            snprintf(ValueBuf,    64, "-_%p", p); UI_LabelWithKey(UI_Context, ValueBuf, "-");
            snprintf(DisabledBuf, 64, "-_%p", p); UI_LabelWithKey(UI_Context, DisabledBuf, "-");
            UI_EndColumn(UI_Context);
          }
        }
        UI_EndScrollbarView(UI_Context);
      }
      UI_WindowEnd(UI_Context);

      UI_End(UI_Context);

      bool do_resize = PrepareFrame(gfx.base);
      if (do_resize) {
        UI_ExternalRecreateSwapchain(&gfx);
      } else {
        gfx.UniformData.ScreenWidth = gfx.base->Swapchain.Extent.width;
        gfx.UniformData.ScreenHeight = gfx.base->Swapchain.Extent.height;
        gfx.UniformData.TextureWidth = gfx.UI_TextureImage.Width;
        gfx.UniformData.TextureHeight = gfx.UI_TextureImage.Height;
        VkMappedMemoryRange flushRange = {0};
        flushRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        // Get the VkDeviceMemory from your buffer's allocation info
        flushRange.memory = gfx.UniformBuffer.Info.deviceMemory;
        flushRange.offset = 0;
        flushRange.size = VK_WHOLE_SIZE; // Or sizeof(ui_uniform)
        vkFlushMappedMemoryRanges(gfx.base->Device, 1, &flushRange);
        memcpy(gfx.UniformBuffer.Info.pMappedData, &gfx.UniformData,
               sizeof(ui_uniform));
        VkCommandBuffer cmd = BeginRender(gfx.base);

        UI_Render(&gfx, cmd);

        do_resize = EndRender(gfx.base, cmd);

        if (do_resize) {
          UI_ExternalRecreateSwapchain(&gfx);
        }
      }

      stack_free_all(UI_Context->TempAllocator);
      stack_free_all(&gfx.base->TempAllocator);
      clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts2);
      posix_dur = 1000.0 * ts2.tv_sec + 1e-6 * ts2.tv_nsec -
                  (1000.0 * ts1.tv_sec + 1e-6 * ts1.tv_nsec);
    }
    XCloseDisplay(VkBase.Window.Dpy);
  }

  gs_thread_block();

  return 0;
}
