#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <assert.h>

int base64_decode(const char *in, size_t inlen, char *out, size_t *outlen) {
	BIO *bio, *b64;
	BUF_MEM *buffer_ptr;

	b64 = BIO_new(BIO_f_base64());
	bio = BIO_new_mem_buf(in, inlen);
	bio = BIO_push(b64, bio);

	BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // Ignore newlines

	*outlen = BIO_read(bio, out, inlen);
	out[*outlen] = '\0';

	BIO_free_all(bio);

	return 0;
}
