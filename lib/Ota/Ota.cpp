/*************************************************************************
*                                                                        *
*   Ota.cpp                                                              *
*   Which firmware is running, and the probation after an update.        *
*                                                                        *
*   Written October 2026                                                 *
*                                                                        *
**************************************************************************/

#include "Ota.h"

#include <Arduino.h>
#include <esp_app_desc.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <Logbook.h>


// Arduino's startup code confirms a new firmware the moment it boots, unless
// this returns true. Returning true hands that decision to ota::tick().
extern "C" bool verifyRollbackLater()
{
    return true;
}


namespace
{
    char fw_id[9]     = "";      // 8 hex digits
    bool on_probation = false;
    bool rolled_back  = false;
}


void ota::begin()
{
    esp_app_get_elf_sha256(fw_id, sizeof(fw_id));

    esp_ota_img_states_t st;
    on_probation = esp_ota_get_state_partition(esp_ota_get_running_partition(), &st) == ESP_OK
                   && st == ESP_OTA_IMG_PENDING_VERIFY;
    rolled_back  = esp_ota_get_last_invalid_partition() != nullptr;

    logbook::add("OTA: running firmware %s%s", fw_id, on_probation ? " (new, on probation)" : "");
    if (rolled_back) {
        logbook::add("OTA: the last update failed its checks, so the box went back to this firmware");
    }
}


void ota::tick(bool healthy)
{
    if (!on_probation) return;

    const uint32_t now = millis();

    if (healthy && now >= SETTLE_MS) {
        esp_ota_mark_app_valid_cancel_rollback();
        on_probation = false;
        logbook::add("OTA: new firmware confirmed");
    }
    else if (now >= DEADLINE_MS) {
        // Restarting while still on probation *is* the rollback: the
        // bootloader marks this firmware as failed and boots the previous
        // one. (Unlike esp_ota_mark_app_invalid_rollback_and_reboot(), this
        // also works on the first update after a USB flash, when the old
        // firmware has no OTA record to go back to.)
        logbook::add("OTA: new firmware never came online; going back to the previous one");
        delay(100);   // let the line reach the serial monitor
        esp_restart();
    }
}


const char* ota::firmware_id()
{
    return fw_id;
}


bool ota::last_update_failed()
{
    return rolled_back;
}
