// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SECURE_MEM_H__
#define SECURE_MEM_H__

#include <stdlib.h>
#include "linux_syscall_support.h"

namespace playground {

class SecureMem {
 public:
  // Each thread is associated with two memory pages (i.e. 8192 bytes). This
  // memory is fully accessible by the trusted process, but in the trusted
  // thread and the sandboxed thread, the first page is only mapped PROT_READ,
  // and the second one is PROT_READ|PROT_WRITE.
  //
  // The first page can be modified by the trusted process and this is the
  // main mechanism how it communicates with the trusted thread. After each
  // update, it updates the "sequence" number. The trusted process must
  // check the "sequence" number has the expected value, and only then can
  // it trust the data in this page.
  typedef struct Args {
    union {
      struct {
        union {
          struct {
            struct Args* self;
            long         sequence;
            long         callType;
            long         syscallNum;
            void*        arg1;
            void*        arg2;
            void*        arg3;
            void*        arg4;
            void*        arg5;
            void*        arg6;

            // TODO(mseaborn): Remove these when the later offsets are
            // no longer hard-coded in the assembly code.
            #if defined(__x86_64__)
            void*        unused[15];
            #elif defined(__i386__)
            void*        unused[7];
            #else
            #error Unsupported target platform
            #endif

            // Used by clone() to set up data for the new thread.
            struct Args* newSecureMem;
            int          processFdPub;
            int          cloneFdPub;

            // Set to non-zero, if in debugging mode
            int          allowAllSystemCalls;

            // The most recent SysV SHM identifier returned by
            // shmget(IPC_PRIVATE)
            int          shmId;

            // The following entries make up the sandboxed thread's TLS
            long long    cookie;
            long long    threadId;
            long long    threadFdPub;
          } __attribute__((packed));
          char           header[512];
        };
        // Used for calls such as open() and stat().
        char             pathname[4096 - 512];
      } __attribute__((packed));
      char               securePage[4096];
    };
    union {
      struct {
        // This scratch space is used by the trusted thread to read parameters
        // for unrestricted system calls.
        int              tmpSyscallNum;
        void*            tmpArg1;
        void*            tmpArg2;
        void*            tmpArg3;
        void*            tmpArg4;
        void*            tmpArg5;
        void*            tmpArg6;
        void*            tmpReturnValue;

        // Scratch space used to return the result of a rdtsc instruction
        int              rdtscpEax;
        int              rdtscpEdx;
        int              rdtscpEcx;

        // We often have long sequences of calls to gettimeofday(). This is
        // needlessly expensive. Coalesce them into a single call.
        int              lastSyscallNum;
        int              gettimeofdayCounter;

        // For debugging purposes, we want to be able to log messages. This can
        // result in additional system calls. Make sure that we don't trigger
        // logging of those recursive calls.
        int              recursionLevel;

        // Computing the signal mask is expensive. Keep a cached copy.
        kernel_sigset_t  signalMask;

        // Keep track of whether we are in a SEGV handler
        int              inSegvHandler;
      } __attribute__((packed));
      char               scratchPage[4096];
    };
  } __attribute__((packed)) Args;

  // Allows the trusted process to check whether the parent process still
  // exists. If it doesn't, kill the trusted process.
  static void dieIfParentDied(int parentProc);

  // The trusted process received a system call that it intends to deny.
  static void abandonSystemCall(int fd, int err);

  // Acquires the syscall_mutex_ prior to making changes to the parameters in
  // the secure memory page. Used by calls such as exit(), clone(), open(),
  // socketcall(), and stat().
  // After locking the mutex, it is no longer valid to abandon the system
  // call!
  static void lockSystemCall(int parentProc, Args* mem);

  // Sends a system call to the trusted thread. If "locked" is true, the
  // caller must first call lockSystemCall() and must also provide
  // "parentProc". In locked mode, sendSystemCall() won't return until the
  // trusted thread has completed processing.
  // Use sparingly as it serializes the operation of the trusted process.
  static void sendSystemCall(int fd, bool locked, int parentProc, Args* mem,
                             int syscallNum) {
    sendSystemCallInternal(fd, locked, parentProc, mem, syscallNum);
  }
  template<class T1> static
  void sendSystemCall(int fd, bool locked, int parentProc, Args* mem,
                      int syscallNum, T1 arg1) {
    sendSystemCallInternal(fd, locked, parentProc, mem, syscallNum,
                           (void*)arg1);
  }
  template<class T1, class T2> static
  void sendSystemCall(int fd, bool locked, int parentProc, Args* mem,
                      int syscallNum, T1 arg1, T2 arg2) {
    sendSystemCallInternal(fd, locked, parentProc, mem, syscallNum,
                           (void*)arg1, (void*)arg2);
  }
  template<class T1, class T2, class T3> static
  void sendSystemCall(int fd, bool locked, int parentProc, Args* mem,
                      int syscallNum, T1 arg1, T2 arg2, T3 arg3) {
    sendSystemCallInternal(fd, locked, parentProc, mem, syscallNum,
                           (void*)arg1, (void*)arg2, (void*)arg3);
  }
  template<class T1, class T2, class T3, class T4> static
  void sendSystemCall(int fd, bool locked, int parentProc, Args* mem,
                      int syscallNum, T1 arg1, T2 arg2, T3 arg3, T4 arg4) {
    sendSystemCallInternal(fd, locked, parentProc, mem, syscallNum,
                           (void*)arg1, (void*)arg2, (void*)arg3, (void*)arg4);
  }
  template<class T1, class T2, class T3, class T4, class T5> static
  void sendSystemCall(int fd, bool locked, int parentProc, Args* mem,
                      int syscallNum, T1 arg1, T2 arg2, T3 arg3, T4 arg4,
                      T5 arg5) {
    sendSystemCallInternal(fd, locked, parentProc, mem, syscallNum,
                           (void*)arg1, (void*)arg2, (void*)arg3, (void*)arg4,
                           (void*)arg5);
  }
  template<class T1, class T2, class T3, class T4, class T5, class T6> static
  void sendSystemCall(int fd, bool locked, int parentProc, Args* mem,
                      int syscallNum, T1 arg1, T2 arg2, T3 arg3, T4 arg4,
                      T5 arg5, T6 arg6) {
    sendSystemCallInternal(fd, locked, parentProc, mem, syscallNum,
                           (void*)arg1, (void*)arg2, (void*)arg3, (void*)arg4,
                           (void*)arg5, (void*)arg6);
  }

 private:
  static void sendSystemCallInternal(int fd, bool locked, int parentProc,
                                     Args* mem, int syscallNum, void* arg1 = 0,
                                     void* arg2 = 0, void* arg3 = 0,
                                     void* arg4 = 0, void* arg5 = 0,
                                     void* arg6 = 0);
};

} // namespace

#endif // SECURE_MEM_H__
