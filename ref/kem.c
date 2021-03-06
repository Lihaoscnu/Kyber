#include "api.h"
#include "randombytes.h"
#include "fips202.h"
#include "params.h"
#include "verify.h"
#include "indcpa.h"

// API FUNCTIONS 

/* Build a CCA-secure KEM from an IND-CPA-secure encryption scheme */

int crypto_kem_keypair(unsigned char *pk, unsigned char *sk)
{
  size_t i;
  indcpa_keypair(pk, sk);
  for(i=0;i<KYBER_INDCPA_PUBLICKEYBYTES;i++)
    sk[i+KYBER_INDCPA_SECRETKEYBYTES] = pk[i];
  shake256(sk+KYBER_SECRETKEYBYTES-64,32,pk,KYBER_PUBLICKEYBYTES);
  randombytes(sk+KYBER_SECRETKEYBYTES-KYBER_SHAREDKEYBYTES,KYBER_SHAREDKEYBYTES);        /* Value z for pseudo-random output on reject */
  return 0;
}

int crypto_kem_enc(unsigned char *ct, unsigned char *ss, const unsigned char *pk)
{
  unsigned char krq[96];                             /* Will contain key, coins, qrom-hash */
  unsigned char buf[64];                          
  int i;

  randombytes(buf, 32);
  shake256(buf,32,buf,32);                           /* Don't release system RNG output */

  shake256(buf+32, 32, pk, KYBER_PUBLICKEYBYTES);    /* Multitarget countermeasure for coins + contributory KEM */
  shake256(krq, 96, buf, 64);

  indcpa_enc(ct, buf, pk, krq+32);                   /* coins are in krq+32 */

  for(i=0;i<32;i++)
    ct[i+KYBER_INDCPA_BYTES] = krq[i+64];

  shake256(krq+32, 32, ct, KYBER_CIPHERTEXTBYTES);   /* overwrite coins in krq with h(c) */
  shake256(ss, 32, krq, 64);                         /* hash concatenation of pre-k and h(c) to k */
  return 0;
}

int crypto_kem_dec(unsigned char *ss, const unsigned char *ct, const unsigned char *sk)
{
  int i, fail;
  unsigned char cmp[KYBER_CIPHERTEXTBYTES];
  unsigned char buf[64];
  unsigned char krq[96];                             /* Will contain key, coins, qrom-hash */
  const unsigned char *pk = sk+KYBER_INDCPA_SECRETKEYBYTES;

  indcpa_dec(buf, ct, sk);

  // shake256(buf+32, 32, pk, KYBER_PUBLICKEYBYTES); /* Multitarget countermeasure for coins + contributory KEM */
  for(i=0;i<32;i++)                                  /* Save hash by storing h(pk) in sk */
    buf[32+i] = sk[KYBER_SECRETKEYBYTES-64+i];
  shake256(krq, 96, buf, 64);

  indcpa_enc(cmp, buf, pk, krq+32);                  /* coins are in krq+32 */

  for(i=0;i<32;i++)
    cmp[i+KYBER_INDCPA_BYTES] = krq[i+64];

  fail = verify(ct, cmp, KYBER_CIPHERTEXTBYTES);

  shake256(krq+32, 32, ct, KYBER_CIPHERTEXTBYTES);    /* overwrite coins in krq with h(c)  */

  cmov(krq, sk+KYBER_SECRETKEYBYTES-KYBER_SHAREDKEYBYTES, KYBER_SHAREDKEYBYTES, fail); /* Overwrite pre-k with z on re-encryption failure */

  shake256(ss, 32, krq, 64);                          /* hash concatenation of pre-k and h(c) to k */

  return -fail;
}
