/*
 *    Gameux library coclass GameExplorer implementation
 *
 * Copyright (C) 2010 Mariusz Pluciński
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
#define COBJMACROS

#include "config.h"

#include "ole2.h"

#include "gameux.h"
#include "gameux_private.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(gameux);

/*
 * IGameExplorer implementation
 */

typedef struct _GameExplorerImpl
{
    const struct IGameExplorerVtbl *lpGameExplorerVtbl;
    LONG ref;
} GameExplorerImpl;

static inline GameExplorerImpl *impl_from_IGameExplorer(IGameExplorer *iface)
{
    return (GameExplorerImpl*)((char*)iface - FIELD_OFFSET(GameExplorerImpl, lpGameExplorerVtbl));
}

static HRESULT WINAPI GameExplorerImpl_QueryInterface(
        IGameExplorer *iface,
        REFIID riid,
        void **ppvObject)
{
    GameExplorerImpl *This = impl_from_IGameExplorer(iface);

    TRACE("(%p, %s, %p)\n", This, debugstr_guid(riid), ppvObject);

    *ppvObject = NULL;

    if(IsEqualGUID(riid, &IID_IUnknown) ||
       IsEqualGUID(riid, &IID_IGameExplorer))
    {
        *ppvObject = iface;
    }
    else
    {
        FIXME("interface %s not implemented\n", debugstr_guid(riid));
        return E_NOINTERFACE;
    }

    IGameExplorer_AddRef(iface);
    return S_OK;
}

static ULONG WINAPI GameExplorerImpl_AddRef(IGameExplorer *iface)
{
    GameExplorerImpl *This = impl_from_IGameExplorer(iface);
    LONG ref;

    ref = InterlockedIncrement(&This->ref);

    TRACE("(%p): ref=%d\n", This, ref);
    return ref;
}

static ULONG WINAPI GameExplorerImpl_Release(IGameExplorer *iface)
{
    GameExplorerImpl *This = impl_from_IGameExplorer(iface);
    LONG ref;

    ref = InterlockedDecrement(&This->ref);
    TRACE("(%p): ref=%d\n", This, ref);

    if(ref == 0)
    {
        TRACE("freeing GameExplorer object\n");
        HeapFree(GetProcessHeap(), 0, This);
    }

    return ref;
}

static HRESULT WINAPI GameExplorerImpl_AddGame(
        IGameExplorer *iface,
        BSTR bstrGDFBinaryPath,
        BSTR sGameInstallDirectory,
        GAME_INSTALL_SCOPE installScope,
        GUID *pInstanceID)
{
    GameExplorerImpl *This = impl_from_IGameExplorer(iface);

    TRACE("(%p, %s, %s, %d, %s)\n", This, debugstr_w(bstrGDFBinaryPath), debugstr_w(sGameInstallDirectory), installScope, debugstr_guid(pInstanceID));
    FIXME("stub\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI GameExplorerImpl_RemoveGame(
        IGameExplorer *iface,
        GUID instanceID)
{
    GameExplorerImpl *This = impl_from_IGameExplorer(iface);

    TRACE("(%p, %s)\n", This, debugstr_guid(&instanceID));
    FIXME("stub\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI GameExplorerImpl_UpdateGame(
        IGameExplorer *iface,
        GUID instanceID)
{
    GameExplorerImpl *This = impl_from_IGameExplorer(iface);

    TRACE("(%p, %s)\n", This, debugstr_guid(&instanceID));
    FIXME("stub\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI GameExplorerImpl_VerifyAccess(
        IGameExplorer *iface,
        BSTR sGDFBinaryPath,
        BOOL *pHasAccess)
{
    GameExplorerImpl *This = impl_from_IGameExplorer(iface);

    TRACE("(%p, %s, %p)\n", This, debugstr_w(sGDFBinaryPath), pHasAccess);
    FIXME("stub\n");
    return E_NOTIMPL;
}

static const struct IGameExplorerVtbl GameExplorerImplVtbl =
{
    GameExplorerImpl_QueryInterface,
    GameExplorerImpl_AddRef,
    GameExplorerImpl_Release,
    GameExplorerImpl_AddGame,
    GameExplorerImpl_RemoveGame,
    GameExplorerImpl_UpdateGame,
    GameExplorerImpl_VerifyAccess
};

/*
 * Construction routine
 */
HRESULT GameExplorer_create(
        IUnknown* pUnkOuter,
        IUnknown** ppObj)
{
    GameExplorerImpl *pGameExplorer;

    TRACE("(%p, %p)\n", pUnkOuter, ppObj);

    pGameExplorer = HeapAlloc(GetProcessHeap(), 0, sizeof(*pGameExplorer));

    if(!pGameExplorer)
        return E_OUTOFMEMORY;

    pGameExplorer->lpGameExplorerVtbl = &GameExplorerImplVtbl;
    pGameExplorer->ref = 1;

    *ppObj = (IUnknown*)(&pGameExplorer->lpGameExplorerVtbl);

    TRACE("returning iface: %p\n", *ppObj);
    return S_OK;
}
