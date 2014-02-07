/*
   Copyright (C) 2010 Red Hat, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
#ifndef _H_DEMARSHAL
#define _H_DEMARSHAL

#include <stddef.h>
#include <spice/macros.h>

SPICE_BEGIN_DECLS

typedef void (*message_destructor_t)(uint8_t *message);
typedef uint8_t * (*spice_parse_channel_func_t)(uint8_t *message_start, uint8_t *message_end, uint16_t message_type, int minor,
						size_t *size_out, message_destructor_t *free_message);

spice_parse_channel_func_t spice_get_server_channel_parser(uint32_t channel, unsigned int *max_message_type);
spice_parse_channel_func_t spice_get_server_channel_parser1(uint32_t channel, unsigned int *max_message_type);

SPICE_END_DECLS

#endif

