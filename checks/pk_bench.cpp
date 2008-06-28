#include <botan/dsa.h>
#include <botan/rsa.h>
#include <botan/dh.h>
#include <botan/nr.h>
#include <botan/rw.h>
#include <botan/elgamal.h>
#include <botan/parsing.h>

#include <botan/pkcs8.h>
#include <botan/look_pk.h>

using namespace Botan;

#include "common.h"
#include "bench.h"

#include <iostream>
#include <fstream>
#include <string>
#include <memory>

#define PRINT_MS_PER_OP 0 /* If 0, print ops / second */

void bench_enc(PK_Encryptor*, RandomNumberGenerator&,
               const std::string&, double, bool);
void bench_dec(PK_Encryptor*, PK_Decryptor*, RandomNumberGenerator&,
               const std::string&, double, bool);
void bench_sig(PK_Signer*, RandomNumberGenerator&,
               const std::string&, double, bool);
void bench_ver(PK_Signer*, PK_Verifier*,
               RandomNumberGenerator&,
               const std::string&, double, bool);
void bench_kas(PK_Key_Agreement*, RandomNumberGenerator&,
               const std::string&, double, bool);

void bench_pk(RandomNumberGenerator& rng,
              const std::string& algo, bool html, double seconds)
   {
   /*
     There is some strangeness going on here. It looks like algorithms
     at the end take some kind of penalty. For example, running the RW tests
     first got a result of:
         RW-1024: 148.14 ms / private operation
     but running them last output:
         RW-1024: 363.54 ms / private operation

     I think it's from memory fragmentation in the allocators, but I'm
     not really sure. Need to investigate.

     Until then, I've basically ordered the tests in order of most important
     algorithms (RSA, DSA) to least important (NR, RW).

     This strange behaviour does not seem to occur with DH (?)

     To get more accurate runs, use --bench-algo (RSA|DSA|DH|ELG|NR); in this
     case the distortion is less than 5%, which is good enough.

     We do random keys with the DL schemes, since it's so easy and fast to
     generate keys for them. For RSA and RW, we load the keys from a file.  The
     RSA keys are stored in a PKCS #8 structure, while RW is stored in a more
     ad-hoc format (the RW algorithm has no assigned OID that I know of, so
     there is no way to encode a RW key into a PKCS #8 structure).
   */

   if(algo == "All" || algo == "RSA")
      {
      const u32bit keylen[] = { 512, 1024, 1536, 2048, 3072, 4096, 0 };

      for(size_t j = 0; keylen[j]; j++)
         {
         const std::string len_str = to_string(keylen[j]);
         const std::string file = "checks/keys/rsa" + len_str + ".pem";

         std::auto_ptr<RSA_PrivateKey> key(
            dynamic_cast<RSA_PrivateKey*>(PKCS8::load_key(file, rng))
            );

         if(key.get() == 0)
            throw Invalid_Argument("Failure reading RSA key from " + file);

         bench_enc(get_pk_encryptor(*key, "Raw"),
                   rng, "RSA-" + len_str, seconds, html);

         bench_dec(get_pk_encryptor(*key, "Raw"),
                   get_pk_decryptor(*key, "Raw"),
                   rng, "RSA-" + len_str, seconds, html);
         }
      }

   if(algo == "All" || algo == "DSA")
      {
      const u32bit keylen[] = { 512, 768, 1024, 0 };

      for(size_t j = 0; keylen[j]; j++)
         {
         const std::string len_str = to_string(keylen[j]);

         DSA_PrivateKey key(rng, "dsa/jce/" + len_str);

         bench_ver(get_pk_signer(key, "EMSA1(SHA-1)"),
                   get_pk_verifier(key, "EMSA1(SHA-1)"),
                   rng, "DSA-" + len_str, seconds, html);

         bench_sig(get_pk_signer(key, "EMSA1(SHA-1)"),
                   rng, "DSA-" + len_str, seconds, html);
         }
      }

   if(algo == "All" || algo == "DH")
      {
      const u32bit keylen[] = { 768, 1024, 1536, 2048, 3072, 4096, 0 };

      for(size_t j = 0; keylen[j]; j++)
         {
         const std::string len_str = to_string(keylen[j]);

         DH_PrivateKey key(rng,
                           "modp/ietf/" + len_str);

         bench_kas(get_pk_kas(key, "Raw"), rng,
                   "DH-" + len_str, seconds, html);
         }
      }

   if(algo == "All" || algo == "ELG" || algo == "ElGamal")
      {
      const u32bit keylen[] = { 768, 1024, 1536, 2048, 3072, 4096, 0 };

      for(size_t j = 0; keylen[j]; j++)
         {
         const std::string len_str = to_string(keylen[j]);

         ElGamal_PrivateKey key(rng, "modp/ietf/" + len_str);

         bench_enc(get_pk_encryptor(key, "Raw"),
                   rng, "ELG-" + len_str, seconds, html);

         bench_dec(get_pk_encryptor(key, "Raw"),
                   get_pk_decryptor(key, "Raw"),
                   rng, "ELG-" + len_str, seconds, html);
         }
      }

   if(algo == "All" || algo == "NR")
      {
      const u32bit keylen[] = { 512, 768, 1024, 0 };

      for(size_t j = 0; keylen[j]; j++)
         {
         const std::string len_str = to_string(keylen[j]);

         NR_PrivateKey key(rng, "dsa/jce/" + len_str);

         bench_ver(get_pk_signer(key, "EMSA1(SHA-1)"),
                   get_pk_verifier(key, "EMSA1(SHA-1)"),
                   rng, "NR-" + len_str, seconds, html);

         bench_sig(get_pk_signer(key, "EMSA1(SHA-1)"),
                   rng, "NR-" + len_str, seconds, html);
         }
      }

   if(algo == "All" || algo == "RW")
      {
      const u32bit keylen[] = { 512, 1024, 0 };

      for(size_t j = 0; keylen[j]; j++)
         {
         const std::string len_str = to_string(keylen[j]);
         const std::string file = "checks/keys/rw" + len_str + ".pem";

         RW_PrivateKey* key =
            dynamic_cast<RW_PrivateKey*>(PKCS8::load_key(file, rng));

         bench_ver(get_pk_signer(*key, "EMSA2(SHA-1)"),
                   get_pk_verifier(*key, "EMSA2(SHA-1)"),
                   rng, "RW-" + len_str, seconds, html);
         bench_sig(get_pk_signer(*key, "EMSA2(SHA-1)"),
                   rng, "RW-" + len_str, seconds, html);

         delete key;
         }
      }
   }

