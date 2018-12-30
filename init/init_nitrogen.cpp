
/*
   Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#include <fstream>
#include <vector>

#include <android-base/logging.h>
#include <android-base/strings.h>

#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>
#include <sys/mount.h>
#include <sys/stat.h>

using android::base::Trim;

namespace android {
namespace init {

const std::string tag = "init_nitrogen: ";
std::vector<std::string> system_buildprops;
std::vector<std::string> vendor_buildprops;

int read_file(std::string fn, std::vector<std::string>& results) {
    std::ifstream file(fn);
    std::string line;

    if (file.is_open()) {
        while (getline(file, line))
            results.push_back(line);

        file.close();
        return 0;
    }
    return -1;
}

void read_buildprops(const std::string& partition) {
    const std::string bootdevice = "/dev/block/bootdevice/by-name" + partition;
    const std::string mnt_point = "mnt_point";
    std::vector<std::string> buildprops;

    mkdir(mnt_point.c_str(), 755);
    mount(bootdevice.c_str(), mnt_point.c_str(), "ext4", MS_RDONLY, NULL);

    if (read_file(mnt_point + "/build.prop", buildprops) != 0) {
        LOG(ERROR) << tag << "Cannot read " << partition << "/build.prop";
        umount(mnt_point.c_str());
        rmdir(mnt_point.c_str());
        return;
    }

    umount(mnt_point.c_str());
    rmdir(mnt_point.c_str());

    if (partition == "/system")
        system_buildprops = buildprops;
    if (partition == "/vendor")
        vendor_buildprops = buildprops;
}

std::string property_get(const std::string& propname, bool system) {
    std::string propvalue = "not found";
    std::vector<std::string> buildprops;
    std::string which;

    if (system) {
        buildprops = system_buildprops;
        which = "system";
    } else {
        buildprops = vendor_buildprops;
        which = "vendor";
    }

    unsigned long numb_lines = buildprops.size();

    if (numb_lines > 0) {
        std::string fnd_propname;
        unsigned long index;
        size_t start_pos = 0, end_pos;

        for (index = 0; index < numb_lines; index++) {
            end_pos = buildprops.at(index).find("=", start_pos);
            fnd_propname = buildprops.at(index).substr(start_pos, end_pos);
            if (fnd_propname == propname) {
                propvalue = buildprops.at(index).substr(end_pos + 1, buildprops.at(index).size());
                if (Trim(propvalue) == "")
                    propvalue = "";

                return propvalue;
            }
        }
    }
    LOG(INFO) << tag << "Cannot find property "
                    << propname << " in /" << which << "/build.prop";

    return propvalue;
}

void property_override(char const prop[], char const value[]) {
    prop_info *pi;

    pi = (prop_info*) __system_property_find(prop);
    if (pi)
        __system_property_update(pi, value, strlen(value));
    else
        __system_property_add(prop, strlen(prop), value, strlen(value));
}

void vendor_load_properties() {
    std::string propvalue;

    read_buildprops("/system");
    //read_buildprops("/vendor");

    propvalue = property_get("ro.build.fingerprint", true);
    if (Trim(propvalue) != "not found") {
        LOG(INFO) << tag << "Overriding recovery property -> "
            << "ro.build.fingerprint=" << propvalue;
        property_override("ro.build.fingerprint", propvalue.c_str());
    }

    propvalue = property_get("ro.build.version.release", true);
    if (Trim(propvalue) != "not found") {
        LOG(INFO) << tag << "Overriding recovery property -> "
            << "ro.build.version.release=" << propvalue;
        property_override("ro.build.version.release", propvalue.c_str());
    }

    propvalue = property_get("ro.build.version.incremental", true);
    if (Trim(propvalue) != "not found") {
        LOG(INFO) << tag << "Overriding recovery property -> "
            << "ro.build.version.incremental=" << propvalue;
        property_override("ro.build.version.incremental", propvalue.c_str());
    }

    propvalue = property_get("ro.miui.ui.version.name", true);
    if (Trim(propvalue) != "not found") {
        LOG(INFO) << tag << "Overriding recovery property -> "
            << "ro.miui.ui.version.name=" << propvalue;
        property_override("ro.miui.ui.version.name", propvalue.c_str());
    }
}

}  // namespace init
}  // namespace android
