#include "expansion.h"

#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>

#include <furi.h>

#include <rpc/rpc.h>

#include "expansion_settings.h"
#include "expansion_protocol.h"

#define TAG "ExpansionSrv"

#define EXPANSION_INACTIVE_TIMEOUT_MS (250U)
#define EXPANSION_BUFFER_SIZE (64U)

typedef enum {
    ExpansionStateDisabled,
    ExpansionStateEnabled,
    ExpansionStateRunning,
} ExpansionState;

typedef enum {
    ExpansionSessionStateHandShake,
    ExpansionSessionStateNormal,
    ExpansionSessionStateRpc,
} ExpansionSessionState;

typedef enum {
    ExpansionSessionExitReasonUnknown,
    ExpansionSessionExitReasonUser,
    ExpansionSessionExitReasonError,
    ExpansionSessionExitReasonTimeout,
} ExpansionSessionExitReason;

typedef enum {
    ExpansionFlagStop = 1 << 0,
    ExpansionFlagData = 1 << 1,
} ExpansionFlag;

#define EXPANSION_ALL_FLAGS (ExpansionFlagData | ExpansionFlagStop)

struct Expansion {
    ExpansionState state;
    ExpansionSessionState session_state;
    ExpansionSessionExitReason exit_reason;
    ExpansionFrame frame;
    FuriMutex* state_mutex;
    FuriThread* worker_thread;
    FuriStreamBuffer* buf;
    FuriHalSerialId serial_id;
    FuriHalSerialHandle* handle;
    RpcSession* rpc_session;
};

static void expansion_detect_callback(void* context);

// Called in uart IRQ context
static void expansion_serial_rx_callback(uint8_t data, void* context) {
    furi_assert(context);
    Expansion* instance = context;

    furi_stream_buffer_send(instance->buf, &data, sizeof(data), 0);
    furi_thread_flags_set(furi_thread_get_id(instance->worker_thread), ExpansionFlagData);
}

static size_t expansion_receive_callback(uint8_t* data, size_t data_size, void* context) {
    Expansion* instance = context;

    size_t received_size = 0;

    while(true) {
        received_size += furi_stream_buffer_receive(
            instance->buf, data + received_size, data_size - received_size, 0);

        if(received_size == data_size) break;

        const uint32_t flags = furi_thread_flags_wait(
            EXPANSION_ALL_FLAGS, FuriFlagWaitAny, EXPANSION_INACTIVE_TIMEOUT_MS);

        if(flags & ExpansionFlagStop) {
            // Exiting due to explicit request
            instance->exit_reason = ExpansionSessionExitReasonUser;
            break;
        } else if(flags & ExpansionFlagData) {
            // Go to buffer reading
            continue;
        } else {
            // Did not receive any flags, exiting due to timeout
            instance->exit_reason = ExpansionSessionExitReasonTimeout;
            break;
        }
    }

    return received_size;
}

static size_t expansion_send_callback(const uint8_t* data, size_t data_size, void* context) {
    Expansion* instance = context;

    // TODO: Do not call it from RPC thread!
    furi_hal_serial_tx(instance->handle, data, data_size);

    return data_size;
}

static void expansion_send_status_response(Expansion* instance, ExpansionFrameError error) {
    ExpansionFrame frame = {
        .header.type = ExpansionFrameTypeStatus,
        .content.status.error = error,
    };

    expansion_frame_encode(&frame, expansion_send_callback, instance);
}

// Can be called in Rpc session thread context OR in expansion worker context
// TODO: Do not call it from RPC thread!
static void
    expansion_send_data_response(Expansion* instance, const uint8_t* data, size_t data_size) {
    furi_assert(data_size <= EXPANSION_MAX_DATA_SIZE);

    ExpansionFrame frame = {
        .header.type = ExpansionFrameTypeData,
        .content.data.size = data_size,
    };

    memcpy(frame.content.data.bytes, data, data_size);
    expansion_frame_encode(&frame, expansion_send_callback, instance);
}

// Called in Rpc session thread context
static void expansion_rpc_send_callback(void* context, uint8_t* data, size_t data_size) {
    // TODO: split big data across several packets
    furi_assert(data_size <= EXPANSION_MAX_DATA_SIZE);
    Expansion* instance = context;
    // TODO: Do not allow sending data directly from RPC thread context !!!!
    expansion_send_data_response(instance, data, data_size);
}

static void expansion_rpc_session_open(Expansion* instance) {
    Rpc* rpc = furi_record_open(RECORD_RPC);
    instance->rpc_session = rpc_session_open(rpc, RpcOwnerUnknown);

    rpc_session_set_context(instance->rpc_session, instance);
    rpc_session_set_send_bytes_callback(instance->rpc_session, expansion_rpc_send_callback);
}

static void expansion_rpc_session_close(Expansion* instance) {
    rpc_session_close(instance->rpc_session);
    furi_record_close(RECORD_RPC);
}