namespace {

void print_result(bool html, u32bit runs, u64bit clocks_used,
                  const std::string& algo_name, const std::string& op)
   {
   double seconds = static_cast<double>(clocks_used) / get_ticks();
   double mseconds_per_run = 1000 * (seconds / runs);
   double runs_per_sec = runs / seconds;

   if(html)
      {
      std::cout << "   <TR><TH>" << algo_name << " (" << op << ") <TH>";

      if(PRINT_MS_PER_OP)
         std::cout << mseconds_per_run;
      else
         std::cout << runs_per_sec;

      std::cout << std::endl;
      }
   else
      {
      std::cout << algo_name << ": ";

      std::cout.setf(std::ios::fixed, std::ios::floatfield);
      std::cout.precision(2);

      if(PRINT_MS_PER_OP)
         std::cout << mseconds_per_run << " ms / " << op << "\n";
      else
         std::cout << runs_per_sec << " ops / second (" << op << ")\n";
      }
   }

}

void bench_enc(PK_Encryptor* enc,
               RandomNumberGenerator& rng,
               const std::string& algo_name,
               double seconds, bool html)
   {
   static const u32bit MSG_SIZE = 16;
   byte msg[MSG_SIZE];

   u32bit runs = 0;

   u64bit clocks_used = 0;

   const u64bit ticks = get_ticks();
   while(clocks_used < seconds * ticks)
      {
      runs++;
      rng.randomize(msg, MSG_SIZE);

      u64bit start = get_clock();
      enc->encrypt(msg, MSG_SIZE, rng);
      clocks_used += get_clock() - start;
      }

   delete enc;

   print_result(html, runs, clocks_used, algo_name, "public operation");
   }

