/*
 * This file is part of the MAVLink Router project
 *
 * Copyright (C) 2019  Pinecone Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <common/log.h>
#include "mainloop.h"
#include "controller.h"

#define READ_BUF_SIZE 256
#define IP_ADDR_LEN_MAX 15
#define SOCKET_NAME "#routercontroller"
#define ADD_ENDPOINT_COMMAND "ADDENDPOINT"
#define REMOVE_ENDPOINT_COMMAND "REMOVEENDPOINT"
#define ACK_OK "OK"
#define ACK_FAIL "FAIL"
#define COMMAND_LEN_MAX 256

enum {
    COMMAND_ID_ADD_ENDPOINT,
    COMMAND_ID_REMOVE_ENDPOINT
};

Controller Controller::_instance{};
Controller::Controller()
{
}

void Controller::open()
{
    if(_instance._open_socket() >= 0) {
        Mainloop::get_instance().add_fd(_instance.fd, &_instance, EPOLLIN);
    }
}

int Controller::handle_read()
{
    char buf[READ_BUF_SIZE] = {0};
    struct sockaddr_un src_addr;
    socklen_t addrlen = sizeof(src_addr);
   
    bzero((void*)&src_addr, sizeof(src_addr));
    ssize_t r = ::recvfrom(fd, buf, READ_BUF_SIZE, 0, (struct sockaddr*)&src_addr, &addrlen);

    if (r == -1) {
        if(errno != EAGAIN) {
            log_error("controller: _read_msg receive from fd error %d", errno);
        }
        return 0;
    }
    if (r == 0) {
        log_error("controller: _read_msg receive empty data");
        return 0;
    }

    if(_parse_msg(buf, r)) {
        ::strcpy(buf, ACK_OK);
    } else {
        ::strcpy(buf, ACK_FAIL);
        log_error("controller: _read_msg parse msg failure");
    }
    ::sendto(fd, buf, ::strlen(buf), 0, (struct sockaddr*)&src_addr, addrlen);
    return 0;
}

int Controller::_open_socket()
{
    int flags = 0;
    struct sockaddr_un addr;
    socklen_t addr_len;

    bzero((void*)&addr, sizeof(addr));
    fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) {
        log_error("controller: opening datagram socket failure");
        return -1;
    }
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_NAME);
    addr.sun_path[0] = 0;
	addr_len = strlen(SOCKET_NAME) + offsetof(struct sockaddr_un, sun_path);

    if (bind(fd, (struct sockaddr *) &addr, addr_len)) {
        log_error("controller: binding to server socket failure %d", errno);
        goto fail;
    }
    if ((flags = fcntl(fd, F_GETFL, 0) == -1)) {
        log_error("controller: Error getfl for fd");
        goto fail;
    }
    if (fcntl(fd, F_SETFL, O_NONBLOCK | flags) < 0) {
        log_error("controller: Error setting socket fd as non-blocking");
        goto fail;
    }
    log_info("controller: opening datagram socket successfully");

    return fd;
 
fail:
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
    return -1;
}

bool Controller::_parse_msg(char *msg, ssize_t len)
{
    int r = 0;
	char* str = NULL;
    char* ipaddress = NULL;
    int port = 0;
	int command = -1;
    char seps[] = ":";
    char* p = (char*)msg;

    if(len > COMMAND_LEN_MAX || strlen(p) > COMMAND_LEN_MAX) {
        log_error("controller: invalid message received");
        return false;
    }
    str = strtok(p, seps); 
    if (str != NULL) {
        if(!strcmp(str, ADD_ENDPOINT_COMMAND)) {
            command = COMMAND_ID_ADD_ENDPOINT;
        } else if (!strcmp(str, REMOVE_ENDPOINT_COMMAND)) {
            command = COMMAND_ID_REMOVE_ENDPOINT;
        }
        if (command != -1) {
            ipaddress = strtok(NULL, seps);
            if (ipaddress != NULL) {
                str = strtok(NULL, seps);
                if (str != NULL) {
                    port = atoi(str);
                    if(port > 0) {
                        if (command == COMMAND_ID_ADD_ENDPOINT) {
                            return _add_dynamic_endpoint(ipaddress, port);
                        } else if (command == COMMAND_ID_REMOVE_ENDPOINT) {
                            return _remove_dynamic_endpoint(ipaddress, port);
                        }
                    }
                }
            }
        }
    }
    log_error("controller: invalid command received: %s", p);
    return false;
}

bool Controller::_add_dynamic_endpoint(const char *ipaddr, unsigned long port)
{
    bool ret;

    std::unique_ptr<UdpEndpoint> udp_endpoint{new UdpEndpoint{}};

    if (udp_endpoint->open(ipaddr, port, false) < 0) {
        log_error("controller: could not open %s:%ld", ipaddr, port);
        udp_endpoint.release();

        return false;
    }
    ret = Mainloop::get_instance().add_udp_endpoint(udp_endpoint.get());
    if (!ret) {
      log_error("controller: add udp endpoint failed");
      udp_endpoint->close();

      return false;
    }

    log_info("controller: add endpoint %s:%ld", ipaddr, port);
    udp_endpoint.release();
    return true;
}

bool Controller::_remove_dynamic_endpoint(const char *ipaddr, unsigned long port)
{
    return Mainloop::get_instance().remove_udp_endpoint(ipaddr, port);
}
