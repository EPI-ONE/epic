// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "crc32.h"

#ifndef HAVE_MM_CRC32
// clang-format off
alignas(64) constexpr uint32_t crc32c_lut[256] = {
        0x00000000L, 0xF26B8303L, 0xE13B70F7L, 0x1350F3F4L, 0xC79A971FL,
        0x35F1141CL, 0x26A1E7E8L, 0xD4CA64EBL, 0x8AD958CFL, 0x78B2DBCCL,
        0x6BE22838L, 0x9989AB3BL, 0x4D43CFD0L, 0xBF284CD3L, 0xAC78BF27L,
        0x5E133C24L, 0x105EC76FL, 0xE235446CL, 0xF165B798L, 0x030E349BL,
        0xD7C45070L, 0x25AFD373L, 0x36FF2087L, 0xC494A384L, 0x9A879FA0L,
        0x68EC1CA3L, 0x7BBCEF57L, 0x89D76C54L, 0x5D1D08BFL, 0xAF768BBCL,
        0xBC267848L, 0x4E4DFB4BL, 0x20BD8EDEL, 0xD2D60DDDL, 0xC186FE29L,
        0x33ED7D2AL, 0xE72719C1L, 0x154C9AC2L, 0x061C6936L, 0xF477EA35L,
        0xAA64D611L, 0x580F5512L, 0x4B5FA6E6L, 0xB93425E5L, 0x6DFE410EL,
        0x9F95C20DL, 0x8CC531F9L, 0x7EAEB2FAL, 0x30E349B1L, 0xC288CAB2L,
        0xD1D83946L, 0x23B3BA45L, 0xF779DEAEL, 0x05125DADL, 0x1642AE59L,
        0xE4292D5AL, 0xBA3A117EL, 0x4851927DL, 0x5B016189L, 0xA96AE28AL,
        0x7DA08661L, 0x8FCB0562L, 0x9C9BF696L, 0x6EF07595L, 0x417B1DBCL,
        0xB3109EBFL, 0xA0406D4BL, 0x522BEE48L, 0x86E18AA3L, 0x748A09A0L,
        0x67DAFA54L, 0x95B17957L, 0xCBA24573L, 0x39C9C670L, 0x2A993584L,
        0xD8F2B687L, 0x0C38D26CL, 0xFE53516FL, 0xED03A29BL, 0x1F682198L,
        0x5125DAD3L, 0xA34E59D0L, 0xB01EAA24L, 0x42752927L, 0x96BF4DCCL,
        0x64D4CECFL, 0x77843D3BL, 0x85EFBE38L, 0xDBFC821CL, 0x2997011FL,
        0x3AC7F2EBL, 0xC8AC71E8L, 0x1C661503L, 0xEE0D9600L, 0xFD5D65F4L,
        0x0F36E6F7L, 0x61C69362L, 0x93AD1061L, 0x80FDE395L, 0x72966096L,
        0xA65C047DL, 0x5437877EL, 0x4767748AL, 0xB50CF789L, 0xEB1FCBADL,
        0x197448AEL, 0x0A24BB5AL, 0xF84F3859L, 0x2C855CB2L, 0xDEEEDFB1L,
        0xCDBE2C45L, 0x3FD5AF46L, 0x7198540DL, 0x83F3D70EL, 0x90A324FAL,
        0x62C8A7F9L, 0xB602C312L, 0x44694011L, 0x5739B3E5L, 0xA55230E6L,
        0xFB410CC2L, 0x092A8FC1L, 0x1A7A7C35L, 0xE811FF36L, 0x3CDB9BDDL,
        0xCEB018DEL, 0xDDE0EB2AL, 0x2F8B6829L, 0x82F63B78L, 0x709DB87BL,
        0x63CD4B8FL, 0x91A6C88CL, 0x456CAC67L, 0xB7072F64L, 0xA457DC90L,
        0x563C5F93L, 0x082F63B7L, 0xFA44E0B4L, 0xE9141340L, 0x1B7F9043L,
        0xCFB5F4A8L, 0x3DDE77ABL, 0x2E8E845FL, 0xDCE5075CL, 0x92A8FC17L,
        0x60C37F14L, 0x73938CE0L, 0x81F80FE3L, 0x55326B08L, 0xA759E80BL,
        0xB4091BFFL, 0x466298FCL, 0x1871A4D8L, 0xEA1A27DBL, 0xF94AD42FL,
        0x0B21572CL, 0xDFEB33C7L, 0x2D80B0C4L, 0x3ED04330L, 0xCCBBC033L,
        0xA24BB5A6L, 0x502036A5L, 0x4370C551L, 0xB11B4652L, 0x65D122B9L,
        0x97BAA1BAL, 0x84EA524EL, 0x7681D14DL, 0x2892ED69L, 0xDAF96E6AL,
        0xC9A99D9EL, 0x3BC21E9DL, 0xEF087A76L, 0x1D63F975L, 0x0E330A81L,
        0xFC588982L, 0xB21572C9L, 0x407EF1CAL, 0x532E023EL, 0xA145813DL,
        0x758FE5D6L, 0x87E466D5L, 0x94B49521L, 0x66DF1622L, 0x38CC2A06L,
        0xCAA7A905L, 0xD9F75AF1L, 0x2B9CD9F2L, 0xFF56BD19L, 0x0D3D3E1AL,
        0x1E6DCDEEL, 0xEC064EEDL, 0xC38D26C4L, 0x31E6A5C7L, 0x22B65633L,
        0xD0DDD530L, 0x0417B1DBL, 0xF67C32D8L, 0xE52CC12CL, 0x1747422FL,
        0x49547E0BL, 0xBB3FFD08L, 0xA86F0EFCL, 0x5A048DFFL, 0x8ECEE914L,
        0x7CA56A17L, 0x6FF599E3L, 0x9D9E1AE0L, 0xD3D3E1ABL, 0x21B862A8L,
        0x32E8915CL, 0xC083125FL, 0x144976B4L, 0xE622F5B7L, 0xF5720643L,
        0x07198540L, 0x590AB964L, 0xAB613A67L, 0xB831C993L, 0x4A5A4A90L,
        0x9E902E7BL, 0x6CFBAD78L, 0x7FAB5E8CL, 0x8DC0DD8FL, 0xE330A81AL,
        0x115B2B19L, 0x020BD8EDL, 0xF0605BEEL, 0x24AA3F05L, 0xD6C1BC06L,
        0xC5914FF2L, 0x37FACCF1L, 0x69E9F0D5L, 0x9B8273D6L, 0x88D28022L,
        0x7AB90321L, 0xAE7367CAL, 0x5C18E4C9L, 0x4F48173DL, 0xBD23943EL,
        0xF36E6F75L, 0x0105EC76L, 0x12551F82L, 0xE03E9C81L, 0x34F4F86AL,
        0xC69F7B69L, 0xD5CF889DL, 0x27A40B9EL, 0x79B737BAL, 0x8BDCB4B9L,
        0x988C474DL, 0x6AE7C44EL, 0xBE2DA0A5L, 0x4C4623A6L, 0x5F16D052L,
        0xAD7D5351L,
};
// clang-format on

