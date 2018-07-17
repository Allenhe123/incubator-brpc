// Copyright (c) 2014 Baidu, Inc.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Authors: Ge,Jun (gejun@baidu.com)

#ifndef BRPC_SOCKET_MAP_H
#define BRPC_SOCKET_MAP_H

#include <vector>                                  // std::vector
#include "bvar/bvar.h"                             // bvar::PassiveStatus
#include "butil/containers/flat_map.h"             // FlatMap
#include "brpc/socket_id.h"                   // SockdetId
#include "brpc/options.pb.h"                  // ProtocolType
#include "brpc/ssl_option.h"                  // ChannelSSLOptions
#include "brpc/input_messenger.h"             // InputMessageHandler


namespace brpc {

// Global mapping from remote-side to out-going sockets created by Channels.

// The following fields uniquely define a Socket. In other word,
// Socket can't be shared between 2 different SocketMapKeys
struct SocketMapKey {
    SocketMapKey(const butil::EndPoint& pt,
                 ChannelSSLOptions ssl = ChannelSSLOptions(),
                 const Authenticator* auth2 = NULL)
            : peer(pt), ssl_options(ssl), auth(auth2)
    {}

    butil::EndPoint peer;
    ChannelSSLOptions ssl_options;
    const Authenticator* auth;
};

// Calculate an 128-bit hashcode for SocketMapKey
void ComputeSocketMapKeyChecksum(const SocketMapKey& key,
                                 unsigned char* checksum);

// Try to share the Socket to `key'. If the Socket does not exist, create one.
// The corresponding SocketId is written to `*id'. If this function returns
// successfully, SocketMapRemove() MUST be called when the Socket is not needed.
// Return 0 on success, -1 otherwise.
int SocketMapInsert(const SocketMapKey& key, SocketId* id);

// Find the SocketId associated with `key'.
// Return 0 on found, -1 otherwise.
int SocketMapFind(const SocketMapKey& key, SocketId* id);

// Called once when the Socket returned by SocketMapInsert() is not needed.
void SocketMapRemove(const SocketMapKey& key);

// Put all existing Sockets into `ids'
void SocketMapList(std::vector<SocketId>* ids);

// ======================================================================
// The underlying class that can be used otherwhere for mapping endpoints
// to sockets.

// SocketMap creates sockets on-demand by calling this object.
class SocketCreator {
public:
    virtual ~SocketCreator() {}
    virtual int CreateSocket(const SocketOptions& opt, SocketId* id) = 0;
};

struct SocketMapOptions {
    // Constructed with default options.
    SocketMapOptions();
    
    // For creating sockets by need. Owned and deleted by SocketMap.
    // Default: NULL (must be set by user).
    SocketCreator* socket_creator;

    // Initial size of the map (proper size reduces number of resizes)
    // Default: 1024
    size_t suggested_map_size;
    
    // Pooled connections without data transmission for so many seconds will
    // be closed. No effect for non-positive values.
    // If idle_timeout_second_dynamic is not NULL, use the dereferenced value
    // each time instead of idle_timeout_second.
    // Default: 0 (disabled)
    const int* idle_timeout_second_dynamic;
    int idle_timeout_second;

    // Defer close of connections for so many seconds even if the connection
    // is not used by anyone. Close immediately for non-positive values.
    // If defer_close_second_dynamic is not NULL, use the dereferenced value
    // each time instead of defer_close_second.
    // Default: 0 (disabled)
    const int* defer_close_second_dynamic;
    int defer_close_second;
};

// Share sockets to the same EndPoint.
class SocketMap {
public:
    SocketMap();
    ~SocketMap();
    int Init(const SocketMapOptions&);
    int Insert(const SocketMapKey& key, SocketId* id);
    void Remove(const SocketMapKey& key, SocketId expected_id);
    int Find(const SocketMapKey& key, SocketId* id);
    void List(std::vector<SocketId>* ids);
    void List(std::vector<butil::EndPoint>* pts);
    const SocketMapOptions& options() const { return _options; }

private:
    void RemoveInternal(const SocketMapKey& key, SocketId id,
                        bool remove_orphan);
    void ListOrphans(int64_t defer_us, std::vector<butil::EndPoint>* out);
    void WatchConnections();
    static void* RunWatchConnections(void*);
    void Print(std::ostream& os);
    static void PrintSocketMap(std::ostream& os, void* arg);

private:
    struct SingleConnection {
        int ref_count;
        Socket* socket;
        int64_t no_ref_us;
    };

    // Store checksum of SocketMapKey instead of itself in order to:
    // 1. Save precious space of key field in FlatMap
    // 2. Simplify equivalence logic between SocketMapKeys
    //    (regard the hash collision to be zero)
    struct SocketMapKeyChecksum {
        explicit SocketMapKeyChecksum(const SocketMapKey& key)
                : peer(key.peer) {
            ComputeSocketMapKeyChecksum(key, checksum);
        }

        butil::EndPoint peer;
        unsigned char checksum[16];

        inline bool operator==(const SocketMapKeyChecksum& rhs) const {
            return this->peer == rhs.peer
                    && memcmp(this->checksum, rhs.checksum, sizeof(checksum)) == 0;
        }
    };

    struct Checksum2Hash {
        std::size_t operator()(const SocketMapKeyChecksum& key) const {
            // Slice a subset of checksum over an evenly distributed hash
            // won't affect the overall balance
            std::size_t hash;
            memcpy(&hash, key.checksum, sizeof(hash));
            return hash;
        }
    };

    // TODO: When RpcChannels connecting to one EndPoint are frequently created
    //       and destroyed, a single map+mutex may become hot-spots.
    typedef butil::FlatMap<SocketMapKeyChecksum,
                           SingleConnection, Checksum2Hash> Map;
    SocketMapOptions _options;
    butil::Mutex _mutex;
    Map _map;
    bool _exposed_in_bvar;
    bvar::PassiveStatus<std::string>* _this_map_bvar;
    bool _has_close_idle_thread;
    bthread_t _close_idle_thread;
};

} // namespace brpc

#endif  // BRPC_SOCKET_MAP_H
