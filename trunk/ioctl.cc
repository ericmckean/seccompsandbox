// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

long Sandbox::sandbox_ioctl(int d, int req, void *arg) {
  long long tm;
  Debug::syscall(&tm, __NR_ioctl, "Executing handler");
  struct {
    int       sysnum;
    long long cookie;
    IOCtl     ioctl_req;
  } __attribute__((packed)) request;
  request.sysnum        = __NR_ioctl;
  request.cookie        = cookie();
  request.ioctl_req.d   = d;
  request.ioctl_req.req = req;
  request.ioctl_req.arg = arg;

  long rc;
  SysCalls sys;
  if (write(sys, processFdPub(), &request, sizeof(request)) !=
      sizeof(request) ||
      read(sys, threadFdPub(), &rc, sizeof(rc)) != sizeof(rc)) {
    die("Failed to forward ioctl() request [sandbox]");
  }
  Debug::elapsed(tm, __NR_ioctl);
  return rc;
}

bool Sandbox::process_ioctl(int parentMapsFd, int sandboxFd, int threadFdPub,
                            int threadFd, SecureMem::Args* mem) {
  // Read request
  IOCtl ioctl_req;
  SysCalls sys;
  if (read(sys, sandboxFd, &ioctl_req, sizeof(ioctl_req)) !=sizeof(ioctl_req)){
    die("Failed to read parameters for ioctl() [process]");
  }
  int rc = -EINVAL;
  switch (ioctl_req.req) {
    case TCGETS:
    case TIOCGWINSZ:
      SecureMem::sendSystemCall(threadFdPub, false, -1, mem, __NR_ioctl,
                                ioctl_req.d, ioctl_req.req, ioctl_req.arg);
      return true;
    default:
      if (Debug::isEnabled()) {
        char buf[80];
        sprintf(buf, "Unsupported ioctl: 0x%04X\n", ioctl_req.req);
        Debug::message(buf);
      }
      SecureMem::abandonSystemCall(threadFd, rc);
      return false;
  }
}

} // namespace
