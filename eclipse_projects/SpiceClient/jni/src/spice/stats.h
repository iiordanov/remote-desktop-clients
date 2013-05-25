/*
   Copyright (C) 2009 Red Hat, Inc.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above copyright
         notice, this list of conditions and the following disclaimer in
         the documentation and/or other materials provided with the
         distribution.
       * Neither the name of the copyright holder nor the names of its
         contributors may be used to endorse or promote products derived
         from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS "AS
   IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
   TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _H_SPICE_STATS
#define _H_SPICE_STATS

#include <spice/types.h>

#define SPICE_STAT_SHM_NAME "/spice.%u"
#define SPICE_STAT_NODE_NAME_MAX 20
#define SPICE_STAT_MAGIC (*(uint32_t*)"STAT")
#define SPICE_STAT_VERSION 1

enum {
    SPICE_STAT_NODE_FLAG_ENABLED = (1 << 0),
    SPICE_STAT_NODE_FLAG_VISIBLE = (1 << 1),
    SPICE_STAT_NODE_FLAG_VALUE = (1 << 2),
};

#define SPICE_STAT_NODE_MASK_SHOW (SPICE_STAT_NODE_FLAG_ENABLED | SPICE_STAT_NODE_FLAG_VISIBLE)
#define SPICE_STAT_NODE_MASK_SHOW_VALUE (SPICE_STAT_NODE_MASK_SHOW | SPICE_STAT_NODE_FLAG_VALUE)

typedef struct SpiceStatNode {
    uint64_t value;
    uint32_t flags;
    uint32_t next_sibling_index;
    uint32_t first_child_index;
    char name[SPICE_STAT_NODE_NAME_MAX];
} SpiceStatNode;

typedef struct SpiceStat {
    uint32_t magic;
    uint32_t version;
    uint32_t generation;
    uint32_t num_of_nodes;
    uint32_t root_index;
    SpiceStatNode nodes[];
} SpiceStat;

#endif /* _H_SPICE_STATS */
