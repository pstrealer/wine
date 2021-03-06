/*
 * Copyright 2010 Piotr Caban for CodeWeavers
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

#include <stdarg.h>

#include "msvcp90.h"
#include "stdio.h"

#include "windef.h"
#include "winbase.h"
#include "wine/debug.h"
WINE_DEFAULT_DEBUG_CHANNEL(msvcp90);

/* char_traits<char> */
/* ?assign@?$char_traits@D@std@@SAXAADABD@Z */
/* ?assign@?$char_traits@D@std@@SAXAEADAEBD@Z */
void CDECL MSVCP_char_traits_char_assign(char *ch, const char *assign)
{
    *ch = *assign;
}

/* ?eq@?$char_traits@D@std@@SA_NABD0@Z */
/* ?eq@?$char_traits@D@std@@SA_NAEBD0@Z */
MSVCP_BOOL CDECL MSVCP_char_traits_char_eq(const char *ch1, const char *ch2)
{
    return *ch1 == *ch2;
}

/* ?lt@?$char_traits@D@std@@SA_NABD0@Z */
/* ?lt@?$char_traits@D@std@@SA_NAEBD0@Z */
MSVCP_BOOL CDECL MSVCP_char_traits_lt(const char *ch1, const char *ch2)
{
    return *ch1 < *ch2;
}

/* ?compare@?$char_traits@D@std@@SAHPBD0I@Z */
/* ?compare@?$char_traits@D@std@@SAHPEBD0_K@Z */
int CDECL MSVCP_char_traits_char_compare(
        const char *s1, const char *s2, size_t count)
{
    int ret = memcmp(s1, s2, count);
    return (ret>0 ? 1 : (ret<0 ? -1 : 0));
}

/* ?length@?$char_traits@D@std@@SAIPBD@Z */
/* ?length@?$char_traits@D@std@@SA_KPEBD@Z */
size_t CDECL MSVCP_char_traits_char_length(const char *str)
{
    return strlen(str);
}

/* ?_Copy_s@?$char_traits@D@std@@SAPADPADIPBDI@Z */
/* ?_Copy_s@?$char_traits@D@std@@SAPEADPEAD_KPEBD1@Z */
char* CDECL MSVCP_char_traits_char__Copy_s(char *dest,
        size_t size, const char *src, size_t count)
{
    if(!dest || !src || size<count) {
        if(dest && size)
            dest[0] = '\0';
        _invalid_parameter(NULL, NULL, NULL, 0, 0);
        return dest;
    }

    return memcpy(dest, src, count);
}

/* ?copy@?$char_traits@D@std@@SAPADPADPBDI@Z */
/* ?copy@?$char_traits@D@std@@SAPEADPEADPEBD_K@Z */
char* CDECL MSVCP_char_traits_char_copy(
        char *dest, const char *src, size_t count)
{
    return MSVCP_char_traits_char__Copy_s(dest, count, src, count);
}

/* ?find@?$char_traits@D@std@@SAPBDPBDIABD@Z */
/* ?find@?$char_traits@D@std@@SAPEBDPEBD_KAEBD@Z */
const char * CDECL MSVCP_char_traits_char_find(
        const char *str, size_t range, const char *c)
{
    return memchr(str, *c, range);
}

/* ?_Move_s@?$char_traits@D@std@@SAPADPADIPBDI@Z */
/* ?_Move_s@?$char_traits@D@std@@SAPEADPEAD_KPEBD1@Z */
char* CDECL MSVCP_char_traits_char__Move_s(char *dest,
        size_t size, const char *src, size_t count)
{
    if(!dest || !src || size<count) {
        if(dest && size)
            dest[0] = '\0';
        _invalid_parameter(NULL, NULL, NULL, 0, 0);
        return dest;
    }

    return memmove(dest, src, count);
}

/* ?move@?$char_traits@D@std@@SAPADPADPBDI@Z */
/* ?move@?$char_traits@D@std@@SAPEADPEADPEBD_K@Z */
char* CDECL MSVCP_char_traits_char_move(
        char *dest, const char *src, size_t count)
{
    return MSVCP_char_traits_char__Move_s(dest, count, src, count);
}

/* ?assign@?$char_traits@D@std@@SAPADPADID@Z */
/* ?assign@?$char_traits@D@std@@SAPEADPEAD_KD@Z */
char* CDECL MSVCP_char_traits_char_assignn(char *str, size_t num, char c)
{
    return memset(str, c, num);
}