void bench_dec(PK_Encryptor* enc, PK_Decryptor* dec,
               RandomNumberGenerator& rng,
               const std::string& algo_name,
               double seconds, bool html)
   {
   static const u32bit MSG_SIZE = 16;
   byte msg[MSG_SIZE];
   rng.randomize(msg, MSG_SIZE);
   SecureVector<byte> output;

   u32bit runs = 0;
   u64bit clocks_used = 0;

   SecureVector<byte> encrypted_msg = enc->encrypt(msg, MSG_SIZE, rng);

   const u64bit ticks = get_ticks();
   while(clocks_used < seconds * ticks)
      {
      runs++;

      rng.randomize(msg, MSG_SIZE);
      msg[0] |= 0x80; // make sure it works with "Raw" padding
      encrypted_msg = enc->encrypt(msg, MSG_SIZE, rng);

      u64bit start = get_clock();
      output = dec->decrypt(encrypted_msg);
      clocks_used += get_clock() - start;

      if(output.size() != MSG_SIZE ||
         std::memcmp(msg, output, MSG_SIZE) != 0)
         {
         std::cout << hex_encode(msg, MSG_SIZE) << std::endl;
         std::cout << hex_encode(output, output.size()) << std::endl;
         throw Internal_Error("Decrypt check failed during benchmark");
         }
      }

   delete enc;
   delete dec;

   print_result(html, runs, clocks_used, algo_name, "private operation");
   }

void bench_sig(PK_Signer* sig,
               RandomNumberGenerator& rng,
               const std::string& algo_name,
               double seconds, bool html)
   {
   static const u32bit MSG_SIZE = 16;
   byte msg[MSG_SIZE];

   u32bit runs = 0;
   u64bit clocks_used = 0;

   const u64bit ticks = get_ticks();
   while(clocks_used < seconds * ticks)
      {
      runs++;
      rng.randomize(msg, MSG_SIZE);
      u64bit start = get_clock();
      sig->update(msg, MSG_SIZE);
      sig->signature(rng);
      clocks_used += get_clock() - start;
      }

   delete sig;

   print_result(html, runs, clocks_used, algo_name, "private operation");
   }

void bench_ver(PK_Signer* sig, PK_Verifier* ver,
               RandomNumberGenerator& rng,
               const std::string& algo_name,
               double seconds, bool html)
   {
   static const u32bit MSG_SIZE = 16;
   byte msg[MSG_SIZE];
   rng.randomize(msg, MSG_SIZE);

   sig->update(msg, MSG_SIZE);
   SecureVector<byte> signature = sig->signature(rng);
   u32bit runs = 0;
   u64bit clocks_used = 0;

   const u64bit ticks = get_ticks();
   while(clocks_used < seconds * ticks)
      {
      // feel free to tweak, but make sure this always runs when runs == 0
      if(runs % 100 == 0)
         {
         rng.randomize(msg, MSG_SIZE);
         sig->update(msg, MSG_SIZE);
         signature = sig->signature(rng);
         }

      runs++;

      u64bit start = get_clock();
      ver->update(msg, MSG_SIZE);
      bool result = ver->check_signature(signature, signature.size());
      clocks_used += get_clock() - start;
      if(!result)
         throw Internal_Error("Signature check failed during benchmark");
      }

   delete sig;
   delete ver;

   print_result(html, runs, clocks_used, algo_name, "public operation");
   }

void bench_kas(PK_Key_Agreement* kas,
               RandomNumberGenerator& rng,
               const std::string& algo_name,
               double seconds, bool html)
   {
   /* 128 bits: should always be considered valid (what about ECC?) */
   static const u32bit REMOTE_KEY_SIZE = 16;
   byte key[REMOTE_KEY_SIZE];

   u32bit runs = 0;
   u64bit clocks_used = 0;

   const u64bit ticks = get_ticks();
   while(clocks_used < seconds * ticks)
      {
      runs++;
      rng.randomize(key, REMOTE_KEY_SIZE);

      u64bit start = get_clock();
      kas->derive_key(0, key, REMOTE_KEY_SIZE);
      clocks_used += get_clock() - start;
      }

   delete kas;

   print_result(html, runs, clocks_used, algo_name, "key agreement");
   }
