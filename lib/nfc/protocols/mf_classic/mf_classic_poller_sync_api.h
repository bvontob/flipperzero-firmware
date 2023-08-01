#pragma once

#include "mf_classic.h"
#include <nfc/nfc.h>

#ifdef __cplusplus
extern "C" {
#endif

MfClassicError mf_classic_poller_auth(
    Nfc* nfc,
    uint8_t block_num,
    MfClassicKey* key,
    MfClassicKeyType key_type,
    MfClassicAuthContext* data);

MfClassicError mf_classic_poller_read_block(
    Nfc* nfc,
    uint8_t block_num,
    MfClassicKey* key,
    MfClassicKeyType key_type,
    MfClassicBlock* data);

MfClassicError mf_classic_poller_write_block(
    Nfc* nfc,
    uint8_t block_num,
    MfClassicKey* key,
    MfClassicKeyType key_type,
    MfClassicBlock* data);

MfClassicError mf_classic_poller_read_value(
    Nfc* nfc,
    uint8_t block_num,
    MfClassicKey* key,
    MfClassicKeyType key_type,
    int32_t* value);

MfClassicError mf_classic_poller_change_value(
    Nfc* nfc,
    uint8_t block_num,
    MfClassicKey* key,
    MfClassicKeyType key_type,
    int32_t data,
    int32_t* new_value);

#ifdef __cplusplus
}
#endif