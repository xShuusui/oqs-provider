// SPDX-License-Identifier: Apache-2.0 AND MIT

#include <openssl/evp.h>
#include <openssl/provider.h>
#include "test_common.h"
#include "oqs/oqs.h"

static OSSL_LIB_CTX *libctx = NULL;
static char *modulename = NULL;
static char *configfile = NULL;
static char *cert = NULL;
static char *privkey = NULL;
static char *certsdir = NULL;
static char *srpvfile = NULL;
static char *tmpfilename = NULL;

static const char *sigalg_names[] = {
///// OQS_TEMPLATE_FRAGMENT_SIGNATURE_CASES_START
#ifdef OQS_ENABLE_SIG_dilithium_2
  "dilithium2","p256_dilithium2","rsa3072_dilithium2",
#endif
#ifdef OQS_ENABLE_SIG_dilithium_3
  "dilithium3","p384_dilithium3",
#endif
#ifdef OQS_ENABLE_SIG_dilithium_5
  "dilithium5","p521_dilithium5",
#endif
#ifdef OQS_ENABLE_SIG_dilithium_2_aes
  "dilithium2_aes","p256_dilithium2_aes","rsa3072_dilithium2_aes",
#endif
#ifdef OQS_ENABLE_SIG_dilithium_3_aes
  "dilithium3_aes","p384_dilithium3_aes",
#endif
#ifdef OQS_ENABLE_SIG_dilithium_5_aes
  "dilithium5_aes","p521_dilithium5_aes",
#endif
#ifdef OQS_ENABLE_SIG_falcon_512
  "falcon512","p256_falcon512","rsa3072_falcon512",
#endif
#ifdef OQS_ENABLE_SIG_falcon_1024
  "falcon1024","p521_falcon1024",
#endif
#ifdef OQS_ENABLE_SIG_sphincs_haraka_128f_robust
  "sphincsharaka128frobust","p256_sphincsharaka128frobust","rsa3072_sphincsharaka128frobust",
#endif
#ifdef OQS_ENABLE_SIG_sphincs_sha256_128f_robust
  "sphincssha256128frobust","p256_sphincssha256128frobust","rsa3072_sphincssha256128frobust",
#endif
#ifdef OQS_ENABLE_SIG_sphincs_shake256_128f_robust
  "sphincsshake256128frobust","p256_sphincsshake256128frobust","rsa3072_sphincsshake256128frobust",
#endif
#ifdef OQS_ENABLE_SIG_sphincs_shake256_192f_simple
  "sphincsshake256192fsimple","p384_sphincsshake256192fsimple",
#endif
#ifdef OQS_ENABLE_SIG_sphincs_shake256_256f_simple
  "sphincsshake256256fsimple","p521_sphincsshake256256fsimple",
#endif
///// OQS_TEMPLATE_FRAGMENT_SIGNATURE_CASES_END
};