/* manual computation of the crc32c checksum via a lookup table;
 * it is the slowest implementation out of the three; complexity = O(3.14N) */
uint32_t crc32c_lut_hword(uint8_t* buf, std::size_t length, uint32_t crc = -1) {
    while (length) {
        crc = crc32c_lut[(crc ^ *buf) & 0x000000FFL] ^ (crc >> 8);
        length -= 1;
        buf += 1;
    }

    return crc;
}

#else
#include <nmmintrin.h>

/* computation via the SSE42 crc32 instruction;
 * pretty fast and works for all sizes; complexity = O(0.33N) */
uint32_t crc32c_sse_qword(uint8_t* buf, std::size_t length, uint64_t crc = -1) {
    /* align buffer so that it's
     * length is a multiple of 8*/
    if (length & (1 << 2)) {
        crc = _mm_crc32_u32(crc, *((uint32_t*) buf));
        buf += 4;
    }

    if (length & (1 << 1)) {
        crc = _mm_crc32_u16(crc, *((uint16_t*) buf));
        buf += 2;
    }

    if (length & (1 << 0)) {
        crc = _mm_crc32_u8(crc, *buf);
        buf += 1;
    }

    /* decrease the length by the number of bytes that
     * have been processed in the alignment step */
    length &= ~7;

    while (length) {
        crc = _mm_crc32_u64(crc, *((uint64_t*) buf));
        length -= 8;
        buf += 8;
    }

    return crc;
}