/* ?to_char_type@?$char_traits@D@std@@SADABH@Z */
/* ?to_char_type@?$char_traits@D@std@@SADAEBH@Z */
char CDECL MSVCP_char_traits_char_to_char_type(const int *i)
{
    return (char)*i;
}

/* ?to_int_type@?$char_traits@D@std@@SAHABD@Z */
/* ?to_int_type@?$char_traits@D@std@@SAHAEBD@Z */
int CDECL MSVCP_char_traits_char_to_int_type(const char *ch)
{
    return (int)*ch;
}

/* ?eq_int_type@?$char_traits@D@std@@SA_NABH0@Z */
/* ?eq_int_type@?$char_traits@D@std@@SA_NAEBH0@Z */
MSVCP_BOOL CDECL MSVCP_char_traits_char_eq_int_type(const int *i1, const int *i2)
{
    return *i1 == *i2;
}

/* ?eof@?$char_traits@D@std@@SAHXZ */
int CDECL MSVCP_char_traits_char_eof(void)
{
    return EOF;
}

/* ?not_eof@?$char_traits@D@std@@SAHABH@Z */
/* ?not_eof@?$char_traits@D@std@@SAHAEBH@Z */
int CDECL MSVCP_char_traits_char_not_eof(int *in)
{
    return (*in==EOF ? !EOF : *in);
}


/* char_traits<wchar_t> */
/* ?assign@?$char_traits@_W@std@@SAXAA_WAB_W@Z */
/* ?assign@?$char_traits@_W@std@@SAXAEA_WAEB_W@Z */
void CDECL MSVCP_char_traits_wchar_assign(wchar_t *ch,
        const wchar_t *assign)
{
    *ch = *assign;
}

/* ?eq@?$char_traits@_W@std@@SA_NAB_W0@Z */
/* ?eq@?$char_traits@_W@std@@SA_NAEB_W0@Z */
MSVCP_BOOL CDECL MSVCP_char_traits_wchar_eq(wchar_t *ch1, wchar_t *ch2)
{
    return *ch1 == *ch2;
}

/* ?lt@?$char_traits@_W@std@@SA_NAB_W0@Z */
/* ?lt@?$char_traits@_W@std@@SA_NAEB_W0@Z */
MSVCP_BOOL CDECL MSVCP_char_traits_wchar_lt(const wchar_t *ch1,
        const wchar_t *ch2)
{
    return *ch1 < *ch2;
}

/* ?compare@?$char_traits@_W@std@@SAHPB_W0I@Z */
/* ?compare@?$char_traits@_W@std@@SAHPEB_W0_K@Z */
int CDECL MSVCP_char_traits_wchar_compare(const wchar_t *s1,
        const wchar_t *s2, size_t count)
{
    int ret = memcmp(s1, s2, sizeof(wchar_t[count]));
    return (ret>0 ? 1 : (ret<0 ? -1 : 0));
}

/* ?length@?$char_traits@_W@std@@SAIPB_W@Z */
/* ?length@?$char_traits@_W@std@@SA_KPEB_W@Z */
size_t CDECL MSVCP_char_traits_wchar_length(const wchar_t *str)
{
    return wcslen((WCHAR*)str);
}

/* ?_Copy_s@?$char_traits@_W@std@@SAPA_WPA_WIPB_WI@Z */
/* ?_Copy_s@?$char_traits@_W@std@@SAPEA_WPEA_W_KPEB_W1@Z */
wchar_t* CDECL MSVCP_char_traits_wchar__Copy_s(wchar_t *dest,
        size_t size, const wchar_t *src, size_t count)
{
    if(!dest || !src || size<count) {
        if(dest && size)
            dest[0] = '\0';
        _invalid_parameter(NULL, NULL, NULL, 0, 0);
        return dest;
    }

    return memcpy(dest, src, sizeof(wchar_t[count]));
}

/* ?copy@?$char_traits@_W@std@@SAPA_WPA_WPB_WI@Z */
/* ?copy@?$char_traits@_W@std@@SAPEA_WPEA_WPEB_W_K@Z */
wchar_t* CDECL MSVCP_char_traits_wchar_copy(wchar_t *dest,
        const wchar_t *src, size_t count)
{
    return MSVCP_char_traits_wchar__Copy_s(dest, count, src, count);
}

/* ?find@?$char_traits@_W@std@@SAPB_WPB_WIAB_W@Z */
/* ?find@?$char_traits@_W@std@@SAPEB_WPEB_W_KAEB_W@Z */
const wchar_t* CDECL MSVCP_char_traits_wchar_find(
        const wchar_t *str, size_t range, const wchar_t *c)
{
    size_t i=0;

    for(i=0; i<range; i++)
        if(str[i] == *c)
            return str+i;

    return NULL;
}

