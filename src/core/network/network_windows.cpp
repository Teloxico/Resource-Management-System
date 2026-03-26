/**
 * @file network_windows.cpp
 * @brief Windows network monitoring implementation using IP Helper APIs.
 */

#ifdef _WIN32

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <tcpmib.h>
#include <udpmib.h>
#include <psapi.h>

#include "network_windows.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "psapi.lib")

WindowsNetwork::WindowsNetwork()
    : prevTime_(std::chrono::steady_clock::now())
{
}

WindowsNetwork::~WindowsNetwork() = default;

std::string WindowsNetwork::resolveProcessName(int pid) {
    if (pid <= 0) return "System";

    auto it = processNameCache_.find(pid);
    if (it != processNameCache_.end()) return it->second;

    std::string name = "Unknown";
    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, static_cast<DWORD>(pid));
    if (hProc) {
        char buf[MAX_PATH] = {};
        if (GetModuleBaseNameA(hProc, nullptr, buf, MAX_PATH)) {
            name = buf;
        }
        CloseHandle(hProc);
    }
    processNameCache_[pid] = name;
    return name;
}

std::string WindowsNetwork::tcpStateToString(int state) {
    switch (state) {
        case  1: return "CLOSED";
        case  2: return "LISTEN";
        case  3: return "SYN_SENT";
        case  4: return "SYN_RCVD";
        case  5: return "ESTABLISHED";
        case  6: return "FIN_WAIT1";
        case  7: return "FIN_WAIT2";
        case  8: return "CLOSE_WAIT";
        case  9: return "CLOSING";
        case 10: return "LAST_ACK";
        case 11: return "TIME_WAIT";
        case 12: return "DELETE_TCB";
        default: return "UNKNOWN";
    }
}