#ifdef HAVE_MM_CLMULEPI
#include <emmintrin.h>
#include <wmmintrin.h>
/* every entry with an odd index is the second part of
 * the 128 bit value starting with the entry before it */
// clang-format off
alignas(16) constexpr uint64_t pclmulqdq_lut[84] = {
    0x14cd00bd6, 0x105ec76f0, 0x0ba4fc28e, 0x14cd00bd6, 0x1d82c63da,
    0x0f20c0dfe, 0x09e4addf8, 0x0ba4fc28e, 0x039d3b296, 0x1384aa63a,
    0x102f9b8a2, 0x1d82c63da, 0x14237f5e6, 0x01c291d04, 0x00d3b6092,
    0x09e4addf8, 0x0c96cfdc0, 0x0740eef02, 0x18266e456, 0x039d3b296,
    0x0daece73e, 0x0083a6eec, 0x0ab7aff2a, 0x102f9b8a2, 0x1248ea574,
    0x1c1733996, 0x083348832, 0x14237f5e6, 0x12c743124, 0x02ad91c30,
    0x0b9e02b86, 0x00d3b6092, 0x018b33a4e, 0x06992cea2, 0x1b331e26a,
    0x0c96cfdc0, 0x17d35ba46, 0x07e908048, 0x1bf2e8b8a, 0x18266e456,
    0x1a3e0968a, 0x11ed1f9d8, 0x0ce7f39f4, 0x0daece73e, 0x061d82e56,
    0x0f1d0f55e, 0x0d270f1a2, 0x0ab7aff2a, 0x1c3f5f66c, 0x0a87ab8a8,
    0x12ed0daac, 0x1248ea574, 0x065863b64, 0x08462d800, 0x11eef4f8e,
    0x083348832, 0x1ee54f54c, 0x071d111a8, 0x0b3e32c28, 0x12c743124,
    0x0064f7f26, 0x0ffd852c6, 0x0dd7e3b0c, 0x0b9e02b86, 0x0f285651c,
    0x0dcb17aa4, 0x010746f3c, 0x018b33a4e, 0x1c24afea4, 0x0f37c5aee,
    0x0271d9844, 0x1b331e26a, 0x08e766a0c, 0x06051d5a2, 0x093a5f730,
    0x17d35ba46, 0x06cb08e5c, 0x11d5ca20e, 0x06b749fb2, 0x1bf2e8b8a,
    0x1167f94f2, 0x021f3d99c, 0x0cec3662e, 0x1a3e0968a,
};
// clang-format on

/* computation is also facilitated via the crc32c instruction from SSE42
 * instruction set but here the crc32 instructions are pipelined which gives a
 * speed up of 0.25 cycles per byte when compared with the crc32c_sse_qword
 * method; the merging is done via carry less multiplcations which in turn done
 * using the PCLMULQDQ instructions; further note that this method only work
 * when the buffer has a lenght of 1024 bytes */
uint32_t crc32c_eq1024_pipelined_sse_qword(uint8_t* buf, uint64_t crc = -1) {
    uint64_t crc_a = crc;
    uint64_t crc_b = 0;
    uint64_t crc_c = 0;

    uint64_t* buf_a = (uint64_t*) (buf + 0 * 336);
    uint64_t* buf_b = (uint64_t*) (buf + 1 * 336);
    uint64_t* buf_c = (uint64_t*) (buf + 2 * 336);

    crc_a = _mm_crc32_u64(crc_a, *((uint64_t*) buf));

    for (std::size_t i = 1; i <= 42; ++i) {
        crc_a = _mm_crc32_u64(crc_a, buf_a[i]);
        crc_b = _mm_crc32_u64(crc_b, buf_b[i]);
        crc_c = _mm_crc32_u64(crc_c, buf_c[i]);
    }

    __m128i crc_a_xmm = _mm_loadu_si64(&crc_a);
    __m128i crc_b_xmm = _mm_loadu_si64(&crc_b);

    const __m128i factor = {0x0e417f38a, 0x08f158014};
    crc_a_xmm            = _mm_clmulepi64_si128(crc_a_xmm, factor, 0x00);
    crc_b_xmm            = _mm_clmulepi64_si128(crc_b_xmm, factor, 0x10);

    crc_c = _mm_crc32_u64(crc_c, *((uint64_t*) (buf + 1016)));

    _mm_storeu_si64(&crc_b, crc_b_xmm);
    crc_c ^= _mm_crc32_u64(0, crc_b);

    _mm_storeu_si64(&crc_a, crc_a_xmm);
    crc_c ^= _mm_crc32_u64(0, crc_a);

    return crc_c;
}