// sign-and-hash must work with and without providing a digest algorithm
static int test_oqs_signatures(const char *sigalg_name)
{
  EVP_MD_CTX *mdctx = NULL;
  EVP_PKEY_CTX *ctx = NULL;
  EVP_PKEY *key = NULL;
  const char msg[] = "The quick brown fox jumps over... you know what";
  unsigned char *sig;
  size_t siglen;

  int testresult = 1;

  if (!alg_is_enabled(sigalg_name)) {
     printf("Not testing disabled algorithm %s.\n", sigalg_name);
     return 1;
  }
  // test with built-in digest only if default provider is active:
  // TBD revisit when hybrids are activated: They always need default provider
  if (OSSL_PROVIDER_available(libctx, "default")) {
    testresult  &=
      (ctx = EVP_PKEY_CTX_new_from_name(libctx, sigalg_name, NULL)) != NULL
      && EVP_PKEY_keygen_init(ctx)
      && EVP_PKEY_generate(ctx, &key)
      && (mdctx = EVP_MD_CTX_new()) != NULL
      && EVP_DigestSignInit_ex(mdctx, NULL, "SHA512", libctx, NULL, key, NULL)
      && EVP_DigestSignUpdate(mdctx, msg, sizeof(msg))
      && EVP_DigestSignFinal(mdctx, NULL, &siglen)
      && (sig = OPENSSL_malloc(siglen)) != NULL
      && EVP_DigestSignFinal(mdctx, sig, &siglen)
      && EVP_DigestVerifyInit_ex(mdctx, NULL, "SHA512", libctx, NULL, key, NULL)
      && EVP_DigestVerifyUpdate(mdctx, msg, sizeof(msg))
      && EVP_DigestVerifyFinal(mdctx, sig, siglen);
    sig[0] = ~sig[0];
    testresult &=
      EVP_DigestVerifyInit_ex(mdctx, NULL, "SHA512", libctx, NULL, key, NULL)
      && EVP_DigestVerifyUpdate(mdctx, msg, sizeof(msg))
      && !EVP_DigestVerifyFinal(mdctx, sig, siglen);
  }

  EVP_MD_CTX_free(mdctx);
  EVP_PKEY_free(key);
  OPENSSL_free(ctx);
  OPENSSL_free(sig);
  mdctx = NULL;
  key = NULL;

  // this test must work also with default provider inactive:
  testresult &=
    (ctx = EVP_PKEY_CTX_new_from_name(libctx, sigalg_name, NULL)) != NULL
    && EVP_PKEY_keygen_init(ctx)
    && EVP_PKEY_generate(ctx, &key)
    && (mdctx = EVP_MD_CTX_new()) != NULL
    && EVP_DigestSignInit_ex(mdctx, NULL, NULL, libctx, NULL, key, NULL)
    && EVP_DigestSignUpdate(mdctx, msg, sizeof(msg))
    && EVP_DigestSignFinal(mdctx, NULL, &siglen)
    && (sig = OPENSSL_malloc(siglen)) != NULL
    && EVP_DigestSignFinal(mdctx, sig, &siglen)
    && EVP_DigestVerifyInit_ex(mdctx, NULL, NULL, libctx, NULL, key, NULL)
    && EVP_DigestVerifyUpdate(mdctx, msg, sizeof(msg))
    && EVP_DigestVerifyFinal(mdctx, sig, siglen);
  sig[0] = ~sig[0];
  testresult &=
    EVP_DigestVerifyInit_ex(mdctx, NULL, NULL, libctx, NULL, key, NULL)
    && EVP_DigestVerifyUpdate(mdctx, msg, sizeof(msg))
    && !EVP_DigestVerifyFinal(mdctx, sig, siglen);

  EVP_MD_CTX_free(mdctx);
  EVP_PKEY_free(key);
  OPENSSL_free(ctx);
  OPENSSL_free(sig);
  return testresult;
}

#define nelem(a) (sizeof(a)/sizeof((a)[0]))

int main(int argc, char *argv[])
{
  size_t i;
  int errcnt = 0, test = 0;

  T((libctx = OSSL_LIB_CTX_new()) != NULL);
  T(argc == 3);
  modulename = argv[1];
  configfile = argv[2];

  T(OSSL_LIB_CTX_load_config(libctx, configfile));

  T(OSSL_PROVIDER_available(libctx, modulename));

  for (i = 0; i < nelem(sigalg_names); i++) {
    if (test_oqs_signatures(sigalg_names[i])) {
      fprintf(stderr,
              cGREEN "  Signature test succeeded: %s" cNORM "\n",
              sigalg_names[i]);
    } else {
      fprintf(stderr,
              cRED "  Signature test failed: %s" cNORM "\n",
              sigalg_names[i]);
      ERR_print_errors_fp(stderr);
      errcnt++;
    }
  }

  OSSL_LIB_CTX_free(libctx);

  TEST_ASSERT(errcnt == 0)
  return !test;
}
