/*
 * Copyright (c) 1997-2005 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "kdc_locl.h"
#include <vis.h>

/*
 *
 */

#undef  __attribute__
#define __attribute__(x)

void
_kdc_audit_vaddreason(kdc_request_t r, const char *fmt, va_list ap)
	__attribute__ ((__format__ (__printf__, 2, 0)))
{
    heim_audit_vaddreason((heim_svc_req_desc)r, fmt, ap);
}

void
_kdc_audit_addreason(kdc_request_t r, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 2, 3)))
{
    va_list ap;

    va_start(ap, fmt);
    heim_audit_vaddreason((heim_svc_req_desc)r, fmt, ap);
    va_end(ap);
}

/*
 * append_token adds a token which is optionally a kv-pair and it
 * also optionally eats the whitespace.  If k == NULL, then it's
 * not a kv-pair.
 */

void
_kdc_audit_vaddkv(kdc_request_t r, int flags, const char *k,
		  const char *fmt, va_list ap)
	__attribute__ ((__format__ (__printf__, 4, 0)))
{
    heim_audit_vaddkv((heim_svc_req_desc)r, flags, k, fmt, ap);
}

void
_kdc_audit_addkv(kdc_request_t r, int flags, const char *k,
		 const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 4, 5)))
{
    va_list ap;

    va_start(ap, fmt);
    heim_audit_vaddkv((heim_svc_req_desc)r, flags, k, fmt, ap);
    va_end(ap);
}

void
_kdc_audit_addkv_timediff(kdc_request_t r, const char *k,
			  const struct timeval *start,
			  const struct timeval *end)
{
    heim_audit_addkv_timediff((heim_svc_req_desc)r,k, start, end);
}

/*
 * Add up to 3 key value pairs to record HostAddresses from request body or
 * PA-TGS ticket or whatever.
 */
void
_kdc_audit_addaddrs(kdc_request_t r, HostAddresses *a, const char *key)
{
    size_t i;
    char buf[128];

    if (a->len > 3) {
        char numkey[32];

        if (snprintf(numkey, sizeof(numkey), "num%s", key) >= sizeof(numkey))
            numkey[31] = '\0';
        _kdc_audit_addkv(r, 0, numkey, "%llu", (unsigned long long)a->len);
    }

    for (i = 0; i < 3 && i < a->len; i++) {
        if (krb5_print_address(&a->val[i], buf, sizeof(buf), NULL) == 0)
            _kdc_audit_addkv(r, 0, key, "%s", buf);
    }
}

void
_kdc_audit_trail(kdc_request_t r, krb5_error_code ret)
{
    const char *retname = NULL;

    /* Get a symbolic name for some error codes */
#define CASE(x)	case x : retname = #x; break
    switch (ret) {
    CASE(ENOMEM);
    CASE(EACCES);
    CASE(HDB_ERR_NOT_FOUND_HERE);
    CASE(HDB_ERR_WRONG_REALM);
    CASE(HDB_ERR_EXISTS);
    CASE(HDB_ERR_KVNO_NOT_FOUND);
    CASE(HDB_ERR_NOENTRY);
    CASE(HDB_ERR_NO_MKEY);
    CASE(KRB5KDC_ERR_BADOPTION);
    CASE(KRB5KDC_ERR_CANNOT_POSTDATE);
    CASE(KRB5KDC_ERR_CLIENT_NOTYET);
    CASE(KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN);
    CASE(KRB5KDC_ERR_ETYPE_NOSUPP);
    CASE(KRB5KDC_ERR_KEY_EXPIRED);
    CASE(KRB5KDC_ERR_NAME_EXP);
    CASE(KRB5KDC_ERR_NEVER_VALID);
    CASE(KRB5KDC_ERR_NONE);
    CASE(KRB5KDC_ERR_NULL_KEY);
    CASE(KRB5KDC_ERR_PADATA_TYPE_NOSUPP);
    CASE(KRB5KDC_ERR_POLICY);
    CASE(KRB5KDC_ERR_PREAUTH_FAILED);
    CASE(KRB5KDC_ERR_PREAUTH_REQUIRED);
    CASE(KRB5KDC_ERR_SERVER_NOMATCH);
    CASE(KRB5KDC_ERR_SERVICE_EXP);
    CASE(KRB5KDC_ERR_SERVICE_NOTYET);
    CASE(KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN);
    CASE(KRB5KDC_ERR_TRTYPE_NOSUPP);
    CASE(KRB5KRB_ERR_RESPONSE_TOO_BIG);
    case 0:
	retname = "SUCCESS";
	break;
    default:
        retname = NULL;
	break;
    }

    /* Let's save a few bytes */
#define PREFIX "KRB5KDC_"
    if (retname && strncmp(PREFIX, retname, strlen(PREFIX)) == 0)
	retname += strlen(PREFIX);
#undef PREFIX

    heim_audit_trail((heim_svc_req_desc)r, ret, retname);
}