/* everything said for method above is true for this one except that this method
 * can operate on buffer lengths smaller or equal 1024 bytes but bigger than 24
 * bytes; note that for a buffer length of 1024 bytes the method above should be
 * preffered */
uint32_t crc32c_le1024_pipelined_sse_qword(uint8_t* buf, std::size_t length, uint64_t crc = -1) {
    std::size_t batch_length = (length * 2731) >> 16;

    uint64_t crc_a = crc;
    uint64_t crc_b = 0;
    uint64_t crc_c = 0;

    uint64_t* buf_a = (uint64_t*) (buf + 0 * 8 * batch_length);
    uint64_t* buf_b = (uint64_t*) (buf + 1 * 8 * batch_length);
    uint64_t* buf_c = (uint64_t*) (buf + 2 * 8 * batch_length);

    for (std::size_t i = 1; i < batch_length; ++i) {
        crc_a = _mm_crc32_u64(crc_a, *buf_a++);
        crc_b = _mm_crc32_u64(crc_b, *buf_b++);
        crc_c = _mm_crc32_u64(crc_c, *buf_c++);
    }

    crc_a = _mm_crc32_u64(crc_a, *buf_a);
    crc_b = _mm_crc32_u64(crc_b, *buf_b);

    __m128i factor_xmm = _mm_load_si128((__m128i*) (pclmulqdq_lut + (2 * batch_length - 2)));

    __m128i crc_a_xmm = _mm_loadu_si64(&crc_a);
    __m128i crc_b_xmm = _mm_loadu_si64(&crc_b);

    crc_a_xmm = _mm_clmulepi64_si128(crc_a_xmm, factor_xmm, 0x00);
    crc_b_xmm = _mm_clmulepi64_si128(crc_b_xmm, factor_xmm, 0x10);

    crc_a_xmm = _mm_xor_si128(crc_a_xmm, crc_b_xmm);
    _mm_storeu_si64(&crc_a, crc_a_xmm);

    crc_a ^= *buf_c;
    crc_c = _mm_crc32_u64(crc_c, crc_a);

    batch_length = length - 24 * batch_length;
    if (batch_length > 0) {
        crc_c = crc32c_sse_qword((uint8_t*) (buf_c + 1), batch_length, crc_c);
    }

    return crc_c;
}

/* combination of the above three methods to provide a method that handles
 * buffers of all sizes; complexity = O(0.19N) */
uint32_t crc32c_pcl(uint8_t* buf, std::size_t length, uint64_t crc = -1) {
    while (length >= 1024) {
        crc = crc32c_eq1024_pipelined_sse_qword(buf, crc);
        length -= 1024;
        buf += 1024;
    }

    if (length >= 24) {
        return crc32c_le1024_pipelined_sse_qword(buf, length, crc);
    } else if (length > 0) {
        return crc32c_sse_qword(buf, length, crc);
    } else {
        return crc;
    }
}
#endif
#endif

/* combination of all methods to have an crc32 implementation that works on all
 * architectues and can process buffers of arbitrary length*/
uint32_t crc32c(uint8_t *buf, std::size_t length, uint32_t crc) {
#ifdef HAVE_MM_CRC32
#ifdef HAVE_MM_CLMULEPI
    crc = crc32c_pcl(buf, length,crc);
#else
    crc = crc32c_sse_qword(buf, length, crc);
#endif
#else
    crc = crc32c_lut_hword(buf, length, crc);
#endif

    return ~crc;
}
