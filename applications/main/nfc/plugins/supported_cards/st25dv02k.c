#include "nfc_supported_card_plugin.h"

#include <flipper_application/flipper_application.h>

#include <nfc/nfc_device.h>
#include <nfc/helpers/nfc_util.h>
#include <nfc/protocols/iso15693_3/iso15693_3.h>

#define TAG "ST25DV02K"

#define ST25DV02K_UID_LENGTH (8)

static const char* st25dv02k_get_product_name(uint8_t product_code) {
    switch(product_code) {
    case 0x38:
        return "ST25DV02K-W1";
    case 0x39:
        return "ST25DV02K-W2";
    default:
        return "unknown";
    }
}

bool st25dv02k_verify(Nfc* nfc) {
    bool verified = false;

    UNUSED(nfc);

    do {
        verified = false;
    } while(false);

    return verified;
}

static bool st25dv02k_read(Nfc* nfc, NfcDevice* device) {
    furi_assert(nfc);
    furi_assert(device);

    bool is_read = false;

    do {
        is_read = false;
    } while(false);

    return is_read;
}

static bool st25dv02k_parse(const NfcDevice* device, FuriString* parsed_data) {
    furi_assert(device);

    bool parsed = false;
    
    UNUSED(st25dv02k_read);

    // const Iso15693_3Data* data = nfc_device_get_data(device, NfcProtocolIso15693_3);

    do {
        size_t uid_len;
        const uint8_t* uid = nfc_device_get_uid(device, &uid_len);
        
        if(uid_len != ST25DV02K_UID_LENGTH) break;
        if(uid[0] != 0xE0) break;
        if(uid[1] != 0x02) break;
        if(uid[2] != 0x38 && uid[2] != 0x39) break;
        
        furi_string_printf(
                           parsed_data,
                           "\e#%s (%s)\n%s\n%s\nUID:\n%02X %02X %02X %02X %02X %02X %02X %02X",
                           nfc_device_get_protocol_name(nfc_device_get_protocol(device)),
                           TAG,
                           nfc_device_get_name(device, NfcDeviceNameTypeFull),
                           st25dv02k_get_product_name(uid[2]),
                           uid[0],
                           uid[1],
                           uid[2],
                           uid[3],
                           uid[4],
                           uid[5],
                           uid[6],
                           uid[7]);
        
        parsed = true;
    } while(false);

    return parsed;
}

/* Actual implementation of app<>plugin interface */
static const NfcSupportedCardsPlugin st25dv02k_plugin = {
    .protocol = NfcProtocolIso15693_3,
    .verify = st25dv02k_verify,
    .read = st25dv02k_read,
    .parse = st25dv02k_parse,
};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor st25dv02k_plugin_descriptor = {
    .appid = NFC_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = NFC_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &st25dv02k_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* st25dv02k_plugin_ep() {
    return &st25dv02k_plugin_descriptor;
}
