QT       -= gui
QT       -= core

TARGET = jincrypto
TEMPLATE = lib

CONFIG += staticlib

include( ../_common.pri )

DEFINES += LTC_NO_ROLC
# LTC_NO_ROLC  the asm part can't be compiled.
linux{
    DEFINES += LTC_PTHREAD
}
#INCLUDEPATH += ./crypto
INCLUDEPATH += ./headers
#INCLUDEPATH += ../tommath
#LIBS += -ltommath

HEADERS  += \
    headers/tomcrypt.h \
    headers/tomcrypt_argchk.h \
    headers/tomcrypt_cfg.h \
    headers/tomcrypt_cipher.h \
    headers/tomcrypt_custom.h \
    headers/tomcrypt_hash.h \
    headers/tomcrypt_mac.h \
    headers/tomcrypt_macros.h \
    headers/tomcrypt_math.h \
    headers/tomcrypt_misc.h \
    headers/tomcrypt_pk.h \
    headers/tomcrypt_pkcs.h \
    headers/tomcrypt_prng.h \
    jincrypt.h \
    headers/tommath.h \
    headers/tommath_class.h \
    headers/tommath_superclass.h


SOURCES += \
    hashes/md5.c \
    hashes/sha1.c \
    ciphers/des.c \
    ciphers/rc5.c \
    mac/hmac/hmac_done.c \
    mac/hmac/hmac_file.c \
    mac/hmac/hmac_init.c \
    mac/hmac/hmac_memory.c \
    mac/hmac/hmac_memory_multi.c \
    mac/hmac/hmac_process.c \
    jincrypt/jinbase32.c \
    jincrypt/jinurlencode.c \
    jincrypt/jinhash.c \
    jincrypt/jinbencode.c \
    misc/base64/base64_decode.c \
    misc/base64/base64_encode.c \
    misc/burn_stack.c \
    misc/crypt/crypt_argchk.c \
    misc/crypt/crypt.c \
    misc/crypt/crypt_cipher_descriptor.c \
    misc/crypt/crypt_cipher_is_valid.c \
    misc/crypt/crypt_find_cipher.c \
    misc/crypt/crypt_find_cipher_any.c \
    misc/crypt/crypt_find_cipher_id.c \
    misc/crypt/crypt_find_hash.c \
    misc/crypt/crypt_find_hash_any.c \
    misc/crypt/crypt_find_hash_id.c \
    misc/crypt/crypt_find_hash_oid.c \
    misc/crypt/crypt_find_prng.c \
    misc/crypt/crypt_fsa.c \
    misc/crypt/crypt_hash_descriptor.c \
    misc/crypt/crypt_hash_is_valid.c \
    misc/crypt/crypt_ltc_mp_descriptor.c \
    misc/crypt/crypt_prng_descriptor.c \
    misc/crypt/crypt_prng_is_valid.c \
    misc/crypt/crypt_register_cipher.c \
    misc/crypt/crypt_register_hash.c \
    misc/crypt/crypt_register_prng.c \
    misc/crypt/crypt_unregister_cipher.c \
    misc/crypt/crypt_unregister_hash.c \
    misc/crypt/crypt_unregister_prng.c \
    misc/error_to_string.c \
    misc/zeromem.c \
    hashes/helper/hash_file.c \
    hashes/helper/hash_filehandle.c \
    hashes/helper/hash_memory.c \
    hashes/helper/hash_memory_multi.c \
    pk/ecc/ecc.c \
    pk/ecc/ecc_ansi_x963_export.c \
    pk/ecc/ecc_ansi_x963_import.c \
    pk/ecc/ecc_decrypt_key.c \
    pk/ecc/ecc_encrypt_key.c \
    pk/ecc/ecc_export.c \
    pk/ecc/ecc_free.c \
    pk/ecc/ecc_get_size.c \
    pk/ecc/ecc_import.c \
    pk/ecc/ecc_make_key.c \
    pk/ecc/ecc_shared_secret.c \
    pk/ecc/ecc_sign_hash.c \
    pk/ecc/ecc_sizes.c \
    pk/ecc/ecc_test.c \
    pk/ecc/ecc_verify_hash.c \
    pk/ecc/ltc_ecc_is_valid_idx.c \
    pk/ecc/ltc_ecc_map.c \
    pk/ecc/ltc_ecc_mul2add.c \
    pk/ecc/ltc_ecc_mulmod.c \
    pk/ecc/ltc_ecc_mulmod_timing.c \
    pk/ecc/ltc_ecc_points.c \
    pk/ecc/ltc_ecc_projective_add_point.c \
    pk/ecc/ltc_ecc_projective_dbl_point.c \
    math/gmp_desc.c \
    math/ltm_desc.c \
    math/multi.c \
    math/rand_prime.c \
    math/tfm_desc.c \
    math/fp/ltc_ecc_fp_mulmod.c \
    prngs/rc4.c \
    pk/rsa/rsa_decrypt_key.c \
    pk/rsa/rsa_encrypt_key.c \
    pk/rsa/rsa_export.c \
    pk/rsa/rsa_exptmod.c \
    pk/rsa/rsa_free.c \
    pk/rsa/rsa_import.c \
    pk/rsa/rsa_make_key.c \
    pk/rsa/rsa_sign_hash.c \
    pk/rsa/rsa_verify_hash.c \
    pk/pkcs1/pkcs_1_i2osp.c \
    pk/pkcs1/pkcs_1_mgf1.c \
    pk/pkcs1/pkcs_1_oaep_decode.c \
    pk/pkcs1/pkcs_1_oaep_encode.c \
    pk/pkcs1/pkcs_1_os2ip.c \
    pk/pkcs1/pkcs_1_pss_decode.c \
    pk/pkcs1/pkcs_1_pss_encode.c \
    pk/pkcs1/pkcs_1_v1_5_decode.c \
    pk/pkcs1/pkcs_1_v1_5_encode.c \
    pk/asn1/der/sequence/der_decode_sequence_ex.c \
    pk/asn1/der/sequence/der_decode_sequence_flexi.c \
    pk/asn1/der/sequence/der_decode_sequence_multi.c \
    pk/asn1/der/sequence/der_encode_sequence_ex.c \
    pk/asn1/der/sequence/der_encode_sequence_multi.c \
    pk/asn1/der/sequence/der_length_sequence.c \
    pk/asn1/der/sequence/der_sequence_free.c \
    pk/asn1/der/bit/der_decode_bit_string.c \
    pk/asn1/der/bit/der_encode_bit_string.c \
    pk/asn1/der/bit/der_length_bit_string.c \
    pk/asn1/der/boolean/der_decode_boolean.c \
    pk/asn1/der/boolean/der_encode_boolean.c \
    pk/asn1/der/boolean/der_length_boolean.c \
    pk/asn1/der/choice/der_decode_choice.c \
    pk/asn1/der/ia5/der_decode_ia5_string.c \
    pk/asn1/der/ia5/der_encode_ia5_string.c \
    pk/asn1/der/ia5/der_length_ia5_string.c \
    pk/asn1/der/integer/der_decode_integer.c \
    pk/asn1/der/integer/der_encode_integer.c \
    pk/asn1/der/integer/der_length_integer.c \
    pk/asn1/der/object_identifier/der_decode_object_identifier.c \
    pk/asn1/der/object_identifier/der_encode_object_identifier.c \
    pk/asn1/der/object_identifier/der_length_object_identifier.c \
    pk/asn1/der/octet/der_decode_octet_string.c \
    pk/asn1/der/octet/der_encode_octet_string.c \
    pk/asn1/der/octet/der_length_octet_string.c \
    pk/asn1/der/printable_string/der_decode_printable_string.c \
    pk/asn1/der/printable_string/der_encode_printable_string.c \
    pk/asn1/der/printable_string/der_length_printable_string.c \
    pk/asn1/der/set/der_encode_set.c \
    pk/asn1/der/set/der_encode_setof.c \
    pk/asn1/der/short_integer/der_decode_short_integer.c \
    pk/asn1/der/short_integer/der_encode_short_integer.c \
    pk/asn1/der/short_integer/der_length_short_integer.c \
    pk/asn1/der/utctime/der_decode_utctime.c \
    pk/asn1/der/utctime/der_encode_utctime.c \
    pk/asn1/der/utctime/der_length_utctime.c \
    pk/asn1/der/utf8/der_decode_utf8_string.c \
    pk/asn1/der/utf8/der_encode_utf8_string.c \
    pk/asn1/der/utf8/der_length_utf8_string.c \
    prngs/fortuna.c \
    prngs/rng_get_bytes.c \
    prngs/rng_make_prng.c \
    prngs/sober128.c \
    prngs/sober128tab.c \
    prngs/sprng.c \
    prngs/yarrow.c \
    tommath/bncore.c \
    tommath/bn_s_mp_sub.c \
    tommath/bn_s_mp_sqr.c \
    tommath/bn_s_mp_mul_high_digs.c \
    tommath/bn_s_mp_mul_digs.c \
    tommath/bn_s_mp_exptmod.c \
    tommath/bn_s_mp_add.c \
    tommath/bn_reverse.c \
    tommath/bn_prime_tab.c \
    tommath/bn_mp_zero.c \
    tommath/bn_mp_xor.c \
    tommath/bn_mp_unsigned_bin_size.c \
    tommath/bn_mp_toradix_n.c \
    tommath/bn_mp_toradix.c \
    tommath/bn_mp_toom_sqr.c \
    tommath/bn_mp_toom_mul.c \
    tommath/bn_mp_to_unsigned_bin_n.c \
    tommath/bn_mp_to_unsigned_bin.c \
    tommath/bn_mp_to_signed_bin_n.c \
    tommath/bn_mp_to_signed_bin.c \
    tommath/bn_mp_submod.c \
    tommath/bn_mp_sub_d.c \
    tommath/bn_mp_sub.c \
    tommath/bn_mp_sqrt.c \
    tommath/bn_mp_sqrmod.c \
    tommath/bn_mp_sqr.c \
    tommath/bn_mp_signed_bin_size.c \
    tommath/bn_mp_shrink.c \
    tommath/bn_mp_set_int.c \
    tommath/bn_mp_set.c \
    tommath/bn_mp_rshd.c \
    tommath/bn_mp_reduce_setup.c \
    tommath/bn_mp_reduce_is_2k_l.c \
    tommath/bn_mp_reduce_is_2k.c \
    tommath/bn_mp_reduce_2k_setup_l.c \
    tommath/bn_mp_reduce_2k_setup.c \
    tommath/bn_mp_reduce_2k_l.c \
    tommath/bn_mp_reduce_2k.c \
    tommath/bn_mp_reduce.c \
    tommath/bn_mp_read_unsigned_bin.c \
    tommath/bn_mp_read_signed_bin.c \
    tommath/bn_mp_read_radix.c \
    tommath/bn_mp_rand.c \
    tommath/bn_mp_radix_smap.c \
    tommath/bn_mp_radix_size.c \
    tommath/bn_mp_prime_random_ex.c \
    tommath/bn_mp_prime_rabin_miller_trials.c \
    tommath/bn_mp_prime_next_prime.c \
    tommath/bn_mp_prime_miller_rabin.c \
    tommath/bn_mp_prime_is_prime.c \
    tommath/bn_mp_prime_is_divisible.c \
    tommath/bn_mp_prime_fermat.c \
    tommath/bn_mp_or.c \
    tommath/bn_mp_neg.c \
    tommath/bn_mp_n_root.c \
    tommath/bn_mp_mulmod.c \
    tommath/bn_mp_mul_d.c \
    tommath/bn_mp_mul_2d.c \
    tommath/bn_mp_mul_2.c \
    tommath/bn_mp_mul.c \
    tommath/bn_mp_montgomery_setup.c \
    tommath/bn_mp_montgomery_reduce.c \
    tommath/bn_mp_montgomery_calc_normalization.c \
    tommath/bn_mp_mod_d.c \
    tommath/bn_mp_mod_2d.c \
    tommath/bn_mp_mod.c \
    tommath/bn_mp_lshd.c \
    tommath/bn_mp_lcm.c \
    tommath/bn_mp_karatsuba_sqr.c \
    tommath/bn_mp_karatsuba_mul.c \
    tommath/bn_mp_jacobi.c \
    tommath/bn_mp_is_square.c \
    tommath/bn_mp_invmod_slow.c \
    tommath/bn_mp_invmod.c \
    tommath/bn_mp_init_size.c \
    tommath/bn_mp_init_set_int.c \
    tommath/bn_mp_init_set.c \
    tommath/bn_mp_init_multi.c \
    tommath/bn_mp_init_copy.c \
    tommath/bn_mp_init.c \
    tommath/bn_mp_grow.c \
    tommath/bn_mp_get_int.c \
    tommath/bn_mp_gcd.c \
    tommath/bn_mp_fwrite.c \
    tommath/bn_mp_fread.c \
    tommath/bn_mp_exteuclid.c \
    tommath/bn_mp_exptmod_fast.c \
    tommath/bn_mp_exptmod.c \
    tommath/bn_mp_expt_d.c \
    tommath/bn_mp_exch.c \
    tommath/bn_mp_dr_setup.c \
    tommath/bn_mp_dr_reduce.c \
    tommath/bn_mp_dr_is_modulus.c \
    tommath/bn_mp_div_d.c \
    tommath/bn_mp_div_3.c \
    tommath/bn_mp_div_2d.c \
    tommath/bn_mp_div_2.c \
    tommath/bn_mp_div.c \
    tommath/bn_mp_count_bits.c \
    tommath/bn_mp_copy.c \
    tommath/bn_mp_cnt_lsb.c \
    tommath/bn_mp_cmp_mag.c \
    tommath/bn_mp_cmp_d.c \
    tommath/bn_mp_cmp.c \
    tommath/bn_mp_clear_multi.c \
    tommath/bn_mp_clear.c \
    tommath/bn_mp_clamp.c \
    tommath/bn_mp_and.c \
    tommath/bn_mp_addmod.c \
    tommath/bn_mp_add_d.c \
    tommath/bn_mp_add.c \
    tommath/bn_mp_abs.c \
    tommath/bn_mp_2expt.c \
    tommath/bn_fast_s_mp_sqr.c \
    tommath/bn_fast_s_mp_mul_high_digs.c \
    tommath/bn_fast_s_mp_mul_digs.c \
    tommath/bn_fast_mp_montgomery_reduce.c \
    tommath/bn_fast_mp_invmod.c \
    tommath/bn_error.c



#OTHER_FILES += \


