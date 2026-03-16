#ifndef TD_TRANSPORT_H
#define TD_TRANSPORT_H

#include <signal.h>

#include "td_crypto.h"

typedef enum {
    TD_WIRE_HELLO = 1,
    TD_WIRE_READ = 2,
    TD_WIRE_WRITE = 3,
    TD_WIRE_CAS = 4,
    TD_WIRE_EVICT = 5,
    TD_WIRE_ACK = 6,
    TD_WIRE_CLOSE = 7,
} td_wire_op_t;

typedef struct {
    uint32_t magic;
    uint16_t op;
    uint16_t status;
    uint64_t offset;
    uint64_t length;
    uint64_t compare;
    uint64_t swap;
    uint64_t remote_addr;
    uint32_t rkey;
    uint32_t reserved;
    td_region_header_t header;
} td_wire_msg_t;

struct td_session;

typedef int (*td_read_fn)(struct td_session *session, size_t offset, void *buf, size_t len, char *err, size_t err_len);
typedef int (*td_write_fn)(struct td_session *session, size_t offset, const void *buf, size_t len, char *err, size_t err_len);
typedef int (*td_cas_fn)(struct td_session *session, size_t offset, uint64_t compare, uint64_t swap, uint64_t *old_value, char *err, size_t err_len);
typedef int (*td_ctrl_fn)(struct td_session *session, td_wire_op_t op, char *err, size_t err_len);
typedef void (*td_close_fn)(struct td_session *session);

typedef struct td_session {
    td_transport_t transport;
    td_endpoint_t endpoint;
    uint64_t remote_addr;
    uint32_t rkey;
    td_region_header_t header;
    size_t region_size;
    void *impl;
    td_read_fn read_region;
    td_write_fn write_region;
    td_cas_fn cas64;
    td_ctrl_fn control;
    td_close_fn close;
} td_session_t;

int td_session_connect(td_session_t *session, const td_config_t *cfg, const td_endpoint_t *endpoint, char *err, size_t err_len);
void td_session_close(td_session_t *session);

int td_tcp_server_run(const td_config_t *cfg, td_local_region_t *region, volatile sig_atomic_t *stop_flag, char *err, size_t err_len);
int td_rdma_server_run(const td_config_t *cfg, td_local_region_t *region, volatile sig_atomic_t *stop_flag, char *err, size_t err_len);

#endif
