#include<xtl.h>

#ifndef PPCOPTS_H
#define PPCOPTS_H

static __inline int isel( int a, int x, int y )
{
    int mask = (a >> 31); // arithmetic shift right, splat out the sign bit
    // mask is 0xFFFFFFFF if (a < 0) and 0x00 otherwise.
    return (x & (~mask)) + (y & mask);
};

template <typename T> FORCEINLINE T VariableShiftLeft(T nVal, int nShift)
{   // 31-bit shift capability (Rolls over at 32-bits)
    const int bMask1=-(1&nShift);
    const int bMask2=-(1&(nShift>>1));
    const int bMask3=-(1&(nShift>>2));
    const int bMask4=-(1&(nShift>>3));
    const int bMask5=-(1&(nShift>>4));
    nVal=(nVal&bMask1) + nVal;   //nVal=((nVal<<1)&bMask1) | (nVal&(~bMask1));
    nVal=((nVal<<(1<<1))&bMask2) | (nVal&(~bMask2));
    nVal=((nVal<<(1<<2))&bMask3) | (nVal&(~bMask3));
    nVal=((nVal<<(1<<3))&bMask4) | (nVal&(~bMask4));
    nVal=((nVal<<(1<<4))&bMask5) | (nVal&(~bMask5));
    return(nVal);
}
template <typename T> FORCEINLINE T VariableShiftRight(T nVal, int nShift)
{   // 31-bit shift capability (Rolls over at 32-bits)
    const int bMask1=-(1&nShift);
    const int bMask2=-(1&(nShift>>1));
    const int bMask3=-(1&(nShift>>2));
    const int bMask4=-(1&(nShift>>3));
    const int bMask5=-(1&(nShift>>4));
    nVal=((nVal>>1)&bMask1) | (nVal&(~bMask1));
    nVal=((nVal>>(1<<1))&bMask2) | (nVal&(~bMask2));
    nVal=((nVal>>(1<<2))&bMask3) | (nVal&(~bMask3));
    nVal=((nVal>>(1<<3))&bMask4) | (nVal&(~bMask4));
    nVal=((nVal>>(1<<4))&bMask5) | (nVal&(~bMask5));
    return(nVal);
}


// Stats about the L2 cache:
const DWORD         g_dwCacheLineSize = 128;
const g_dwL2CacheSize = 1024 * 1024;
const g_dwL2CacheWays = 8;

const DWORD g_dwDefaultBufferSizes[] =
{
    64 * 1024,
    4 * 1024 * 1024, // last must be largest
};
const DWORD         g_dwLargestBufferSize = g_dwDefaultBufferSizes[ARRAYSIZE( g_dwDefaultBufferSizes ) - 1];
static DWORD        g_dwBufferSizeIndex = 0;

template <typename t_type> static __inline BOOL AlignedToPowerOf2( const t_type& t, DWORD dwPowerOf2 )
{
    return ( ( ( DWORD )t ) & ( dwPowerOf2 - 1 ) ) == 0;
}

template <typename t_type> static __inline t_type RoundDownToPowerOf2( const t_type& t, DWORD dwPowerOf2 )
{
    return ( t_type )( ( ( DWORD )t ) & ~( dwPowerOf2 - 1 ) );
}

template <typename t_type> static __inline t_type RoundUpToPowerOf2( const t_type& t, DWORD dwPowerOf2 )
{
    return ( t_type )( ( ( ( DWORD )t ) + ( dwPowerOf2 - 1 ) ) & ~( dwPowerOf2 - 1 ) );
}
 
// Kick data out of the cache
static void Purge( const char* pData, DWORD dwSize )
{
    // Expand input range to be cache-line aligned:
    const char* pEnd = pData + dwSize;
    pData = RoundDownToPowerOf2( pData, g_dwCacheLineSize );
    dwSize = RoundUpToPowerOf2( pEnd, g_dwCacheLineSize ) - pData;
 
    for( size_t i = 0; i < dwSize; i += g_dwCacheLineSize )
    {
        __dcbf( i, pData );
    }
}
 
// Kick data into the cache. 
static void Prefetch( const char* pData, DWORD dwSize )
{
    // Expand input range to be cache-line aligned:
    const char* pEnd = pData + dwSize;
    pData = RoundDownToPowerOf2( pData, g_dwCacheLineSize );
    dwSize = RoundUpToPowerOf2( pEnd, g_dwCacheLineSize ) - pData;
 
    for( size_t i = 0; i < dwSize; i += g_dwCacheLineSize )
    {
        __dcbt( i, pData );
    }
}
 
// Initialize the cache to zero. 
static void PreZero( char* pData, DWORD dwSize )
{

    for( size_t i = 0; i < dwSize; i += g_dwCacheLineSize )
    {
        __dcbz( i, pData );
    }
}

#endif