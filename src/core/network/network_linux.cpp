/**
 * @file network_linux.cpp
 * @brief Linux network monitoring implementation using /proc and sysfs.
 */

#ifdef __linux__

#include "network_linux.h"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <arpa/inet.h>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

LinuxNetwork::LinuxNetwork()
    : prevTime_(std::chrono::steady_clock::now()),
      lastInodeScan_(std::chrono::steady_clock::now() - std::chrono::seconds(10))
{
}

LinuxNetwork::~LinuxNetwork() = default;

std::string LinuxNetwork::tcpStateToString(int state) {
    switch (state) {
        case 0x01: return "ESTABLISHED";
        case 0x02: return "SYN_SENT";
        case 0x03: return "SYN_RECV";
        case 0x04: return "FIN_WAIT1";
        case 0x05: return "FIN_WAIT2";
        case 0x06: return "TIME_WAIT";
        case 0x07: return "CLOSE";
        case 0x08: return "CLOSE_WAIT";
        case 0x09: return "LAST_ACK";
        case 0x0A: return "LISTEN";
        case 0x0B: return "CLOSING";
        default:   return "UNKNOWN";
    }
}

std::string LinuxNetwork::resolveProcessName(int pid) {
    if (pid <= 0) return "N/A";

    auto it = processNameCache_.find(pid);
    if (it != processNameCache_.end()) return it->second;

    std::string name = "Unknown";
    std::string commPath = "/proc/" + std::to_string(pid) + "/comm";
    std::ifstream f(commPath);
    if (f.is_open()) {
        std::getline(f, name);
        if (name.empty()) name = "Unknown";
    }
    processNameCache_[pid] = name;
    return name;
}

void LinuxNetwork::refreshInodePidMap() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - lastInodeScan_).count() < 5) {
        return;
    }
    lastInodeScan_ = now;
    inodePidMap_.clear();

    DIR* procDir = opendir("/proc");
    if (!procDir) return;

    struct dirent* pEntry = nullptr;
    while ((pEntry = readdir(procDir)) != nullptr) {
        if (pEntry->d_type != DT_DIR && pEntry->d_type != DT_UNKNOWN) continue;
        const char* dname = pEntry->d_name;
        bool allDigit = true;
        for (const char* p = dname; *p; ++p) {
            if (!std::isdigit(static_cast<unsigned char>(*p))) { allDigit = false; break; }
        }
        if (!allDigit || dname[0] == '\0') continue;

        int pid = std::atoi(dname);
        std::string fdDir = std::string("/proc/") + dname + "/fd";
        DIR* fdDirPtr = opendir(fdDir.c_str());
        if (!fdDirPtr) continue;

        struct dirent* fdEntry = nullptr;
        while ((fdEntry = readdir(fdDirPtr)) != nullptr) {
            std::string linkPath = fdDir + "/" + fdEntry->d_name;
            char target[256] = {};
            ssize_t len = readlink(linkPath.c_str(), target, sizeof(target) - 1);
            if (len <= 0) continue;
            target[len] = '\0';

            if (std::strncmp(target, "socket:[", 8) == 0) {
                uint64_t inode = std::strtoull(target + 8, nullptr, 10);
                if (inode > 0) {
                    inodePidMap_[inode] = pid;
                }
            }
        }
        closedir(fdDirPtr);
    }
    closedir(procDir);
}