void WindowsNetwork::update() {
    NetworkSnapshot local;
    auto now = std::chrono::steady_clock::now();
    double dtSec = std::chrono::duration<double>(now - prevTime_).count();
    if (dtSec <= 0.0) dtSec = 1.0;

    PMIB_IF_TABLE2 ifTable = nullptr;
    if (GetIfTable2(&ifTable) == NO_ERROR && ifTable) {

        ULONG aaSize = 0;
        GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, nullptr, &aaSize);

        std::vector<BYTE> aaBuf(aaSize);
        PIP_ADAPTER_ADDRESSES pAA = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(aaBuf.data());
        bool haveAA = (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX,
                                            nullptr, pAA, &aaSize) == NO_ERROR);

        struct AddrInfo { std::string ip; std::string mac; };
        std::unordered_map<ULONG, AddrInfo> addrMap;
        if (haveAA) {
            for (auto* a = pAA; a; a = a->Next) {
                AddrInfo ai;
                if (a->FirstUnicastAddress) {
                    sockaddr_in* sa = reinterpret_cast<sockaddr_in*>(
                        a->FirstUnicastAddress->Address.lpSockaddr);
                    char ipBuf[INET_ADDRSTRLEN] = {};
                    inet_ntop(AF_INET, &sa->sin_addr, ipBuf, sizeof(ipBuf));
                    ai.ip = ipBuf;
                }
                if (a->PhysicalAddressLength == 6) {
                    char macBuf[18] = {};
                    std::snprintf(macBuf, sizeof(macBuf),
                        "%02X:%02X:%02X:%02X:%02X:%02X",
                        a->PhysicalAddress[0], a->PhysicalAddress[1],
                        a->PhysicalAddress[2], a->PhysicalAddress[3],
                        a->PhysicalAddress[4], a->PhysicalAddress[5]);
                    ai.mac = macBuf;
                }
                addrMap[a->IfIndex] = ai;
            }
        }

        std::unordered_map<uint32_t, IfPrev> newPrev;

        for (ULONG i = 0; i < ifTable->NumEntries; ++i) {
            const MIB_IF_ROW2& row = ifTable->Table[i];

            if (row.Type == IF_TYPE_SOFTWARE_LOOPBACK) continue;

            uint32_t idx = static_cast<uint32_t>(row.InterfaceIndex);

            NetworkInterfaceInfo info;

            {
                char nameBuf[256] = {};
                WideCharToMultiByte(CP_UTF8, 0, row.Alias, -1,
                                    nameBuf, sizeof(nameBuf), nullptr, nullptr);
                info.name = nameBuf;
                if (info.name.empty()) {
                    WideCharToMultiByte(CP_UTF8, 0, row.Description, -1,
                                        nameBuf, sizeof(nameBuf), nullptr, nullptr);
                    info.name = nameBuf;
                }
            }

            info.isUp = (row.OperStatus == IfOperStatusUp);
            info.linkSpeedMbps = static_cast<float>(
                static_cast<double>(row.TransmitLinkSpeed) / 1'000'000.0);

            info.totalRecv  = row.InOctets;
            info.totalSent  = row.OutOctets;
            info.packetsIn  = row.InUcastPkts;
            info.packetsOut = row.OutUcastPkts;
            info.errorsIn   = row.InErrors;
            info.errorsOut  = row.OutErrors;
            info.dropsIn    = row.InDiscards;
            info.dropsOut   = row.OutDiscards;

            auto ait = addrMap.find(static_cast<ULONG>(idx));
            if (ait != addrMap.end()) {
                info.ipAddress  = ait->second.ip;
                info.macAddress = ait->second.mac;
            }

            if (hasPrevSample_) {
                auto pit = prevCounters_.find(idx);
                if (pit != prevCounters_.end()) {
                    uint64_t dIn  = (row.InOctets  >= pit->second.inOctets)
                                     ? (row.InOctets  - pit->second.inOctets)  : 0;
                    uint64_t dOut = (row.OutOctets >= pit->second.outOctets)
                                     ? (row.OutOctets - pit->second.outOctets) : 0;
                    info.downloadRate = static_cast<float>(dIn  / dtSec);
                    info.uploadRate   = static_cast<float>(dOut / dtSec);
                }
            }

            newPrev[idx] = { row.InOctets, row.OutOctets };

            local.totalBytesSent += info.totalSent;
            local.totalBytesRecv += info.totalRecv;
            local.totalUploadRate   += info.uploadRate;
            local.totalDownloadRate += info.downloadRate;

            local.interfaces.push_back(std::move(info));
        }

        FreeMibTable(ifTable);
        prevCounters_ = std::move(newPrev);
    }

    std::unordered_map<int, int> pidEstabCount;

    {
        DWORD tcpSize = 0;
        GetExtendedTcpTable(nullptr, &tcpSize, FALSE, AF_INET,
                            TCP_TABLE_OWNER_PID_ALL, 0);
        if (tcpSize > 0) {
            std::vector<BYTE> tcpBuf(tcpSize);
            if (GetExtendedTcpTable(tcpBuf.data(), &tcpSize, FALSE, AF_INET,
                                    TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
                auto* table = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(tcpBuf.data());
                for (DWORD i = 0; i < table->dwNumEntries; ++i) {
                    const auto& r = table->table[i];

                    TcpConnection conn;

                    {
                        in_addr la;
                        la.S_un.S_addr = r.dwLocalAddr;
                        char buf[INET_ADDRSTRLEN] = {};
                        inet_ntop(AF_INET, &la, buf, sizeof(buf));
                        conn.localAddr = buf;
                        conn.localPort = ntohs(static_cast<uint16_t>(r.dwLocalPort));
                    }
                    {
                        in_addr ra;
                        ra.S_un.S_addr = r.dwRemoteAddr;
                        char buf[INET_ADDRSTRLEN] = {};
                        inet_ntop(AF_INET, &ra, buf, sizeof(buf));
                        conn.remoteAddr = buf;
                        conn.remotePort = ntohs(static_cast<uint16_t>(r.dwRemotePort));
                    }

                    conn.state = tcpStateToString(r.dwState);
                    conn.pid   = static_cast<int>(r.dwOwningPid);
                    conn.processName = resolveProcessName(conn.pid);

                    if (r.dwState == MIB_TCP_STATE_ESTAB) {
                        pidEstabCount[conn.pid]++;
                    }

                    local.connections.push_back(std::move(conn));
                }
            }
        }
    }

    {
        DWORD tcpSize = 0;
        GetExtendedTcpTable(nullptr, &tcpSize, FALSE, AF_INET6,
                            TCP_TABLE_OWNER_PID_ALL, 0);
        if (tcpSize > 0) {
            std::vector<BYTE> tcpBuf(tcpSize);
            if (GetExtendedTcpTable(tcpBuf.data(), &tcpSize, FALSE, AF_INET6,
                                    TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
                auto* table = reinterpret_cast<MIB_TCP6TABLE_OWNER_PID*>(tcpBuf.data());
                for (DWORD i = 0; i < table->dwNumEntries; ++i) {
                    const auto& r = table->table[i];

                    TcpConnection conn;

                    {
                        char buf[INET6_ADDRSTRLEN] = {};
                        inet_ntop(AF_INET6, &r.ucLocalAddr, buf, sizeof(buf));
                        conn.localAddr = buf;
                        conn.localPort = ntohs(static_cast<uint16_t>(r.dwLocalPort));
                    }
                    {
                        char buf[INET6_ADDRSTRLEN] = {};
                        inet_ntop(AF_INET6, &r.ucRemoteAddr, buf, sizeof(buf));
                        conn.remoteAddr = buf;
                        conn.remotePort = ntohs(static_cast<uint16_t>(r.dwRemotePort));
                    }

                    conn.state = tcpStateToString(r.dwState);
                    conn.pid   = static_cast<int>(r.dwOwningPid);
                    conn.processName = resolveProcessName(conn.pid);

                    if (r.dwState == MIB_TCP_STATE_ESTAB) {
                        pidEstabCount[conn.pid]++;
                    }

                    local.connections.push_back(std::move(conn));
                }
            }
        }
    }

    {
        DWORD udpSize = 0;
        GetExtendedUdpTable(nullptr, &udpSize, FALSE, AF_INET,
                            UDP_TABLE_OWNER_PID, 0);
        if (udpSize > 0) {
            std::vector<BYTE> udpBuf(udpSize);
            if (GetExtendedUdpTable(udpBuf.data(), &udpSize, FALSE, AF_INET,
                                    UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
                auto* table = reinterpret_cast<MIB_UDPTABLE_OWNER_PID*>(udpBuf.data());
                for (DWORD i = 0; i < table->dwNumEntries; ++i) {
                    const auto& r = table->table[i];

                    TcpConnection conn;
                    {
                        in_addr la;
                        la.S_un.S_addr = r.dwLocalAddr;
                        char buf[INET_ADDRSTRLEN] = {};
                        inet_ntop(AF_INET, &la, buf, sizeof(buf));
                        conn.localAddr = buf;
                        conn.localPort = ntohs(static_cast<uint16_t>(r.dwLocalPort));
                    }
                    conn.state = "UDP";
                    conn.pid   = static_cast<int>(r.dwOwningPid);
                    conn.processName = resolveProcessName(conn.pid);

                    local.connections.push_back(std::move(conn));
                }
            }
        }
    }

    {
        DWORD udpSize = 0;
        GetExtendedUdpTable(nullptr, &udpSize, FALSE, AF_INET6,
                            UDP_TABLE_OWNER_PID, 0);
        if (udpSize > 0) {
            std::vector<BYTE> udpBuf(udpSize);
            if (GetExtendedUdpTable(udpBuf.data(), &udpSize, FALSE, AF_INET6,
                                    UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
                auto* table = reinterpret_cast<MIB_UDP6TABLE_OWNER_PID*>(udpBuf.data());
                for (DWORD i = 0; i < table->dwNumEntries; ++i) {
                    const auto& r = table->table[i];

                    TcpConnection conn;
                    {
                        char buf[INET6_ADDRSTRLEN] = {};
                        inet_ntop(AF_INET6, &r.ucLocalAddr, buf, sizeof(buf));
                        conn.localAddr = buf;
                        conn.localPort = ntohs(static_cast<uint16_t>(r.dwLocalPort));
                    }
                    conn.state = "UDP";
                    conn.pid   = static_cast<int>(r.dwOwningPid);
                    conn.processName = resolveProcessName(conn.pid);

                    local.connections.push_back(std::move(conn));
                }
            }
        }
    }

    if (!pidEstabCount.empty()) {
        auto best = std::max_element(pidEstabCount.begin(), pidEstabCount.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });
        local.topProcess = resolveProcessName(best->first);
    } else {
        local.topProcess = "N/A";
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

NetworkSnapshot WindowsNetwork::snapshot() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return snap_;
}

#endif
