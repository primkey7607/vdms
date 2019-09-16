/**
 * @section LICENSE
 *
 * The MIT License
 *
 * @copyright Copyright (c) 2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <string>
#include <unistd.h>
#include <cstdlib>
#include <assert.h>

#include <netdb.h>

#include "Connection.h"

using namespace comm;

Connection::Connection():
    _socket_fd(-1)
{
}

Connection::Connection(int socket_fd):
    _socket_fd(socket_fd)
{
}

Connection::Connection(Connection &&c)
{
    _socket_fd = c._socket_fd;
    c._socket_fd = -1;
}

Connection& Connection::operator=(Connection &&c)
{
    _socket_fd = c._socket_fd;
    c._socket_fd = -1;
    return *this;
}

Connection::~Connection()
{
    if (_socket_fd != -1) {
        ::close(_socket_fd);
        _socket_fd = -1;
    }
}

void Connection::shutdown()
{
    ::shutdown(_socket_fd, SHUT_RDWR);
}

void Connection::send_message(const uint8_t *data, uint32_t size)
{
    if (size > MAX_BUFFER_SIZE) {
		fprintf(stderr, ("Max Buffer Size: " + std::to_string(MAX_BUFFER_SIZE)).c_str());
        throw ExceptionComm(InvalidMessageSize);
    }

    // We need MSG_NOSIGNAL so we don't get SIGPIPE, and we can throw.
    int ret = ::send(_socket_fd, (const char*)&size, sizeof(size), MSG_NOSIGNAL);

    if (ret != sizeof(size)) {
        throw ExceptionComm(WriteFail);
    }

    int bytes_sent = 0;
    while (bytes_sent < size) {
        // We need MSG_NOSIGNAL so we don't get SIGPIPE, and we can throw.
        int ret = ::send(_socket_fd,
                        (const char*)data + bytes_sent,
                        size - bytes_sent, MSG_NOSIGNAL);
        if (ret < 0) {
            throw ExceptionComm(WriteFail);
        }

        bytes_sent += ret;
    }
}

const std::basic_string<uint8_t>& Connection::recv_message()
{
    uint32_t recv_message_size;

    auto recv_and_check = [this](void* buffer, uint32_t size, int flags)
    {
        size_t bytes_recv = 0;

        while (bytes_recv < size) {

            int ret = ::recv(_socket_fd, (void*)((char*)buffer + bytes_recv),
                    size - bytes_recv, flags);

            if (ret < 0) {
                throw ExceptionComm(ReadFail);
            }
            // When a stream socket peer has performed an orderly shutdown, the
            // return value will be 0 (the traditional "end-of-file" return).
            else if (ret == 0) {
                throw ExceptionComm(ConnectionShutDown);
            }

            bytes_recv += ret;
        }

        return bytes_recv;
    };

    size_t bytes_recv = recv_and_check(&recv_message_size, sizeof(uint32_t),
                                       MSG_WAITALL);

    if (bytes_recv != sizeof(recv_message_size)) {
        throw ExceptionComm(ReadFail);
    }

    if (recv_message_size > MAX_BUFFER_SIZE) {
        throw ExceptionComm(InvalidMessageSize);
    }

    buffer_str.resize(recv_message_size);

    uint8_t *buffer = (uint8_t*) buffer_str.data();
    bytes_recv = recv_and_check(buffer, recv_message_size, MSG_WAITALL);

    if (recv_message_size != bytes_recv) {
        throw ExceptionComm(ReadFail);
    }

    if (recv_message_size != buffer_str.size()) {
        throw ExceptionComm(ReadFail);
    }

    return buffer_str;
}