void
krb5_kdc_update_time(struct timeval *tv)
{
    if (tv == NULL)
	gettimeofday(&_kdc_now, NULL);
    else
	_kdc_now = *tv;
}


#define EXTEND_REQUEST_T(LHS, RHS) do {			\
	RHS = realloc(LHS, sizeof(*RHS));		\
	if (!RHS)					\
	    return krb5_enomem((LHS)->context);		\
	LHS = (void *)RHS;				\
	memset(((char *)LHS) + sizeof(*LHS),		\
	       0x0,					\
	       sizeof(*RHS) - sizeof(*LHS));		\
    } while (0)

static krb5_error_code
kdc_as_req(kdc_request_t *rptr, int *claim)
{
    astgs_request_t r;
    krb5_error_code ret;
    size_t len;

    /* We must free things in the extensions */
    EXTEND_REQUEST_T(*rptr, r);

    ret = decode_AS_REQ(r->request.data, r->request.length, &r->req, &len);
    if (ret)
	return ret;

    r->reqtype = "AS-REQ";
    r->use_request_t = 1;
    *claim = 1;

    ret = _kdc_as_rep(r);
    free_AS_REQ(&r->req);
    return ret;
}


static krb5_error_code
kdc_tgs_req(kdc_request_t *rptr, int *claim)
{
    astgs_request_t r;
    krb5_error_code ret;
    size_t len;

    /* We must free things in the extensions */
    EXTEND_REQUEST_T(*rptr, r);

    ret = decode_TGS_REQ(r->request.data, r->request.length, &r->req, &len);
    if (ret)
	return ret;

    r->reqtype = "TGS-REQ";
    r->use_request_t = 1;
    *claim = 1;

    ret = _kdc_tgs_rep(r);
    free_TGS_REQ(&r->req);
    return ret;
}

#ifdef DIGEST

static krb5_error_code
kdc_digest(kdc_request_t *rptr, int *claim)
{
    kdc_request_t r;
    DigestREQ digestreq;
    krb5_error_code ret;
    size_t len;

    r = *rptr;

    ret = decode_DigestREQ(r->request.data, r->request.length,
			   &digestreq, &len);
    if (ret)
	return ret;

    r->use_request_t = 0;
    *claim = 1;

    ret = _kdc_do_digest(r->context, r->config, &digestreq,
			 r->reply, r->from, r->addr);
    free_DigestREQ(&digestreq);
    return ret;
}

#endif

#ifdef KX509

static krb5_error_code
kdc_kx509(kdc_request_t *rptr, int *claim)
{
    kx509_req_context r;
    krb5_error_code ret;

    /* We must free things in the extensions */
    EXTEND_REQUEST_T(*rptr, r);

    ret = _kdc_try_kx509_request(r);
    if (ret)
	return ret;

    r->use_request_t = 1;
    r->reqtype = "KX509";
    *claim = 1;

    return _kdc_do_kx509(r); /* Must clean up the req struct extensions */
}

#endif


static struct krb5_kdc_service services[] =  {
    { KS_KRB5, "AS-REQ",	kdc_as_req },
    { KS_KRB5, "TGS-REQ",	kdc_tgs_req },
#ifdef DIGEST
    { 0,	"DIGEST",	kdc_digest },
#endif
#ifdef KX509
    { 0,	"KX509",	kdc_kx509 },
#endif
    { 0, NULL, NULL }
};