static int32_t expansion_worker(void* context) {
    furi_assert(context);
    Expansion* instance = context;

    furi_hal_serial_control_set_expansion_callback(instance->serial_id, NULL, NULL);

    instance->handle = furi_hal_serial_control_acquire(instance->serial_id);
    furi_check(instance->handle);

    instance->buf = furi_stream_buffer_alloc(EXPANSION_BUFFER_SIZE, 1);
    instance->session_state = ExpansionSessionStateHandShake;
    instance->exit_reason = ExpansionSessionExitReasonUnknown;

    furi_hal_serial_init(instance->handle, EXPANSION_DEFAULT_BAUD_RATE);
    furi_hal_serial_set_rx_callback(instance->handle, expansion_serial_rx_callback, instance);

    while(true) {
        if(!expansion_frame_decode(&instance->frame, expansion_receive_callback, instance)) {
            break;
        }

        if(instance->session_state == ExpansionSessionStateHandShake) {
            if(instance->frame.header.type == ExpansionFrameTypeBaudRate) {
                // TODO: proper baud rate check
                if(instance->frame.content.baud_rate.baud == 230400) {
                    // Send response on previous baud rate
                    expansion_send_status_response(instance, ExpansionFrameErrorNone);
                    // Set new baud rate
                    furi_hal_serial_set_br(instance->handle, 230400);
                    instance->session_state = ExpansionSessionStateNormal;
                    // Go to the next iteration
                    continue;
                }
            }

        } else if(instance->session_state == ExpansionSessionStateNormal) {
            if(instance->frame.header.type == ExpansionFrameTypeControl) {
                if(instance->frame.content.control.command ==
                   ExpansionFrameControlCommandStartRpc) {
                    instance->session_state = ExpansionSessionStateRpc;
                    expansion_rpc_session_open(instance);
                    expansion_send_status_response(instance, ExpansionFrameErrorNone);
                    // Go to the next iteration
                    continue;
                }
            }

        } else if(instance->session_state == ExpansionSessionStateRpc) {
            if(instance->frame.header.type == ExpansionFrameTypeData) {
                const size_t size_consumed = rpc_session_feed(
                    instance->rpc_session,
                    instance->frame.content.data.bytes,
                    instance->frame.content.data.size,
                    EXPANSION_INACTIVE_TIMEOUT_MS);

                if(size_consumed != instance->frame.content.data.size) {
                    // Send HOLD response
                    // Restart the RPC session
                } else {
                    expansion_send_status_response(instance, ExpansionFrameErrorNone);
                }

                // Go to the next iteration
                continue;

            } else if(instance->frame.header.type == ExpansionFrameTypeControl) {
                if(instance->frame.content.control.command ==
                   ExpansionFrameControlCommandStopRpc) {
                    expansion_rpc_session_close(instance);
                    instance->session_state = ExpansionSessionStateNormal;
                    expansion_send_status_response(instance, ExpansionFrameErrorNone);
                    // Go to the next iteration
                    continue;
                }
            }

        } else {
            furi_crash();
        }

        // In any confusing situation, respond with an error
        expansion_send_status_response(instance, ExpansionFrameErrorUnknown);
    }

    if(instance->session_state == ExpansionSessionStateRpc) {
        expansion_rpc_session_close(instance);
    }

    furi_hal_serial_control_release(instance->handle);
    furi_stream_buffer_free(instance->buf);

    if(instance->exit_reason == ExpansionSessionExitReasonTimeout) {
        // Thread exited due to timeout, and no disable request was issued by user code
        // Re-enable the expansion detection interrupt and be ready to establish a new connection
        furi_mutex_acquire(instance->state_mutex, FuriWaitForever);
        instance->state = ExpansionStateEnabled;
        furi_hal_serial_control_set_expansion_callback(
            instance->serial_id, expansion_detect_callback, instance);
        furi_mutex_release(instance->state_mutex);
    }

    return 0;
}

// Called from the serial control thread
static void expansion_detect_callback(void* context) {
    furi_assert(context);
    Expansion* instance = context;

    furi_mutex_acquire(instance->state_mutex, FuriWaitForever);

    if(instance->state == ExpansionStateEnabled) {
        instance->state = ExpansionStateRunning;
        furi_thread_start(instance->worker_thread);
    }

    furi_mutex_release(instance->state_mutex);
}

static Expansion* expansion_alloc() {
    Expansion* instance = malloc(sizeof(Expansion));

    instance->state_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    instance->worker_thread = furi_thread_alloc_ex(TAG, 1024, expansion_worker, instance);

    return instance;
}

void expansion_on_system_start(void* arg) {
    UNUSED(arg);

    Expansion* instance = expansion_alloc();
    furi_record_create(RECORD_EXPANSION, instance);

    ExpansionSettings settings = {};
    if(!expansion_settings_load(&settings)) {
        expansion_settings_save(&settings);
    } else if(settings.uart_index != 0) {
        expansion_enable(instance, settings.uart_index - 1);
    }
}

// Public API functions

void expansion_enable(Expansion* instance, FuriHalSerialId serial_id) {
    expansion_disable(instance);

    furi_mutex_acquire(instance->state_mutex, FuriWaitForever);

    instance->serial_id = serial_id;
    instance->state = ExpansionStateEnabled;

    furi_hal_serial_control_set_expansion_callback(
        instance->serial_id, expansion_detect_callback, instance);

    furi_mutex_release(instance->state_mutex);
}

void expansion_disable(Expansion* instance) {
    furi_mutex_acquire(instance->state_mutex, FuriWaitForever);

    if(instance->state == ExpansionStateRunning) {
        furi_thread_flags_set(furi_thread_get_id(instance->worker_thread), ExpansionFlagStop);
        furi_thread_join(instance->worker_thread);
    } else if(instance->state == ExpansionStateEnabled) {
        furi_hal_serial_control_set_expansion_callback(instance->serial_id, NULL, NULL);
    }

    instance->state = ExpansionStateDisabled;

    furi_mutex_release(instance->state_mutex);
}