/* ?_Move_s@?$char_traits@_W@std@@SAPA_WPA_WIPB_WI@Z */
/* ?_Move_s@?$char_traits@_W@std@@SAPEA_WPEA_W_KPEB_W1@Z */
wchar_t* CDECL MSVCP_char_traits_wchar__Move_s(wchar_t *dest,
        size_t size, const wchar_t *src, size_t count)
{
    if(!dest || !src || size<count) {
        if(dest && size)
            dest[0] = '\0';
        _invalid_parameter(NULL, NULL, NULL, 0, 0);
        return dest;
    }

    return memmove(dest, src, sizeof(WCHAR[count]));
}

/* ?move@?$char_traits@_W@std@@SAPA_WPA_WPB_WI@Z */
/* ?move@?$char_traits@_W@std@@SAPEA_WPEA_WPEB_W_K@Z */
wchar_t* CDECL MSVCP_char_traits_wchar_move(wchar_t *dest,
        const wchar_t *src, size_t count)
{
    return MSVCP_char_traits_wchar__Move_s(dest, count, src, count);
}

/* ?assign@?$char_traits@_W@std@@SAPA_WPA_WI_W@Z */
/* ?assign@?$char_traits@_W@std@@SAPEA_WPEA_W_K_W@Z */
wchar_t* CDECL MSVCP_char_traits_wchar_assignn(wchar_t *str,
        size_t num, wchar_t c)
{
    size_t i;

    for(i=0; i<num; i++)
        str[i] = c;

    return str;
}

/* ?to_char_type@?$char_traits@_W@std@@SA_WABG@Z */
/* ?to_char_type@?$char_traits@_W@std@@SA_WAEBG@Z */
wchar_t CDECL MSVCP_char_traits_wchar_to_char_type(const unsigned short *i)
{
    return *i;
}

/* ?to_int_type@?$char_traits@_W@std@@SAGAB_W@Z */
/* ?to_int_type@?$char_traits@_W@std@@SAGAEB_W@Z */
unsigned short CDECL MSVCP_char_traits_wchar_to_int_type(const wchar_t *ch)
{
    return *ch;
}

/* ?eq_int_type@?$char_traits@_W@std@@SA_NABG0@Z */
/* ?eq_int_type@?$char_traits@_W@std@@SA_NAEBG0@Z */
MSVCP_BOOL CDECL MSVCP_char_traits_wchar_eq_int_tpe(const unsigned short *i1,
        const unsigned short *i2)
{
    return *i1 == *i2;
}

/* ?eof@?$char_traits@_W@std@@SAGXZ */
unsigned short CDECL MSVCP_char_traits_wchar_eof(void)
{
    return WEOF;
}

/* ?not_eof@?$char_traits@_W@std@@SAGABG@Z */
/* ?not_eof@?$char_traits@_W@std@@SAGAEBG@Z */
unsigned short CDECL MSVCP_char_traits_wchar_not_eof(const unsigned short *in)
{
    return (*in==WEOF ? !WEOF : *in);
}


/* char_traits<unsigned short> */
/* ?assign@?$char_traits@G@std@@SAXAAGABG@Z */
/* ?assign@?$char_traits@G@std@@SAXAEAGAEBG@Z */
void CDECL MSVCP_char_traits_short_assign(unsigned short *ch,
        const unsigned short *assign)
{
    *ch = *assign;
}

/* ?eq@?$char_traits@G@std@@SA_NABG0@Z */
/* ?eq@?$char_traits@G@std@@SA_NAEBG0@Z */
MSVCP_BOOL CDECL MSVCP_char_traits_short_eq(const unsigned short *ch1,
        const unsigned short *ch2)
{
    return *ch1 == *ch2;
}

/* ?lt@?$char_traits@G@std@@SA_NABG0@Z */
/* ?lt@?$char_traits@G@std@@SA_NAEBG0@Z */
MSVCP_BOOL CDECL MSVCP_char_traits_short_lt(const unsigned short *ch1,
        const unsigned short *ch2)
{
    return *ch1 < *ch2;
}

/* ?compare@?$char_traits@G@std@@SAHPBG0I@Z */
/* ?compare@?$char_traits@G@std@@SAHPEBG0_K@Z */
int CDECL MSVCP_char_traits_short_compare(const unsigned short *s1,
        const unsigned short *s2, size_t count)
{
    size_t i;

    for(i=0; i<count; i++)
        if(s1[i] != s2[i])
            return (s1[i] < s2[i] ? -1 : 1);

    return 0;
}