static int
process_request(krb5_context context,
		krb5_kdc_configuration *config,
		unsigned int krb5_only,
		unsigned char *buf,
		size_t len,
		krb5_data *reply,
		krb5_boolean *prependlength,
		const char *from,
		struct sockaddr *addr,
		int datagram_reply)
{
    kdc_request_t r;
    krb5_error_code ret;
    unsigned int i;
    int claim = 0;

    r = calloc(sizeof(*r), 1);
    if (!r)
	return krb5_enomem(context);

    r->context = context;
    r->hcontext = context->hcontext;
    r->config = config;
    r->logf = config->logf;
    r->from = from;
    r->addr = addr;
    r->request.data = buf;
    r->request.length = len;
    r->datagram_reply = datagram_reply;
    r->reply = reply;
    r->kv = heim_array_create();
    if (!r->kv) {
	free(r);
	return krb5_enomem(context);
    }

    gettimeofday(&r->tv_start, NULL);

    for (i = 0; services[i].process != NULL; i++) {
	if (krb5_only && (services[i].flags & KS_KRB5) == 0)
	    continue;
	kdc_log(context, config, 7, "Probing for %s", services[i].name);
	ret = (*services[i].process)(&r, &claim);
	if (claim) {
	    if (prependlength && services[i].flags & KS_NO_LENGTH)
		*prependlength = 0;

	    if (r->use_request_t) {
		gettimeofday(&r->tv_end, NULL);
		_kdc_audit_trail(r, ret);
		free(r->cname);
		free(r->sname);
		free(r->e_text_buf);
	    }

            heim_release(r->reason);
            heim_release(r->kv);
            free(r);
	    return ret;
	}
    }

    heim_release(r->reason);
    heim_release(r->kv);
    free(r);
    return -1;
}

/*
 * handle the request in `buf, len', from `addr' (or `from' as a string),
 * sending a reply in `reply'.
 */

int
krb5_kdc_process_request(krb5_context context,
			 krb5_kdc_configuration *config,
			 unsigned char *buf,
			 size_t len,
			 krb5_data *reply,
			 krb5_boolean *prependlength,
			 const char *from,
			 struct sockaddr *addr,
			 int datagram_reply)
{
    return process_request(context, config, 0, buf, len, reply, prependlength,
			   from, addr, datagram_reply);
}
 
/*
 * handle the request in `buf, len', from `addr' (or `from' as a string),
 * sending a reply in `reply'.
 *
 * This only processes krb5 requests
 */

int
krb5_kdc_process_krb5_request(krb5_context context,
			      krb5_kdc_configuration *config,
			      unsigned char *buf,
			      size_t len,
			      krb5_data *reply,
			      const char *from,
			      struct sockaddr *addr,
			      int datagram_reply)
{
    return process_request(context, config, 1, buf, len, reply, NULL,
			   from, addr, datagram_reply);
}


/*
 *
 */

int
krb5_kdc_save_request(krb5_context context,
		      const char *fn,
		      const unsigned char *buf,
		      size_t len,
		      const krb5_data *reply,
		      const struct sockaddr *sa)
{
    krb5_storage *sp;
    krb5_address a;
    int fd, ret;
    uint32_t t;
    krb5_data d;

    memset(&a, 0, sizeof(a));

    d.data = rk_UNCONST(buf);
    d.length = len;
    t = _kdc_now.tv_sec;

    fd = open(fn, O_WRONLY|O_CREAT|O_APPEND, 0600);
    if (fd < 0) {
	int saved_errno = errno;
	krb5_set_error_message(context, saved_errno, "Failed to open: %s", fn);
	return saved_errno;
    }

    sp = krb5_storage_from_fd(fd);
    close(fd);
    if (sp == NULL) {
	krb5_set_error_message(context, ENOMEM, "Storage failed to open fd");
	return ENOMEM;
    }

    ret = krb5_sockaddr2address(context, sa, &a);
    if (ret)
	goto out;

    krb5_store_uint32(sp, 1);
    krb5_store_uint32(sp, t);
    krb5_store_address(sp, a);
    krb5_store_data(sp, d);
    {
	Der_class cl;
	Der_type ty;
	unsigned int tag;
	ret = der_get_tag (reply->data, reply->length,
			   &cl, &ty, &tag, NULL);
	if (ret) {
	    krb5_store_uint32(sp, 0xffffffff);
	    krb5_store_uint32(sp, 0xffffffff);
	} else {
	    krb5_store_uint32(sp, MAKE_TAG(cl, ty, 0));
	    krb5_store_uint32(sp, tag);
	}
    }

    krb5_free_address(context, &a);
out:
    krb5_storage_free(sp);

    return 0;
}
