// MODULE: ip
// Purpose : list local IPv4 addresses per network adapter
// Exports : ip_cmd()
// Depends : common.h

#include <iphlpapi.h>

static int ip_cmd() {
    ULONG size = 0;
    GetAdaptersInfo(nullptr, &size);
    std::vector<uint8_t> buf(size);
    auto* ai = reinterpret_cast<IP_ADAPTER_INFO*>(buf.data());
    if (GetAdaptersInfo(ai, &size) != NO_ERROR) {
        err("Failed to get adapter info\r\n");
        return 1;
    }

    bool found = false;
    for (auto* a = ai; a; a = a->Next) {
        std::string ip = a->IpAddressList.IpAddress.String;
        if (ip == "0.0.0.0" || ip.substr(0, 4) == "127.") continue;
        char line[320];
        snprintf(line, sizeof(line), "%-18s: " GREEN "%s" RESET "\r\n", a->Description, ip.c_str());
        out(line);
        found = true;
    }

    if (!found) out(GRAY "No active network adapters found\r\n" RESET);
    return 0;
}
