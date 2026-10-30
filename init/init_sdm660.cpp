/*
   Copyright (c) 2016, The CyanogenMod Project
   Copyright (c) 2019-2020, The LineageOS Project

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
   BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
   BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
   IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <cstdlib>
#include <fstream>
#include <string.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/strings.h>
#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/_system_properties.h>

#include "vendor_init.h"
#include "property_service.h"

char const *heapstartsize;
char const *heapgrowthlimit;
char const *heapsize;
char const *heapminfree;
char const *heapmaxfree;
char const *heaptargetutilization;

void check_device()
{
    struct sysinfo sys;

    sysinfo(&sys);

    if (sys.totalram > 5072ull * 1024 * 1024) {
        // from - phone-xhdpi-6144-dalvik-heap.mk
        heapstartsize = "16m";
        heapgrowthlimit = "256m";
        heapsize = "512m";
        heaptargetutilization = "0.5";
        heapminfree = "8m";
        heapmaxfree = "32m";
    } else if (sys.totalram > 3072ull * 1024 * 1024) {
        // from - phone-xxhdpi-4096-dalvik-heap.mk
        heapstartsize = "8m";
        heapgrowthlimit = "256m";
        heapsize = "512m";
        heaptargetutilization = "0.6";
        heapminfree = "8m";
        heapmaxfree = "16m";
    } else {
        // from - phone-xhdpi-2048-dalvik-heap.mk
        heapstartsize = "8m";
        heapgrowthlimit = "192m";
        heapsize = "512m";
        heaptargetutilization = "0.75";
        heapminfree = "512k";
        heapmaxfree = "8m";
    }
}

void property_override(char const prop[], char const value[], bool add = true)
{
    auto pi = (prop_info *) __system_property_find(prop);

    if (pi != nullptr) {
        __system_property_update(pi, value, strlen(value));
    } else if (add) {
        __system_property_add(prop, strlen(prop), value, strlen(value));
    }
}

static void init_clover_properties()
{
    if (android::base::GetProperty("ro.product.vendor.device", "") != "clover") {
        return;
    }

    std::string panel_info;
    if (!android::base::ReadFileToString(
            "/sys/devices/virtual/graphics/fb0/msm_fb_panel_info", &panel_info)) {
        PLOG(WARNING) << "Unable to read clover panel information";
        return;
    }

    std::string panel_name;
    for (const auto& line : android::base::Split(panel_info, "\n")) {
        if (line.find("panel_name=") == 0) {
            panel_name = android::base::Trim(line.substr(strlen("panel_name=")));
            break;
        }
    }

    const bool is_plus = panel_name == "NT51021_BOE_BOE10";
    if (!is_plus && panel_name != "NT51021_BOE_BOE") {
        LOG(WARNING) << "Unknown clover panel: " << panel_name;
        return;
    }

    const char* model = is_plus ? "MI PAD 4 PLUS" : "MI PAD 4";
    const char* density = is_plus ? "265" : "320";
    property_override("ro.board.variant", is_plus ? "d9p" : "d9");
    property_override("ro.product.model", model);
    property_override("ro.product.vendor.model", model);
    property_override("ro.vendor.product.model", model);
    property_override("vendor.display.lcd_density", density);
    // ro.sf.lcd_density is already loaded from vendor/build.prop at this point.
    property_override("ro.sf.lcd_density", density);
    property_override("persist.vendor.sys.fp.vendor", is_plus ? "fpc" : "none");

    const std::string acdb_prefix = is_plus
            ? "/vendor/etc/acdbdata/QRD/sdm660-snd-card-d9p/QRD_D9P_"
            : "/vendor/etc/acdbdata/QRD/sdm660-snd-card-skush/QRD_SKUSH_";
    const char* const acdb_files[] = {
        "Bluetooth_cal.acdb", "General_cal.acdb", "Global_cal.acdb",
        "Handset_cal.acdb", "Hdmi_cal.acdb", "Headset_cal.acdb",
        "Speaker_cal.acdb", "workspaceFile.qwsp",
    };
    for (size_t i = 0; i < sizeof(acdb_files) / sizeof(acdb_files[0]); ++i) {
        const std::string prop = "persist.vendor.audio.calfile" + std::to_string(i);
        const std::string path = acdb_prefix + acdb_files[i];
        property_override(prop.c_str(), path.c_str());
    }
    property_override("persist.vendor.audio.calfile8",
                      "/vendor/etc/acdbdata/adsp_avs_config.acdb");
}

/* From Magisk@jni/magiskhide/hide_utils.c */
static const char *snet_prop_key[] = {
    "ro.boot.vbmeta.device_state",
    "ro.boot.verifiedbootstate",
    "ro.boot.flash.locked",
    "ro.boot.selinux",
    "ro.boot.veritymode",
    "ro.boot.warranty_bit",
    "ro.warranty_bit",
    "ro.debuggable",
    "ro.secure",
    "ro.build.type",
    "ro.build.tags",
    "ro.build.selinux",
    NULL
};

static const char *snet_prop_value[] = {
    "locked",
    "green",
    "1",
    "enforcing",
    "enforcing",
    "0",
    "0",
    "0",
    "1",
    "user",
    "release-keys",
    "1",
    NULL
};

static void workaround_snet_properties() {
    // Hide all sensitive props
    for (int i = 0; snet_prop_key[i]; ++i) {
        property_override(snet_prop_key[i], snet_prop_value[i]);
    }
}

void vendor_load_properties()
{
    check_device();

    property_override("dalvik.vm.heapstartsize", heapstartsize);
    property_override("dalvik.vm.heapgrowthlimit", heapgrowthlimit);
    property_override("dalvik.vm.heapsize", heapsize);
    property_override("dalvik.vm.heaptargetutilization", heaptargetutilization);
    property_override("dalvik.vm.heapminfree", heapminfree);
    property_override("dalvik.vm.heapmaxfree", heapmaxfree);

    init_clover_properties();

    // Preserve adb root and the original build properties on debug builds.
    if (!android::base::GetBoolProperty("ro.debuggable", false)) {
        workaround_snet_properties();
    }
}
