/*
 * Copyright (c) 1997-2001, 2003, 2005-2006 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
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

#include "kadm5_locl.h"

RCSID("$Id$");

struct modify_principal_hook_ctx {
    kadm5_server_context *context;
    enum kadm5_hook_stage stage;
    krb5_error_code code;
    kadm5_principal_ent_t princ;
    uint32_t mask;
};

static krb5_error_code KRB5_LIB_CALL
modify_principal_hook_cb(krb5_context context,
			 const void *hook,
			 void *hookctx,
			 void *userctx)
{
    krb5_error_code ret;
    const struct kadm5_hook_ftable *ftable = hook;
    struct modify_principal_hook_ctx *ctx = userctx;

    ret = ftable->modify(context, hookctx, ctx->stage,
			 ctx->code, ctx->princ, ctx->mask);
    if (ret != 0 && ret != KRB5_PLUGIN_NO_HANDLE)
	_kadm5_s_set_hook_error_message(ctx->context, ret, "modify",
					hook, ctx->stage);

    /* only pre-commit plugins can abort */
    if (ret == 0 || ctx->stage == KADM5_HOOK_STAGE_POSTCOMMIT)
	ret = KRB5_PLUGIN_NO_HANDLE;

    return ret;
}

static kadm5_ret_t
modify_principal_hook(kadm5_server_context *context,
		      enum kadm5_hook_stage stage,
		      krb5_error_code code,
		      kadm5_principal_ent_t princ,
		      uint32_t mask)
{
    krb5_error_code ret;
    struct modify_principal_hook_ctx ctx;

    ctx.context = context;
    ctx.stage = stage;
    ctx.code = code;
    ctx.princ = princ;
    ctx.mask = mask;

    ret = _krb5_plugin_run_f(context->context, &kadm5_hook_plugin_data,
			     0, &ctx, modify_principal_hook_cb);
    if (ret == KRB5_PLUGIN_NO_HANDLE)
	ret = 0;

    return ret;
}

static kadm5_ret_t
modify_principal(void *server_handle,
		 kadm5_principal_ent_t princ,
		 uint32_t mask,
		 uint32_t forbidden_mask)
{
    kadm5_server_context *context = server_handle;
    hdb_entry_ex ent;
    kadm5_ret_t ret;

    memset(&ent, 0, sizeof(ent));

    if((mask & forbidden_mask))
	return KADM5_BAD_MASK;
    if((mask & KADM5_POLICY) && strcmp(princ->policy, "default") != 0)
	return KADM5_UNK_POLICY;

    if (!context->keep_open) {
	ret = context->db->hdb_open(context->context, context->db, O_RDWR, 0);
	if(ret)
	    return ret;
    }

    ret = kadm5_log_init(context);
    if (ret)
        goto out;

    /*
     * NOTE: We do not use hdb_fetch_kvno() here, which means we'll
     *       automatically reject modifications of would-be virtual principals.
     */
    ret = context->db->hdb_fetch_kvno(context->context, context->db,
				      princ->principal,
                                      HDB_F_DECRYPT|HDB_F_GET_ANY|HDB_F_ADMIN_DATA,
                                      0, &ent);
    if (ret)
	goto out2;

    ret = modify_principal_hook(context, KADM5_HOOK_STAGE_PRECOMMIT,
				0, princ, mask);
    if (ret)
	goto out3;
    /*
     * XXX Make sure that _kadm5_setup_entry() checks that the time of last
     * change in `ent' matches the one in `princ'.
     */
    ret = _kadm5_setup_entry(context, &ent, mask, princ, mask, NULL, 0);
    if (ret)
	goto out3;
    ret = _kadm5_set_modifier(context, &ent.entry);
    if (ret)
	goto out3;

    /*
     * If any keys are bogus, disallow the modify.  If the keys were
     * bogus as stored in the HDB we could allow those through, but
     * distinguishing that case from a pre-1.6 client using add_enctype
     * without the get-keys privilege requires more work (mainly: checking that
     * the bogus keys in princ->key_data[] have corresponding bogus keys in ent
     * before calling _kadm5_setup_entry()).
     */
    if ((mask & KADM5_KEY_DATA) &&
	kadm5_some_keys_are_bogus(princ->n_key_data, princ->key_data)) {
	ret = KADM5_AUTH_GET_KEYS; /* Not quite appropriate, but it'll do */
	goto out3;
    }

    ret = hdb_seal_keys(context->context, context->db, &ent.entry);
    if (ret)
	goto out3;

    if ((mask & KADM5_POLICY)) {
	HDB_extension ext;

        memset(&ext, 0, sizeof(ext));
        /* XXX should be TRUE, but we don't yet support policies */
        ext.mandatory = FALSE;
	ext.data.element = choice_HDB_extension_data_policy;
	ext.data.u.policy = strdup(princ->policy);
	if (ext.data.u.policy == NULL) {
	    ret = krb5_enomem(context->context);
	    goto out3;
	}
	/* This calls free_HDB_extension(), freeing ext.data.u.policy */
	ret = hdb_replace_extension(context->context, &ent.entry, &ext);
        free(ext.data.u.policy);
	if (ret)
	    goto out3;
    }

    /* This logs the change for iprop and writes to the HDB */
    ret = kadm5_log_modify(context, &ent.entry,
                           mask | KADM5_MOD_NAME | KADM5_MOD_TIME);

    (void) modify_principal_hook(context, KADM5_HOOK_STAGE_POSTCOMMIT,
				 ret, princ, mask);

 out3:
    hdb_free_entry(context->context, &ent);
 out2:
    (void) kadm5_log_end(context);
 out:
    if (!context->keep_open) {
        kadm5_ret_t ret2;
        ret2 = context->db->hdb_close(context->context, context->db);
        if (ret == 0 && ret2 != 0)
            ret = ret2;
    }
    return _kadm5_error_code(ret);
}


kadm5_ret_t
kadm5_s_modify_principal(void *server_handle,
			 kadm5_principal_ent_t princ,
			 uint32_t mask)
{
    return modify_principal(server_handle, princ, mask,
			    KADM5_LAST_PWD_CHANGE | KADM5_MOD_TIME
			    | KADM5_MOD_NAME | KADM5_MKVNO
			    | KADM5_AUX_ATTRIBUTES | KADM5_LAST_SUCCESS
			    | KADM5_LAST_FAILED);
}
