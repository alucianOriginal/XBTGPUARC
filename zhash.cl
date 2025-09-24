#define INPUT_SIZE     512
#define HASH_SIZE      32
#define WORK_ITEMS     131072
#define BUCKET_COUNT   32

__constant uint IV[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

inline uint rotr32(uint x, uint n) {
    return (x >> n) | (x << (32 - n));
}

// 🔁 Eine Hashrunde mit veränderbarem Startwert
inline void hash_core(uint m, uint round_offset, __global uchar* hash_out, int offset) {
    uint a = IV[0] ^ m;
    uint b = IV[1] ^ (m << 1);
    uint c = IV[2] ^ (m >> 1);
    uint d = IV[3] ^ (~m);

    for (int i = 0; i < 91; ++i) {
        a += b ^ (m ^ (i + round_offset)); a = rotr32(a, 16);
        b += c ^ a;                         b = rotr32(b, 12);
        c += d ^ b;                         c = rotr32(c, 8);
        d += a ^ c;                         d = rotr32(d, 7);
    }

    hash_out[offset +  0] = a & 0xFF;
    hash_out[offset +  1] = (a >> 8) & 0xFF;
    hash_out[offset +  2] = (a >> 16) & 0xFF;
    hash_out[offset +  3] = (a >> 24) & 0xFF;

    hash_out[offset +  4] = b & 0xFF;
    hash_out[offset +  5] = (b >> 8) & 0xFF;
    hash_out[offset +  6] = (b >> 16) & 0xFF;
    hash_out[offset +  7] = (b >> 24) & 0xFF;

    hash_out[offset +  8] = c & 0xFF;
    hash_out[offset +  9] = (c >> 8) & 0xFF;
    hash_out[offset + 10] = (c >> 16) & 0xFF;
    hash_out[offset + 11] = (c >> 24) & 0xFF;

    hash_out[offset + 12] = d & 0xFF;
    hash_out[offset + 13] = (d >> 8) & 0xFF;
    hash_out[offset + 14] = (d >> 16) & 0xFF;
    hash_out[offset + 15] = (d >> 24) & 0xFF;
}

// Hauptfunktion: erzeugt zwei 16-Byte-Runden für zusammen 32 Byte
inline void hash_rounds(__global const uchar* input, uint gid, __global uchar* hash_out) {
    uint m = 0;
    for (int i = 0; i < 4; ++i)
        m |= ((uint)input[gid * INPUT_SIZE + i]) << (i * 8);

    // Erste Hälfte: direkt aus m
    hash_core(m, 0, hash_out, gid * HASH_SIZE);

    // Zweite Hälfte: aus m XOR 0xa5a5a5a5 (Variation)
    hash_core(m ^ 0xa5a5a5a5, 91, hash_out, gid * HASH_SIZE + 16);
}

// Kernel
__kernel void zhash_144_5(
    __global const uchar* input,
    __global uchar* output,
    __global uint* solution_indexes
) {
    int gid = get_global_id(0);

    hash_rounds(input, gid, output);

    __global uchar* my_hash = &output[gid * HASH_SIZE];
    uint bucket = my_hash[0] >> 3;

    for (int i = 0; i < gid; ++i) {
        __global uchar* other = &output[i * HASH_SIZE];
        uint other_bucket = other[0] >> 3;

        if (bucket == other_bucket) {
            solution_indexes[gid * 2]     = i;
            solution_indexes[gid * 2 + 1] = gid;
            break;
        }
    }
}