void LinuxNetwork::parseNetDev(std::vector<NetworkInterfaceInfo>& ifaces, double dtSec) {
    std::ifstream f("/proc/net/dev");
    if (!f.is_open()) return;

    std::string line;
    std::getline(f, line);
    std::getline(f, line);

    std::unordered_map<std::string, IfPrev> newPrev;

    while (std::getline(f, line)) {
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string name = line.substr(0, colon);
        name.erase(0, name.find_first_not_of(" \t"));
        name.erase(name.find_last_not_of(" \t") + 1);

        if (name == "lo") continue;

        std::istringstream ss(line.substr(colon + 1));
        uint64_t rxBytes, rxPackets, rxErrors, rxDrops, rxFifo, rxFrame, rxCompressed, rxMulticast;
        uint64_t txBytes, txPackets, txErrors, txDrops, txFifo, txColls, txCarrier, txCompressed;

        ss >> rxBytes >> rxPackets >> rxErrors >> rxDrops
           >> rxFifo  >> rxFrame   >> rxCompressed >> rxMulticast
           >> txBytes >> txPackets >> txErrors >> txDrops
           >> txFifo  >> txColls   >> txCarrier >> txCompressed;

        NetworkInterfaceInfo info;
        info.name       = name;
        info.totalRecv  = rxBytes;
        info.totalSent  = txBytes;
        info.packetsIn  = rxPackets;
        info.packetsOut = txPackets;
        info.errorsIn   = rxErrors;
        info.errorsOut  = txErrors;
        info.dropsIn    = rxDrops;
        info.dropsOut   = txDrops;

        info.isUp          = readOperState(name);
        info.linkSpeedMbps = readLinkSpeed(name);

        if (hasPrevSample_) {
            auto pit = prevCounters_.find(name);
            if (pit != prevCounters_.end()) {
                uint64_t dRx = (rxBytes >= pit->second.rxBytes)
                                ? (rxBytes - pit->second.rxBytes) : 0;
                uint64_t dTx = (txBytes >= pit->second.txBytes)
                                ? (txBytes - pit->second.txBytes) : 0;
                info.downloadRate = static_cast<float>(dRx / dtSec);
                info.uploadRate   = static_cast<float>(dTx / dtSec);
            }
        }

        newPrev[name] = { rxBytes, txBytes, rxPackets, txPackets,
                          rxErrors, txErrors, rxDrops, txDrops };

        ifaces.push_back(std::move(info));
    }

    prevCounters_ = std::move(newPrev);
}

void LinuxNetwork::fillAddresses(std::vector<NetworkInterfaceInfo>& ifaces) {
    struct ifaddrs* ifap = nullptr;
    if (getifaddrs(&ifap) != 0) return;

    std::unordered_map<std::string, std::string> ipMap;
    std::unordered_map<std::string, std::string> macMap;

    for (auto* ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        std::string ifName = ifa->ifa_name;

        if (ifa->ifa_addr->sa_family == AF_INET) {
            auto* sa = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
            char buf[INET_ADDRSTRLEN] = {};
            inet_ntop(AF_INET, &sa->sin_addr, buf, sizeof(buf));
            ipMap[ifName] = buf;
        } else if (ifa->ifa_addr->sa_family == AF_PACKET) {
            auto* sll = reinterpret_cast<sockaddr_ll*>(ifa->ifa_addr);
            if (sll->sll_halen == 6) {
                char macBuf[18] = {};
                std::snprintf(macBuf, sizeof(macBuf),
                    "%02x:%02x:%02x:%02x:%02x:%02x",
                    sll->sll_addr[0], sll->sll_addr[1], sll->sll_addr[2],
                    sll->sll_addr[3], sll->sll_addr[4], sll->sll_addr[5]);
                macMap[ifName] = macBuf;
            }
        }
    }
    freeifaddrs(ifap);

    for (auto& info : ifaces) {
        auto iit = ipMap.find(info.name);
        if (iit != ipMap.end()) info.ipAddress = iit->second;
        auto mit = macMap.find(info.name);
        if (mit != macMap.end()) info.macAddress = mit->second;
    }
}

float LinuxNetwork::readLinkSpeed(const std::string& iface) {
    std::string path = "/sys/class/net/" + iface + "/speed";
    std::ifstream f(path);
    if (!f.is_open()) return 0.0f;
    int speed = 0;
    f >> speed;
    if (f.fail() || speed < 0) return 0.0f;
    return static_cast<float>(speed);
}

bool LinuxNetwork::readOperState(const std::string& iface) {
    std::string path = "/sys/class/net/" + iface + "/operstate";
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::string state;
    f >> state;
    return (state == "up");
}

std::vector<TcpConnection> LinuxNetwork::parseTcpConnections() {
    std::vector<TcpConnection> conns;

    refreshInodePidMap();

    std::ifstream f("/proc/net/tcp");
    if (!f.is_open()) return conns;

    std::string line;
    std::getline(f, line);

    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string sl, localHex, remoteHex, stHex;
        std::string txrx, trtm, retrans;
        int uid = 0;
        int timeout = 0;
        uint64_t inode = 0;

        ss >> sl >> localHex >> remoteHex >> stHex
           >> txrx >> trtm >> retrans >> uid >> timeout >> inode;

        auto parseAddr = [](const std::string& hex, std::string& ip, uint16_t& port) {
            auto colon = hex.find(':');
            if (colon == std::string::npos) return;
            uint32_t ipRaw = static_cast<uint32_t>(std::strtoul(hex.substr(0, colon).c_str(), nullptr, 16));
            port = static_cast<uint16_t>(std::strtoul(hex.substr(colon + 1).c_str(), nullptr, 16));
            in_addr addr;
            addr.s_addr = ipRaw;
            char buf[INET_ADDRSTRLEN] = {};
            inet_ntop(AF_INET, &addr, buf, sizeof(buf));
            ip = buf;
        };

        TcpConnection conn;
        parseAddr(localHex,  conn.localAddr,  conn.localPort);
        parseAddr(remoteHex, conn.remoteAddr, conn.remotePort);

        int stateInt = static_cast<int>(std::strtol(stHex.c_str(), nullptr, 16));
        conn.state = tcpStateToString(stateInt);

        auto pit = inodePidMap_.find(inode);
        if (pit != inodePidMap_.end()) {
            conn.pid = pit->second;
            conn.processName = resolveProcessName(conn.pid);
        } else {
            conn.pid = 0;
            conn.processName = "N/A";
        }

        conns.push_back(std::move(conn));
    }

    return conns;
}

