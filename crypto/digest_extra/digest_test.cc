/* Copyright (c) 2014, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <openssl/asn1.h>
#include <openssl/bytestring.h>
#include <openssl/crypto.h>
#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/md4.h>
#include <openssl/md5.h>
#include <openssl/nid.h>
#include <openssl/obj.h>
#include <openssl/sha.h>

#include "../fipsmodule/sha/internal.h"
#include "../internal.h"
#include "../test/test_util.h"


struct MD {
  // name is the name of the digest.
  const char* name;
  // md_func is the digest to test.
  const EVP_MD *(*func)(void);
  // one_shot_func is the convenience one-shot version of the
  // digest.
  uint8_t *(*one_shot_func)(const uint8_t *, size_t, uint8_t *);
};

static const MD md4 = { "MD4", &EVP_md4, nullptr };
static const MD md5 = { "MD5", &EVP_md5, &MD5 };
static const MD sha1 = { "SHA1", &EVP_sha1, &SHA1 };
static const MD sha224 = { "SHA224", &EVP_sha224, &SHA224 };
static const MD sha256 = { "SHA256", &EVP_sha256, &SHA256 };
static const MD sha384 = { "SHA384", &EVP_sha384, &SHA384 };
static const MD sha512 = { "SHA512", &EVP_sha512, &SHA512 };
static const MD sha512_256 = { "SHA512-256", &EVP_sha512_256, &SHA512_256 };
static const MD sha3_224 = { "SHA3-224", &EVP_sha3_224, &SHA3_224 };
static const MD sha3_256 = { "SHA3-256", &EVP_sha3_256, &SHA3_256 };
static const MD sha3_384 = { "SHA3-384", &EVP_sha3_384, &SHA3_384 };
static const MD sha3_512 = { "SHA3-512", &EVP_sha3_512, &SHA3_512 };
static const MD md5_sha1 = { "MD5-SHA1", &EVP_md5_sha1, nullptr };
static const MD blake2b256 = { "BLAKE2b-256", &EVP_blake2b256, nullptr };

struct DigestTestVector {
  // md is the digest to test.
  const MD &md;
  // input is a NUL-terminated string to hash.
  const char *input;
  // repeat is the number of times to repeat input.
  size_t repeat;
  // expected_hex is the expected digest in hexadecimal.
  const char *expected_hex;
};

static const DigestTestVector kTestVectors[] = {
    // MD4 tests, from RFC 1320. (crypto/md4 does not provide a
    // one-shot MD4 function.)
    {md4, "", 1, "31d6cfe0d16ae931b73c59d7e0c089c0"},
    {md4, "a", 1, "bde52cb31de33e46245e05fbdbd6fb24"},
    {md4, "abc", 1, "a448017aaf21d8525fc10ae87aa6729d"},
    {md4, "message digest", 1, "d9130a8164549fe818874806e1c7014b"},
    {md4, "abcdefghijklmnopqrstuvwxyz", 1, "d79e1c308aa5bbcdeea8ed63df412da9"},
    {md4, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", 1,
     "043f8582f241db351ce627e153e7f0e4"},
    {md4, "1234567890", 8, "e33b4ddc9c38f2199c3e7b164fcc0536"},

    // MD5 tests, from RFC 1321.
    {md5, "", 1, "d41d8cd98f00b204e9800998ecf8427e"},
    {md5, "a", 1, "0cc175b9c0f1b6a831c399e269772661"},
    {md5, "abc", 1, "900150983cd24fb0d6963f7d28e17f72"},
    {md5, "message digest", 1, "f96b697d7cb7938d525a2f31aaf161d0"},
    {md5, "abcdefghijklmnopqrstuvwxyz", 1, "c3fcd3d76192e4007dfb496cca67e13b"},
    {md5, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", 1,
     "d174ab98d277d9f5a5611c2c9f419d9f"},
    {md5, "1234567890", 8, "57edf4a22be3c955ac49da2e2107b67a"},

    // SHA-1 tests, from RFC 3174.
    {sha1, "abc", 1, "a9993e364706816aba3e25717850c26c9cd0d89d"},
    {sha1, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 1,
     "84983e441c3bd26ebaae4aa1f95129e5e54670f1"},
    {sha1, "a", 1000000, "34aa973cd4c4daa4f61eeb2bdbad27316534016f"},
    {sha1, "0123456701234567012345670123456701234567012345670123456701234567",
     10, "dea356a2cddd90c7a7ecedc5ebb563934f460452"},

    // SHA-224 tests, from RFC 3874.
    {sha224, "abc", 1,
     "23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7"},
    {sha224, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 1,
     "75388b16512776cc5dba5da1fd890150b0c6455cb4f58b1952522525"},
    {sha224, "a", 1000000,
     "20794655980c91d8bbb4c1ea97618a4bf03f42581948b2ee4ee7ad67"},

    // SHA-256 tests, from NIST.
    {sha256, "abc", 1,
     "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
    {sha256, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 1,
     "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},

    // SHA-384 tests, from NIST.
    {sha384, "abc", 1,
     "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed"
     "8086072ba1e7cc2358baeca134c825a7"},
    {sha384,
     "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
     "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
     1,
     "09330c33f71147e83d192fc782cd1b4753111b173b3b05d22fa08086e3b0f712"
     "fcc7c71a557e2db966c3e9fa91746039"},

    // SHA-512 tests, from NIST.
    {sha512, "abc", 1,
     "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
     "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f"},
    {sha512,
     "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
     "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
     1,
     "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018"
     "501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909"},

    // SHA-512-256 tests, from
    // https://csrc.nist.gov/csrc/media/projects/cryptographic-standards-and-guidelines/documents/examples/sha512_256.pdf
    {sha512_256, "abc", 1,
     "53048e2681941ef99b2e29b76b4c7dabe4c2d0c634fc6d46e0e2f13107e7af23"},
    {sha512_256,
     "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopj"
     "klmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
     1, "3928e184fb8690f840da3988121d31be65cb9d3ef83ee6146feac861e19b563a"},

     // SHA3-224 tests, from NIST.
    // http://csrc.nist.gov/groups/STM/cavp/secure-hashing.html
    {sha3_224, "", 1, "6b4e03423667dbb73b6e15454f0eb1abd4597f9a1b078e3f5b5a6bc7"},
    {sha3_224, "\x01", 1, "488286d9d32716e5881ea1ee51f36d3660d70f0db03b3f612ce9eda4"},
    {sha3_224, "\x69\xcb", 1, "94bd25c4cf6ca889126df37ddd9c36e6a9b28a4fe15cc3da6debcdd7"},
    {sha3_224, "\xbf\x58\x31", 1, "1bb36bebde5f3cb6d8e4672acf6eec8728f31a54dacc2560da2a00cc"},
    {sha3_224, "\xd1\x48\xce\x6d", 1, "0b521dac1efe292e20dfb585c8bff481899df72d59983315958391ba"},
    {sha3_224, "\x91\xc7\x10\x68\xf8", 1, "989f017709f50bd0230623c417f3daf194507f7b90a11127ba1638fa"},
    {sha3_224, "\xe7\x18\x3e\x4d\x89\xc9", 1, "650618f3b945c07de85b8478d69609647d5e2a432c6b15fbb3db91e4"},
    {sha3_224, "\xd8\x5e\x47\x0a\x7c\x69\x88", 1, "8a134c33c7abd673cd3d0c33956700760de980c5aee74c96e6ba08b2"},
    {sha3_224, "\xe4\xea\x2c\x16\x36\x6b\x80\xd6", 1, "7dd1a8e3ffe8c99cc547a69af14bd63b15ac26bd3d36b8a99513e89e"},
    {sha3_224, "\xe6\x5d\xe9\x1f\xdc\xb7\x60\x6f\x14\xdb\xcf\xc9\x4c\x9c\x94\xa5\x72\x40\xa6\xb2\xc3\x1e\xd4\x10\x34\x6c\x4d\xc0\x11\x52\x65\x59\xe4\x42\x96\xfc\x98\x8c\xc5\x89\xde\x2d\xc7\x13\xd0\xe8\x24\x92\xd4\x99\x1b\xd8\xc4\xc5\xe6\xc7\x4c\x75\x3f\xc0\x93\x45\x22\x5e\x1d\xb8\xd5\x65\xf0\xce\x26\xf5\xf5\xd9\xf4\x04\xa2\x8c\xf0\x0b\xd6\x55\xa5\xfe\x04\xed\xb6\x82\x94\x2d\x67\x5b\x86\x23\x5f\x23\x59\x65\xad\x42\x2b\xa5\x08\x1a\x21\x86\x5b\x82\x09\xae\x81\x76\x3e\x1c\x4c\x0c\xcc\xbc\xcd\xaa\xd5\x39\xcf\x77\x34\x13\xa5\x0f\x5f\xf1\x26\x7b\x92\x38\xf5\x60\x2a\xdc\x06\x76\x4f\x77\x5d\x3c", 
    1, "26ec9df54d9afe11710772bfbeccc83d9d0439d3530777c81b8ae6a3"},

    // SHA3-256 tests, from NIST.
    // http://csrc.nist.gov/groups/STM/cavp/secure-hashing.html
    {sha3_256, "", 1, "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a"},
    {sha3_256, "\xe9", 1, "f0d04dd1e6cfc29a4460d521796852f25d9ef8d28b44ee91ff5b759d72c1e6d6"},
    {sha3_256, "\xd4\x77", 1, "94279e8f5ccdf6e17f292b59698ab4e614dfe696a46c46da78305fc6a3146ab7"},
    {sha3_256, "\xb0\x53\xfa", 1, "9d0ff086cd0ec06a682c51c094dc73abdc492004292344bd41b82a60498ccfdb"},
    {sha3_256, "\xe7\x37\x21\x05", 1, "3a42b68ab079f28c4ca3c752296f279006c4fe78b1eb79d989777f051e4046ae"},
    {sha3_256, "\xe6\xfd\x42\x03\x7f\x80", 1, "2294f8d3834f24aa9037c431f8c233a66a57b23fa3de10530bbb6911f6e1850f"},
    {sha3_256, "\x37\xb4\x42\x38\x5e\x05\x38", 1, "cfa55031e716bbd7a83f2157513099e229a88891bb899d9ccd317191819998f8"},
    {sha3_256, "\x8b\xca\x93\x1c\x8a\x13\x2d\x2f", 1, "dbb8be5dec1d715bd117b24566dc3f24f2cc0c799795d0638d9537481ef1e03e"},
    {sha3_256, "\xfb\x8d\xfa\x3a\x13\x2f\x98\x13\xac", 1, "fd09b3501888445ffc8c3bb95d106440ceee469415fce1474743273094306e2e"},
    {sha3_256, "\x56\xea\x14\xd7\xfc\xb0\xdb\x74\x8f\xf6\x49\xaa\xa5\xd0\xaf\xdc\x23\x57\x52\x8a\x9a\xad\x60\x76\xd7\x3b\x28\x05\xb5\x3d\x89\xe7\x36\x81\xab\xfa\xd2\x6b\xee\x6c\x0f\x3d\x20\x21\x52\x95\xf3\x54\xf5\x38\xae\x80\x99\x0d\x22\x81\xbe\x6d\xe0\xf6\x91\x9a\xa9\xeb\x04\x8c\x26\xb5\x24\xf4\xd9\x1c\xa8\x7b\x54\xc0\xc5\x4a\xa9\xb5\x4a\xd0\x21\x71\xe8\xbf\x31\xe8\xd1\x58\xa9\xf5\x86\xe9\x2f\xfc\xe9\x94\xec\xce\x9a\x51\x85\xcc\x80\x36\x4d\x50\xa6\xf7\xb9\x48\x49\xa9\x14\x24\x2f\xcb\x73\xf3\x3a\x86\xec\xc8\x3c\x34\x03\x63\x0d\x20\x65\x0d\xdb\x8c\xd9\xc4", 
    1, "4beae3515ba35ec8cbd1d94567e22b0d7809c466abfbafe9610349597ba15b45"},

    // SHA3-384 tests, from NIST.
    // http://csrc.nist.gov/groups/STM/cavp/secure-hashing.html
    {sha3_384, "", 1, "0c63a75b845e4f7d01107d852e4c2485c51a50aaaa94fc61995e71bbee983a2ac3713831264adb47fb6bd1e058d5f004"},
    {sha3_384, "\x80", 1, "7541384852e10ff10d5fb6a7213a4a6c15ccc86d8bc1068ac04f69277142944f4ee50d91fdc56553db06b2f5039c8ab7"},
    {sha3_384, "\xfb\x52", 1, "d73a9d0e7f1802352ea54f3e062d3910577bf87edda48101de92a3de957e698b836085f5f10cab1de19fd0c906e48385"},
    {sha3_384, "\x6a\xb7\xd6", 1, "ea12d6d32d69ad2154a57e0e1be481a45add739ee7dd6e2a27e544b6c8b5ad122654bbf95134d567987156295d5e57db"},
    {sha3_384, "\x11\x58\x7d\xcb", 1, "cb6e6ce4a266d438ddd52867f2e183021be50223c7d57f8fdcaa18093a9d0126607df026c025bff40bc314af43fd8a08"},
    {sha3_384, "\x4d\x7f\xc6\xca\xe6", 1, "e570d463a010c71b78acd7f9790c78ce946e00cc54dae82bfc3833a10f0d8d35b03cbb4aa2f9ba4b27498807a397cd47"},
    {sha3_384, "\x5a\x66\x59\xe9\xf0\xe7", 1, "21b1f3f63b907f968821185a7fe30b16d47e1d6ee5b9c80be68947854de7a8ef4a03a6b2e4ec96abdd4fa29ab9796f28"},
    {sha3_384, "\x17\x51\x0e\xca\x2f\xe1\x1b", 1, "35fba6958b6c68eae8f2b5f5bdf5ebcc565252bc70f983548c2dfd5406f111a0a95b1bb9a639988c8d65da912d2c3ea2"},
    {sha3_384, "\xc4\x4a\x2c\x58\xc8\x4c\x39\x3a", 1, "60ad40f964d0edcf19281e415f7389968275ff613199a069c916a0ff7ef65503b740683162a622b913d43a46559e913c"},
    {sha3_384, "\x92\xc4\x1d\x34\xbd\x24\x9c\x18\x2a\xd4\xe1\x8e\x3b\x85\x67\x70\x76\x6f\x17\x57\x20\x96\x75\x02\x0d\x4c\x1c\xf7\xb6\xf7\x68\x6c\x8c\x14\x72\x67\x8c\x7c\x41\x25\x14\xe6\x3e\xb9\xf5\xae\xe9\xf5\xc9\xd5\xcb\x8d\x87\x48\xab\x7a\x54\x65\x05\x9d\x9c\xbb\xb8\xa5\x62\x11\xff\x32\xd4\xaa\xa2\x3a\x23\xc8\x6e\xad\x91\x6f\xe2\x54\xcc\x6b\x2b\xff\x7a\x95\x53\xdf\x15\x51\xb5\x31\xf9\x5b\xb4\x1c\xbb\xc4\xac\xdd\xbd\x37\x29\x21", 
    1, "71307eec1355f73e5b726ed9efa1129086af81364e30a291f684dfade693cc4bc3d6ffcb7f3b4012a21976ff9edcab61"},

    // SHA3-512 tests, from NIST.
    // http://csrc.nist.gov/groups/STM/cavp/secure-hashing.html
    {sha3_512, "", 1, "a69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a615b2123af1f5f94c11e3e9402c3ac558f500199d95b6d3e301758586281dcd26"},
    {sha3_512, "\xe5", 1, "150240baf95fb36f8ccb87a19a41767e7aed95125075a2b2dbba6e565e1ce8575f2b042b62e29a04e9440314a821c6224182964d8b557b16a492b3806f4c39c1"},
    {sha3_512, "\xef\x26", 1, "809b4124d2b174731db14585c253194c8619a68294c8c48947879316fef249b1575da81ab72aad8fae08d24ece75ca1be46d0634143705d79d2f5177856a0437"},
    {sha3_512, "\x37\xd5\x18", 1, "4aa96b1547e6402c0eee781acaa660797efe26ec00b4f2e0aec4a6d10688dd64cbd7f12b3b6c7f802e2096c041208b9289aec380d1a748fdfcd4128553d781e3"},
    {sha3_512, "\xfc\x7b\x8c\xda", 1, "58a5422d6b15eb1f223ebe4f4a5281bc6824d1599d979f4c6fe45695ca89014260b859a2d46ebf75f51ff204927932c79270dd7aef975657bb48fe09d8ea008e"},
    {sha3_512, "\x47\x75\xc8\x6b\x1c", 1, "ce96da8bcd6bc9d81419f0dd3308e3ef541bc7b030eee1339cf8b3c4e8420cd303180f8da77037c8c1ae375cab81ee475710923b9519adbddedb36db0c199f70"},
    {sha3_512, "\x71\xa9\x86\xd2\xf6\x62", 1, "def6aac2b08c98d56a0501a8cb93f5b47d6322daf99e03255457c303326395f765576930f8571d89c01e727cc79c2d4497f85c45691b554e20da810c2bc865ef"},
    {sha3_512, "\xec\x83\xd7\x07\xa1\x41\x4a", 1, "84fd3775bac5b87e550d03ec6fe4905cc60e851a4c33a61858d4e7d8a34d471f05008b9a1d63044445df5a9fce958cb012a6ac778ecf45104b0fcb979aa4692d"},
    {sha3_512, "\x0c\xe9\xf8\xc3\xa9\x90\xc2\x68\xf3\x4e\xfd\x9b\xef\xdb\x0f\x7c\x4e\xf8\x46\x6c\xfd\xb0\x11\x71\xf8\xde\x70\xdc\x5f\xef\xa9\x2a\xcb\xe9\x3d\x29\xe2\xac\x1a\x5c\x29\x79\x12\x9f\x1a\xb0\x8c\x0e\x77\xde\x79\x24\xdd\xf6\x8a\x20\x9c\xdf\xa0\xad\xc6\x2f\x85\xc1\x86\x37\xd9\xc6\xb3\x3f\x4f\xf8",
    1, "b018a20fcf831dde290e4fb18c56342efe138472cbe142da6b77eea4fce52588c04c808eb32912faa345245a850346faec46c3a16d39bd2e1ddb1816bc57d2da"},
    
    // MD5-SHA1 tests.
    {md5_sha1, "abc", 1,
     "900150983cd24fb0d6963f7d28e17f72a9993e364706816aba3e25717850c26c9cd0d89d"},

    // BLAKE2b-256 tests.
    {blake2b256, "abc", 1,
     "bddd813c634239723171ef3fee98579b94964e3bb1cb3e427262c8c068d52319"},
};

static void CompareDigest(const DigestTestVector *test,
                          const uint8_t *digest,
                          size_t digest_len) {
  EXPECT_EQ(test->expected_hex,
            EncodeHex(bssl::MakeConstSpan(digest, digest_len)));
}

static void TestDigest(const DigestTestVector *test) {
    // Test SHA3 by enabling |unstable_sha3_enabled_flag|, then disable it
    // |unstable_sha3_enabled_flag| is desabled by default
    // SHA3 negative tests are implemented in /fipsmodule/sha/sha3_test.cc
    if (strstr(test->md.name, "SHA3") != NULL) {
      EVP_MD_unstable_sha3_enable(true);
    }

    bssl::ScopedEVP_MD_CTX ctx;
    // Test the input provided.
    ASSERT_TRUE(EVP_DigestInit_ex(ctx.get(), test->md.func(), nullptr));
    for (size_t i = 0; i < test->repeat; i++) {
      ASSERT_TRUE(EVP_DigestUpdate(ctx.get(), test->input, strlen(test->input)));
    }
    std::unique_ptr<uint8_t[]> digest(new uint8_t[EVP_MD_size(test->md.func())]);
    unsigned digest_len;
    ASSERT_TRUE(EVP_DigestFinal_ex(ctx.get(), digest.get(), &digest_len));
    CompareDigest(test, digest.get(), digest_len);

    // Test the input one character at a time.
    ASSERT_TRUE(EVP_DigestInit_ex(ctx.get(), test->md.func(), nullptr));
    ASSERT_TRUE(EVP_DigestUpdate(ctx.get(), nullptr, 0));
    for (size_t i = 0; i < test->repeat; i++) {
      for (const char *p = test->input; *p; p++) {
        ASSERT_TRUE(EVP_DigestUpdate(ctx.get(), p, 1));
      }
    }
    ASSERT_TRUE(EVP_DigestFinal_ex(ctx.get(), digest.get(), &digest_len));
    EXPECT_EQ(EVP_MD_size(test->md.func()), digest_len);
    CompareDigest(test, digest.get(), digest_len);

    // Test with unaligned input.
    ASSERT_TRUE(EVP_DigestInit_ex(ctx.get(), test->md.func(), nullptr));
    std::vector<char> unaligned(strlen(test->input) + 1);
    char *ptr = unaligned.data();
    if ((reinterpret_cast<uintptr_t>(ptr) & 1) == 0) {
      ptr++;
    }
    OPENSSL_memcpy(ptr, test->input, strlen(test->input));
    for (size_t i = 0; i < test->repeat; i++) {
      ASSERT_TRUE(EVP_DigestUpdate(ctx.get(), ptr, strlen(test->input)));
    }
    ASSERT_TRUE(EVP_DigestFinal_ex(ctx.get(), digest.get(), &digest_len));
    CompareDigest(test, digest.get(), digest_len);

    // Make a copy of the digest in the initial state.
    ASSERT_TRUE(EVP_DigestInit_ex(ctx.get(), test->md.func(), nullptr));
    bssl::ScopedEVP_MD_CTX copy;
    ASSERT_TRUE(EVP_MD_CTX_copy_ex(copy.get(), ctx.get()));
    for (size_t i = 0; i < test->repeat; i++) {
      ASSERT_TRUE(EVP_DigestUpdate(copy.get(), test->input, strlen(test->input)));
    }
    ASSERT_TRUE(EVP_DigestFinal_ex(copy.get(), digest.get(), &digest_len));
    CompareDigest(test, digest.get(), digest_len);

    // Make a copy of the digest with half the input provided.
    size_t half = strlen(test->input) / 2;
    ASSERT_TRUE(EVP_DigestUpdate(ctx.get(), test->input, half));
    ASSERT_TRUE(EVP_MD_CTX_copy_ex(copy.get(), ctx.get()));
    ASSERT_TRUE(EVP_DigestUpdate(copy.get(), test->input + half,
                                strlen(test->input) - half));
    for (size_t i = 1; i < test->repeat; i++) {
      ASSERT_TRUE(EVP_DigestUpdate(copy.get(), test->input, strlen(test->input)));
    }
    ASSERT_TRUE(EVP_DigestFinal_ex(copy.get(), digest.get(), &digest_len));
    CompareDigest(test, digest.get(), digest_len);

    // Move the digest from the initial state.
    ASSERT_TRUE(EVP_DigestInit_ex(ctx.get(), test->md.func(), nullptr));
    copy = std::move(ctx);
    for (size_t i = 0; i < test->repeat; i++) {
      ASSERT_TRUE(EVP_DigestUpdate(copy.get(), test->input, strlen(test->input)));
    }
    ASSERT_TRUE(EVP_DigestFinal_ex(copy.get(), digest.get(), &digest_len));
    CompareDigest(test, digest.get(), digest_len);

    // Move the digest with half the input provided.
    ASSERT_TRUE(EVP_DigestInit_ex(ctx.get(), test->md.func(), nullptr));
    ASSERT_TRUE(EVP_DigestUpdate(ctx.get(), test->input, half));
    copy = std::move(ctx);
    ASSERT_TRUE(EVP_DigestUpdate(copy.get(), test->input + half,
                                strlen(test->input) - half));
    for (size_t i = 1; i < test->repeat; i++) {
      ASSERT_TRUE(EVP_DigestUpdate(copy.get(), test->input, strlen(test->input)));
    }
    ASSERT_TRUE(EVP_DigestFinal_ex(copy.get(), digest.get(), &digest_len));
    CompareDigest(test, digest.get(), digest_len);

    // Test the one-shot function.
    if (test->md.one_shot_func && test->repeat == 1) {
      uint8_t *out = test->md.one_shot_func((const uint8_t *)test->input,
                                            strlen(test->input), digest.get());
    // One-shot functions return their supplied buffers.
    EXPECT_EQ(digest.get(), out);
    CompareDigest(test, digest.get(), EVP_MD_size(test->md.func()));

    EVP_MD_unstable_sha3_enable(false);
  }
}

TEST(DigestTest, TestVectors) {
  for (size_t i = 0; i < OPENSSL_ARRAY_SIZE(kTestVectors); i++) {
    SCOPED_TRACE(i);
    TestDigest(&kTestVectors[i]);
  }
}

TEST(DigestTest, Getters) {
  EXPECT_EQ(EVP_sha512(), EVP_get_digestbyname("RSA-SHA512"));
  EXPECT_EQ(EVP_sha512(), EVP_get_digestbyname("sha512WithRSAEncryption"));
  EXPECT_EQ(nullptr, EVP_get_digestbyname("nonsense"));
  EXPECT_EQ(EVP_sha512(), EVP_get_digestbyname("SHA512"));
  EXPECT_EQ(EVP_sha512(), EVP_get_digestbyname("sha512"));

  EXPECT_EQ(EVP_sha512(), EVP_get_digestbynid(NID_sha512));
  EXPECT_EQ(nullptr, EVP_get_digestbynid(NID_sha512WithRSAEncryption));
  EXPECT_EQ(nullptr, EVP_get_digestbynid(NID_undef));

  bssl::UniquePtr<ASN1_OBJECT> obj(OBJ_txt2obj("1.3.14.3.2.26", 0));
  ASSERT_TRUE(obj);
  EXPECT_EQ(EVP_sha1(), EVP_get_digestbyobj(obj.get()));
  EXPECT_EQ(EVP_md5_sha1(), EVP_get_digestbyobj(OBJ_nid2obj(NID_md5_sha1)));
  EXPECT_EQ(EVP_sha1(), EVP_get_digestbyobj(OBJ_nid2obj(NID_sha1)));
}

TEST(DigestTest, ASN1) {
  bssl::ScopedCBB cbb;
  ASSERT_TRUE(CBB_init(cbb.get(), 0));
  EXPECT_FALSE(EVP_marshal_digest_algorithm(cbb.get(), EVP_md5_sha1()));

  static const uint8_t kSHA256[] = {0x30, 0x0d, 0x06, 0x09, 0x60,
                                    0x86, 0x48, 0x01, 0x65, 0x03,
                                    0x04, 0x02, 0x01, 0x05, 0x00};
  static const uint8_t kSHA256NoParam[] = {0x30, 0x0b, 0x06, 0x09, 0x60,
                                           0x86, 0x48, 0x01, 0x65, 0x03,
                                           0x04, 0x02, 0x01};
  static const uint8_t kSHA256GarbageParam[] = {
      0x30, 0x0e, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
      0x65, 0x03, 0x04, 0x02, 0x01, 0x02, 0x01, 0x2a};

  // Serialize SHA-256.
  cbb.Reset();
  ASSERT_TRUE(CBB_init(cbb.get(), 0));
  ASSERT_TRUE(EVP_marshal_digest_algorithm(cbb.get(), EVP_sha256()));
  uint8_t *der;
  size_t der_len;
  ASSERT_TRUE(CBB_finish(cbb.get(), &der, &der_len));
  bssl::UniquePtr<uint8_t> free_der(der);
  EXPECT_EQ(Bytes(kSHA256), Bytes(der, der_len));

  // Parse SHA-256.
  CBS cbs;
  CBS_init(&cbs, kSHA256, sizeof(kSHA256));
  EXPECT_EQ(EVP_sha256(), EVP_parse_digest_algorithm(&cbs));
  EXPECT_EQ(0u, CBS_len(&cbs));

  // Missing parameters are tolerated for compatibility.
  CBS_init(&cbs, kSHA256NoParam, sizeof(kSHA256NoParam));
  EXPECT_EQ(EVP_sha256(), EVP_parse_digest_algorithm(&cbs));
  EXPECT_EQ(0u, CBS_len(&cbs));

  // Garbage parameters are not.
  CBS_init(&cbs, kSHA256GarbageParam, sizeof(kSHA256GarbageParam));
  EXPECT_FALSE(EVP_parse_digest_algorithm(&cbs));
}

TEST(DigestTest, TransformBlocks) {
  uint8_t blocks[SHA256_CBLOCK * 10];
  for (size_t i = 0; i < sizeof(blocks); i++) {
    blocks[i] = i*3;
  }

  SHA256_CTX ctx1;
  SHA256_Init(&ctx1);
  SHA256_Update(&ctx1, blocks, sizeof(blocks));

  SHA256_CTX ctx2;
  SHA256_Init(&ctx2);
  SHA256_TransformBlocks(ctx2.h, blocks, sizeof(blocks) / SHA256_CBLOCK);

  EXPECT_TRUE(0 == OPENSSL_memcmp(ctx1.h, ctx2.h, sizeof(ctx1.h)));
}
