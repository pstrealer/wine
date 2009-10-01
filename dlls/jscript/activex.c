/*
 * Copyright 2009 Jacek Caban for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"
#include "wine/port.h"

#include "jscript.h"
#include "objsafe.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(jscript);

/* Defined as extern in urlmon.idl, but not exported by uuid.lib */
const GUID GUID_CUSTOM_CONFIRMOBJECTSAFETY =
    {0x10200490,0xfa38,0x11d0,{0xac,0x0e,0x00,0xa0,0xc9,0xf,0xff,0xc0}};

static IInternetHostSecurityManager *get_sec_mgr(script_ctx_t *ctx)
{
    IInternetHostSecurityManager *secmgr;
    IServiceProvider *sp;
    HRESULT hres;

    if(!ctx->site)
        return NULL;

    if(ctx->secmgr)
        return ctx->secmgr;

    hres = IActiveScriptSite_QueryInterface(ctx->site, &IID_IServiceProvider, (void**)&sp);
    if(FAILED(hres))
        return NULL;

    hres = IServiceProvider_QueryService(sp, &SID_SInternetHostSecurityManager, &IID_IInternetHostSecurityManager,
            (void**)&secmgr);
    IServiceProvider_Release(sp);
    if(FAILED(hres))
        return NULL;

    return ctx->secmgr = secmgr;
}

static IUnknown *create_activex_object(script_ctx_t *ctx, const WCHAR *progid)
{
    IInternetHostSecurityManager *secmgr;
    struct CONFIRMSAFETY cs;
    DWORD policy_size;
    BYTE *bpolicy;
    IUnknown *obj;
    DWORD policy;
    GUID guid;
    HRESULT hres;

    hres = CLSIDFromProgID(progid, &guid);
    if(FAILED(hres))
        return NULL;

    TRACE("GUID %s\n", debugstr_guid(&guid));

    secmgr = get_sec_mgr(ctx);
    if(!secmgr)
        return NULL;

    policy = 0;
    hres = IInternetHostSecurityManager_ProcessUrlAction(secmgr, URLACTION_ACTIVEX_RUN, (BYTE*)&policy, sizeof(policy),
            (BYTE*)&guid, sizeof(GUID), 0, 0);
    if(FAILED(hres) || policy != URLPOLICY_ALLOW)
        return NULL;

    /* FIXME: Use IClassFactoryEx */

    hres = CoCreateInstance(&guid, NULL, CLSCTX_INPROC_SERVER|CLSCTX_INPROC_HANDLER, &IID_IUnknown, (void**)&obj);
    if(FAILED(hres))
        return NULL;

    cs.clsid = guid;
    cs.pUnk = obj;
    cs.dwFlags = 0;
    hres = IInternetHostSecurityManager_QueryCustomPolicy(secmgr, &GUID_CUSTOM_CONFIRMOBJECTSAFETY, &bpolicy, &policy_size,
            (BYTE*)&cs, sizeof(cs), 0);
    if(SUCCEEDED(hres)) {
        policy = policy_size >= sizeof(DWORD) ? *(DWORD*)bpolicy : URLPOLICY_DISALLOW;
        CoTaskMemFree(bpolicy);
    }

    if(FAILED(hres) || policy != URLPOLICY_ALLOW) {
        IUnknown_Release(obj);
        return NULL;
    }

    return obj;
}

static HRESULT ActiveXObject_value(script_ctx_t *ctx, vdisp_t *jsthis, WORD flags, DISPPARAMS *dp,
        VARIANT *retv, jsexcept_t *ei, IServiceProvider *caller)
{
    IDispatch *disp;
    IUnknown *obj;
    BSTR progid;
    HRESULT hres;

    TRACE("\n");

    if(flags != DISPATCH_CONSTRUCT) {
        FIXME("unsupported flags %x\n", flags);
        return E_NOTIMPL;
    }

    if(ctx->safeopt != (INTERFACESAFE_FOR_UNTRUSTED_DATA|INTERFACE_USES_DISPEX|INTERFACE_USES_SECURITY_MANAGER)) {
        FIXME("Unsupported safeopt %x\n", ctx->safeopt);
        return E_NOTIMPL;
    }

    if(arg_cnt(dp) != 1) {
        FIXME("unsuported arg_cnt %d\n", arg_cnt(dp));
        return E_NOTIMPL;
    }

    hres = to_string(ctx, get_arg(dp,0), ei, &progid);
    if(FAILED(hres))
        return hres;

    obj = create_activex_object(ctx, progid);
    SysFreeString(progid);
    if(!obj)
        return throw_generic_error(ctx, ei, IDS_CREATE_OBJ_ERROR, NULL);

    hres = IUnknown_QueryInterface(obj, &IID_IDispatch, (void**)&disp);
    IUnknown_Release(obj);
    if(FAILED(hres)) {
        FIXME("Object does not support IDispatch\n");
        return E_NOTIMPL;
    }

    V_VT(retv) = VT_DISPATCH;
    V_DISPATCH(retv) = disp;
    return S_OK;
}

HRESULT create_activex_constr(script_ctx_t *ctx, DispatchEx **ret)
{
    DispatchEx *prototype;
    HRESULT hres;

    static const WCHAR ActiveXObjectW[] = {'A','c','t','i','v','e','X','O','b','j','e','c','t',0};

    hres = create_object(ctx, NULL, &prototype);
    if(FAILED(hres))
        return hres;

    hres = create_builtin_function(ctx, ActiveXObject_value, ActiveXObjectW, NULL, PROPF_CONSTR, prototype, ret);

    jsdisp_release(prototype);
    return hres;
}