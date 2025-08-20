/*
 * Copyright (c) 2025, horoni. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef KMEMHACK_H
#define KMEMHACK_H

#include <linux/types.h>

#define DEVICE_NAME "rAnd0m"
#define DEVICE_PATH "/dev/rAnd0m"

/* Macros for extracing flags */
#define R_FL(map) (map.flags >> 0) & 1
#define W_FL(map) (map.flags >> 1) & 1
#define X_FL(map) (map.flags >> 2) & 1

struct __maps_t {
  pid_t pid;
  size_t start, end;
  unsigned long flags;
  char name[64];
};

struct __memory_t {
  pid_t pid;
  unsigned long addr;
  size_t n;
  char *buf;
};

#define GET_MAPS _IOR('a', 'a', struct __maps_t *)
#define GET_MEM  _IOR('b', 'a', struct __memory_t *)
#define SET_MEM  _IOW('b', 'b', struct __memory_t *)

#endif // KMEMHACK_H