std::vector<TcpConnection> LinuxNetwork::parseTcp6Connections() {
    std::vector<TcpConnection> conns;
    refreshInodePidMap();

    std::ifstream f("/proc/net/tcp6");
    if (!f.is_open()) return conns;

    std::string line;
    std::getline(f, line);

    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string sl, localHex, remoteHex, stHex;
        std::string txrx, trtm, retrans;
        int uid = 0, timeout = 0;
        uint64_t inode = 0;

        ss >> sl >> localHex >> remoteHex >> stHex
           >> txrx >> trtm >> retrans >> uid >> timeout >> inode;

        auto parseAddr6 = [](const std::string& hex, std::string& ip, uint16_t& port) {
            auto colon = hex.find(':');
            if (colon == std::string::npos || colon < 32) return;
            std::string addrHex = hex.substr(0, colon);
            port = static_cast<uint16_t>(std::strtoul(hex.substr(colon + 1).c_str(), nullptr, 16));

            unsigned char addr[16] = {};
            for (int i = 0; i < 4; ++i) {
                uint32_t group = static_cast<uint32_t>(
                    std::strtoul(addrHex.substr(i * 8, 8).c_str(), nullptr, 16));
                addr[i*4 + 0] = static_cast<unsigned char>((group >>  0) & 0xFF);
                addr[i*4 + 1] = static_cast<unsigned char>((group >>  8) & 0xFF);
                addr[i*4 + 2] = static_cast<unsigned char>((group >> 16) & 0xFF);
                addr[i*4 + 3] = static_cast<unsigned char>((group >> 24) & 0xFF);
            }

            char buf[INET6_ADDRSTRLEN] = {};
            inet_ntop(AF_INET6, addr, buf, sizeof(buf));
            ip = buf;
        };

        TcpConnection conn;
        parseAddr6(localHex,  conn.localAddr,  conn.localPort);
        parseAddr6(remoteHex, conn.remoteAddr, conn.remotePort);

        int stateInt = static_cast<int>(std::strtol(stHex.c_str(), nullptr, 16));
        conn.state = tcpStateToString(stateInt);

        auto pit = inodePidMap_.find(inode);
        if (pit != inodePidMap_.end()) {
            conn.pid = pit->second;
            conn.processName = resolveProcessName(conn.pid);
        } else {
            conn.pid = 0;
            conn.processName = "N/A";
        }

        conns.push_back(std::move(conn));
    }

    return conns;
}