/* ?length@?$char_traits@G@std@@SAIPBG@Z */
/* ?length@?$char_traits@G@std@@SA_KPEBG@Z */
size_t CDECL MSVCP_char_traits_short_length(const unsigned short *str)
{
    size_t len;

    for(len=0; str[len]; len++);

    return len;
}

/* ?_Copy_s@?$char_traits@G@std@@SAPAGPAGIPBGI@Z */
/* ?_Copy_s@?$char_traits@G@std@@SAPEAGPEAG_KPEBG1@Z */
unsigned short * CDECL MSVCP_char_traits_short__Copy_s(unsigned short *dest,
        size_t size, const unsigned short *src, size_t count)
{
    if(size<count) {
        _invalid_parameter(NULL, NULL, NULL, 0, 0);
        return dest;
    }

    return memcpy(dest, src, sizeof(unsigned short[count]));
}

/* ?copy@?$char_traits@G@std@@SAPAGPAGPBGI@Z */
/* ?copy@?$char_traits@G@std@@SAPEAGPEAGPEBG_K@Z */
unsigned short* CDECL MSVCP_char_traits_short_copy(unsigned short *dest,
        const unsigned short *src, size_t count)
{
    return MSVCP_char_traits_short__Copy_s(dest, count, src, count);
}

/* ?find@?$char_traits@G@std@@SAPBGPBGIABG@Z */
/* ?find@?$char_traits@G@std@@SAPEBGPEBG_KAEBG@Z */
const unsigned short* CDECL MSVCP_char_traits_short_find(
        const unsigned short *str, size_t range, const unsigned short *c)
{
    size_t i;

    for(i=0; i<range; i++)
        if(str[i] == *c)
            return str+i;

    return NULL;
}

/* ?_Move_s@?$char_traits@G@std@@SAPAGPAGIPBGI@Z */
/* ?_Move_s@?$char_traits@G@std@@SAPEAGPEAG_KPEBG1@Z */
unsigned short* CDECL MSVCP_char_traits_short__Move_s(unsigned short *dest,
        size_t size, const unsigned short *src, size_t count)
{
    if(size<count) {
        _invalid_parameter(NULL, NULL, NULL, 0, 0);
        return dest;
    }

    return memmove(dest, src, sizeof(unsigned short[count]));
}

/* ?move@?$char_traits@G@std@@SAPAGPAGPBGI@Z */
/* ?move@?$char_traits@G@std@@SAPEAGPEAGPEBG_K@Z */
unsigned short* CDECL MSVCP_char_traits_short_move(unsigned short *dest,
        const unsigned short *src, size_t count)
{
    return MSVCP_char_traits_short__Move_s(dest, count, src, count);
}

/* ?assign@?$char_traits@G@std@@SAPAGPAGIG@Z */
/* ?assign@?$char_traits@G@std@@SAPEAGPEAG_KG@Z */
unsigned short* CDECL MSVCP_char_traits_short_assignn(unsigned short *str,
        size_t num, unsigned short c)
{
    size_t i;

    for(i=0; i<num; i++)
        str[i] = c;

    return str;
}

/* ?to_char_type@?$char_traits@G@std@@SAGABG@Z */
/* ?to_char_type@?$char_traits@G@std@@SAGAEBG@Z */
unsigned short CDECL MSVCP_char_traits_short_to_char_type(const unsigned short *i)
{
    return *i;
}

/* ?to_int_type@?$char_traits@G@std@@SAGABG@Z */
/* ?to_int_type@?$char_traits@G@std@@SAGAEBG@Z */
unsigned short CDECL MSVCP_char_traits_short_to_int_type(const unsigned short *ch)
{
    return *ch;
}

/* ?eq_int_type@?$char_traits@G@std@@SA_NABG0@Z */
/* ?eq_int_type@?$char_traits@G@std@@SA_NAEBG0@Z */
MSVCP_BOOL CDECL MSVCP_char_traits_short_eq_int_type(unsigned short *i1,
        unsigned short *i2)
{
    return *i1 == *i2;
}

/* ?eof@?$char_traits@G@std@@SAGXZ */
unsigned short CDECL MSVCP_char_traits_short_eof(void)
{
    return -1;
}

/* ?not_eof@?$char_traits@G@std@@SAGABG@Z */
/* ?not_eof@?$char_traits@G@std@@SAGAEBG@Z */
unsigned short CDECL MSVCP_char_traits_short_not_eof(const unsigned short *in)
{
    return (*in==(unsigned short)-1 ? 0 : *in);
}
