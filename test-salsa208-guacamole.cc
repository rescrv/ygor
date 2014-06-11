// Copyright (c) 2014, Robert Escriva
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright notice,
//       this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of Ygor nor the names of its contributors may be used
//       to endorse or promote products derived from this software without
//       specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#define __STDC_LIMIT_MACROS

// C
#include <assert.h>
#include <stdlib.h>
#include <string.h>

// Sodium
#include <sodium/crypto_stream_salsa208.h>

// e
#include <e/endian.h>

// Ygor
#include "guacamole.h"

unsigned char key[crypto_stream_salsa208_KEYBYTES];
unsigned char nonce[sizeof(uint64_t)];

void
salsa2guacamole(uint64_t number, uint32_t output[16])
{
    unsigned char* bytes = reinterpret_cast<unsigned char*>(output);
    e::pack64le(number, nonce);
    int rc = crypto_stream_salsa208(bytes, 64, nonce, key);
}

int
main(int argc, const char* argv[])
{
    memset(key, 0, sizeof(key));
    uint32_t salsa_out[16];
    uint32_t guacamole_out[16];

    for (uint64_t i = 0; i < 1UL<<24; ++i)
    {
        //salsa2guacamole(i, salsa_out);
        guacamole(i, guacamole_out);

        //for (unsigned i = 0; i < 16; ++i)
        //{
        //    assert(salsa_out[i] == guacamole_out[i]);
        //}
    }
}