std::vector<TcpConnection> LinuxNetwork::parseUdpConnections(const std::string& path) {
    std::vector<TcpConnection> conns;
    refreshInodePidMap();

    std::ifstream f(path);
    if (!f.is_open()) return conns;

    bool isV6 = (path.find("udp6") != std::string::npos);

    std::string line;
    std::getline(f, line);

    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string sl, localHex, remoteHex, stHex;
        std::string txrx, trtm, retrans;
        int uid = 0, timeout = 0;
        uint64_t inode = 0;

        ss >> sl >> localHex >> remoteHex >> stHex
           >> txrx >> trtm >> retrans >> uid >> timeout >> inode;

        TcpConnection conn;
        conn.state = "UDP";

        if (isV6) {
            auto parseAddr6 = [](const std::string& hex, std::string& ip, uint16_t& port) {
                auto colon = hex.find(':');
                if (colon == std::string::npos || colon < 32) return;
                std::string addrHex = hex.substr(0, colon);
                port = static_cast<uint16_t>(std::strtoul(hex.substr(colon + 1).c_str(), nullptr, 16));

                unsigned char addr[16] = {};
                for (int i = 0; i < 4; ++i) {
                    uint32_t group = static_cast<uint32_t>(
                        std::strtoul(addrHex.substr(i * 8, 8).c_str(), nullptr, 16));
                    addr[i*4 + 0] = static_cast<unsigned char>((group >>  0) & 0xFF);
                    addr[i*4 + 1] = static_cast<unsigned char>((group >>  8) & 0xFF);
                    addr[i*4 + 2] = static_cast<unsigned char>((group >> 16) & 0xFF);
                    addr[i*4 + 3] = static_cast<unsigned char>((group >> 24) & 0xFF);
                }

                char buf[INET6_ADDRSTRLEN] = {};
                inet_ntop(AF_INET6, addr, buf, sizeof(buf));
                ip = buf;
            };
            parseAddr6(localHex,  conn.localAddr,  conn.localPort);
            parseAddr6(remoteHex, conn.remoteAddr, conn.remotePort);
        } else {
            auto parseAddr4 = [](const std::string& hex, std::string& ip, uint16_t& port) {
                auto colon = hex.find(':');
                if (colon == std::string::npos) return;
                uint32_t ipRaw = static_cast<uint32_t>(std::strtoul(hex.substr(0, colon).c_str(), nullptr, 16));
                port = static_cast<uint16_t>(std::strtoul(hex.substr(colon + 1).c_str(), nullptr, 16));
                in_addr addr;
                addr.s_addr = ipRaw;
                char buf[INET_ADDRSTRLEN] = {};
                inet_ntop(AF_INET, &addr, buf, sizeof(buf));
                ip = buf;
            };
            parseAddr4(localHex,  conn.localAddr,  conn.localPort);
            parseAddr4(remoteHex, conn.remoteAddr, conn.remotePort);
        }

        auto pit = inodePidMap_.find(inode);
        if (pit != inodePidMap_.end()) {
            conn.pid = pit->second;
            conn.processName = resolveProcessName(conn.pid);
        } else {
            conn.pid = 0;
            conn.processName = "N/A";
        }

        conns.push_back(std::move(conn));
    }

    return conns;
}

void LinuxNetwork::update() {
    NetworkSnapshot local;
    auto now = std::chrono::steady_clock::now();
    double dtSec = std::chrono::duration<double>(now - prevTime_).count();
    if (dtSec <= 0.0) dtSec = 1.0;

    parseNetDev(local.interfaces, dtSec);

    fillAddresses(local.interfaces);

    for (const auto& iface : local.interfaces) {
        local.totalBytesSent += iface.totalSent;
        local.totalBytesRecv += iface.totalRecv;
        local.totalUploadRate   += iface.uploadRate;
        local.totalDownloadRate += iface.downloadRate;
    }

    local.connections = parseTcpConnections();
    {
        auto v6conns = parseTcp6Connections();
        local.connections.insert(local.connections.end(),
                                 std::make_move_iterator(v6conns.begin()),
                                 std::make_move_iterator(v6conns.end()));
        auto udp4 = parseUdpConnections("/proc/net/udp");
        local.connections.insert(local.connections.end(),
                                 std::make_move_iterator(udp4.begin()),
                                 std::make_move_iterator(udp4.end()));
        auto udp6 = parseUdpConnections("/proc/net/udp6");
        local.connections.insert(local.connections.end(),
                                 std::make_move_iterator(udp6.begin()),
                                 std::make_move_iterator(udp6.end()));
    }

    {
        std::unordered_map<int, int> pidEstabCount;
        for (const auto& c : local.connections) {
            if (c.state == "ESTABLISHED" && c.pid > 0) {
                pidEstabCount[c.pid]++;
            }
        }
        if (!pidEstabCount.empty()) {
            auto best = std::max_element(pidEstabCount.begin(), pidEstabCount.end(),
                [](const auto& a, const auto& b) { return a.second < b.second; });
            local.topProcess = resolveProcessName(best->first);
        } else {
            local.topProcess = "N/A";
        }
    }

    float newHighUp   = highestUpload_;
    float newHighDown = highestDownload_;
    if (local.totalUploadRate   > newHighUp)   newHighUp   = local.totalUploadRate;
    if (local.totalDownloadRate > newHighDown)  newHighDown = local.totalDownloadRate;
    local.highestUpload   = newHighUp;
    local.highestDownload = newHighDown;

    {
        std::lock_guard<std::mutex> lock(mtx_);
        snap_            = std::move(local);
        highestUpload_   = newHighUp;
        highestDownload_ = newHighDown;
    }

    hasPrevSample_ = true;
    prevTime_      = now;
}

NetworkSnapshot LinuxNetwork::snapshot() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return snap_;
}

#endif
