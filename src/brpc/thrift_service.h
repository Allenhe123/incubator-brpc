// Copyright (c) 2015 Baidu, Inc.
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

// Authors: wangxuefeng (wangxuefeng@didichuxing.com)

#ifndef BRPC_THRIFT_SERVICE_H
#define BRPC_THRIFT_SERVICE_H

#include "brpc/controller.h"                        // Controller
#include "brpc/thrift_message.h"                    // ThriftFramedMessage
#include "brpc/describable.h"

namespace brpc {

class Socket;
class Server;
class MethodStatus;
class StatusService;
namespace policy {
class ThriftClosure;
void ProcessThriftRequest(InputMessageBase* msg_base);
}

// Inherit this class to let brpc server understands thrift_binary requests.
class ThriftService : public Describable {
public:
    ThriftService();
    virtual ~ThriftService();

    // Implement this method to handle thrift_binary requests. Notice that this
    // method can be called with a failed Controller(something wrong with the
    // request before calling this method), in which case the implemenetation
    // shall send specific response with error information back to client.
    // Parameters:
    //   controller  per-rpc settings.
    //   request     The thrift_binary request received.
    //   response    The thrift_binary response that you should fill in.
    //   done        You must call done->Run() to end the processing.
    virtual void ProcessThriftFramedRequest(
        Controller* controller,
        ThriftFramedMessage* request,
        ThriftFramedMessage* response,
        ::google::protobuf::Closure* done) = 0;

    // Put descriptions into the stream.
    void Describe(std::ostream &os, const DescribeOptions&) const;

private:
DISALLOW_COPY_AND_ASSIGN(ThriftService);
friend class policy::ThriftClosure;
friend void policy::ProcessThriftRequest(InputMessageBase* msg_base);
friend class StatusService;
friend class Server;

private:
    void Expose(const butil::StringPiece& prefix);
    
    MethodStatus* _status;
};

} // namespace brpc

#endif // BRPC_THRIFT_SERVICE_H
