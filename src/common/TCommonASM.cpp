/*
**   Helper methods for TIVTC and TDeint
**
**
**   Copyright (C) 2004-2007 Kevin Stone, additional work (C) 2020 pinterf
**
**   This program is free software; you can redistribute it and/or modify
**   it under the terms of the GNU General Public License as published by
**   the Free Software Foundation; either version 2 of the License, or
**   (at your option) any later version.
**
**   This program is distributed in the hope that it will be useful,
**   but WITHOUT ANY WARRANTY; without even the implied warranty of
**   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**   GNU General Public License for more details.
**
**   You should have received a copy of the GNU General Public License
**   along with this program; if not, write to the Free Software
**   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "TCommonASM.h"
#include "emmintrin.h"
#include <algorithm>

void absDiff_SSE2(const unsigned char *srcp1, const unsigned char *srcp2,
  unsigned char *dstp, int src1_pitch, int src2_pitch, int dst_pitch, int width,
  int height, int mthresh1, int mthresh2)
{
  mthresh1 = std::min(std::max(255 - mthresh1, 0), 255);
  mthresh2 = std::min(std::max(255 - mthresh2, 0), 255);

  static const auto onesMask = _mm_set1_epi8(1);
  static const auto sthresh = _mm_set1_epi16((mthresh2 << 8) + mthresh1);
  static const auto all_ff = _mm_set1_epi8(-1);
  for (int y = 0; y < height; ++y)
  {
    for (int x = 0; x < width; x += 16)
    {
      auto src1 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp1 + x));
      auto src2 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp2 + x));
      auto diff12 = _mm_subs_epu8(src1, src2);
      auto diff21 = _mm_subs_epu8(src2, src1);
      auto diff = _mm_or_si128(diff12, diff21);
      auto addedsthresh = _mm_adds_epu8(diff, sthresh);
      auto cmpresult = _mm_cmpeq_epi8(addedsthresh, all_ff);
      auto res = _mm_xor_si128(cmpresult, all_ff);
      auto tmp = _mm_and_si128(res, onesMask);
      _mm_store_si128(reinterpret_cast<__m128i*>(dstp + x), tmp);
      /*
      if (abs(srcp1[x] - srcp2[x]) < mthresh1) dstp[x] = 1;
      else dstp[x] = 0;
      ++x;
      if (abs(srcp1[x] - srcp2[x]) < mthresh2) dstp[x] = 1;
      else dstp[x] = 0;
      */
    }
    srcp1 += src1_pitch;
    srcp2 += src2_pitch;
    dstp += dst_pitch;
  }

}

void absDiff_c(const unsigned char* srcp1, const unsigned char* srcp2,
  unsigned char* dstp, int src1_pitch, int src2_pitch, int dst_pitch, int width,
  int height, int mthresh1, int mthresh2)
{
  for (int y = 0; y < height; ++y)
  {
    for (int x = 0; x < width; ++x)
    {
      if (abs(srcp1[x] - srcp2[x]) < mthresh1) dstp[x] = 1;
      else dstp[x] = 0;
      ++x;
      if (abs(srcp1[x] - srcp2[x]) < mthresh2) dstp[x] = 1;
      else dstp[x] = 0;
    }
    srcp1 += src1_pitch;
    srcp2 += src2_pitch;
    dstp += dst_pitch;
  }
}


// different path if not mod16, but only for remaining 8 bytes
void buildABSDiffMask_SSE2(const unsigned char* prvp, const unsigned char* nxtp,
  unsigned char* dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width,
  int height)
{
  if (!(width & 15)) // exact mod16
  {
    while (height--) {
      for (int x = 0; x < width; x += 16)
      {
        __m128i src_prev = _mm_load_si128(reinterpret_cast<const __m128i*>(prvp + x));
        __m128i src_next = _mm_load_si128(reinterpret_cast<const __m128i*>(nxtp + x));
        __m128i diffpn = _mm_subs_epu8(src_prev, src_next);
        __m128i diffnp = _mm_subs_epu8(src_next, src_prev);
        __m128i diff = _mm_or_si128(diffpn, diffnp);
        _mm_store_si128(reinterpret_cast<__m128i*>(dstp + x), diff);
      }
      prvp += prv_pitch;
      nxtp += nxt_pitch;
      dstp += dst_pitch;
    }
  }
  else {
    width -= 8; // last chunk is 8 bytes instead of 16
    while (height--) {
      int x;
      for (x = 0; x < width; x += 16)
      {
        __m128i src_prev = _mm_load_si128(reinterpret_cast<const __m128i*>(prvp + x));
        __m128i src_next = _mm_load_si128(reinterpret_cast<const __m128i*>(nxtp + x));
        __m128i diffpn = _mm_subs_epu8(src_prev, src_next);
        __m128i diffnp = _mm_subs_epu8(src_next, src_prev);
        __m128i diff = _mm_or_si128(diffpn, diffnp);
        _mm_store_si128(reinterpret_cast<__m128i*>(dstp + x), diff);
      }
      __m128i src_prev = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(prvp + x));
      __m128i src_next = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(nxtp + x));
      __m128i diffpn = _mm_subs_epu8(src_prev, src_next);
      __m128i diffnp = _mm_subs_epu8(src_next, src_prev);
      __m128i diff = _mm_or_si128(diffpn, diffnp);
      _mm_storel_epi64(reinterpret_cast<__m128i*>(dstp + x), diff);
      prvp += prv_pitch;
      nxtp += nxt_pitch;
      dstp += dst_pitch;
    }
  }
}


template<bool YUY2_LumaOnly>
void buildABSDiffMask_c(const unsigned char* prvp, const unsigned char* nxtp,
  unsigned char* dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width, int height)
{
  if (width <= 0)
    return;

  if constexpr (YUY2_LumaOnly) {
    // TFM would use with ignoreYUY2chroma == true
    // where (vi.IsYUY2() && !mChroma)
    // Why: C version is quicker if dealing with every second (luma) pixel
    // (remark in 2020: only if compiler would not vectorize it
    // SSE2: no template because with omitting chroma is slower.
    // YUY2: YUYVYUYV... skip U and V
    for (int y = 0; y < height; ++y)
    {
      for (int x = 0; x < width; x += 4)
      {
        dstp[x + 0] = abs(prvp[x + 0] - nxtp[x + 0]);
        dstp[x + 2] = abs(prvp[x + 2] - nxtp[x + 2]);
      }
      prvp += prv_pitch;
      nxtp += nxt_pitch;
      dstp += dst_pitch;
    }
  }
  else {
    for (int y = 0; y < height; ++y)
    {
      for (int x = 0; x < width; x += 4)
      {
        dstp[x + 0] = abs(prvp[x + 0] - nxtp[x + 0]);
        dstp[x + 1] = abs(prvp[x + 1] - nxtp[x + 1]);
        dstp[x + 2] = abs(prvp[x + 2] - nxtp[x + 2]);
        dstp[x + 3] = abs(prvp[x + 3] - nxtp[x + 3]);
      }
      prvp += prv_pitch;
      nxtp += nxt_pitch;
      dstp += dst_pitch;
    }
  }
}

void do_buildABSDiffMask(const unsigned char* prvp, const unsigned char* nxtp, unsigned char* tbuffer,
  int prv_pitch, int nxt_pitch, int tpitch, int width, int height, bool YUY2_LumaOnly, int cpuFlags)
{
  if (cpuFlags & CPUF_SSE2 && width >= 8)
  {
    const int mod8Width = width / 8 * 8;
    // SSE2 is not chroma-ignore template, it's quicker if not skipping each YUY2 chroma
    buildABSDiffMask_SSE2(prvp, nxtp, tbuffer, prv_pitch, nxt_pitch, tpitch, mod8Width, height);
    if(YUY2_LumaOnly)
      buildABSDiffMask_c<true>(prvp + mod8Width, nxtp + mod8Width, tbuffer + mod8Width, prv_pitch, nxt_pitch, tpitch, width - mod8Width, height);
    else
      buildABSDiffMask_c<false>(prvp + mod8Width, nxtp + mod8Width, tbuffer + mod8Width, prv_pitch, nxt_pitch, tpitch, width - mod8Width, height);
  }
  else {
    if (YUY2_LumaOnly)
      buildABSDiffMask_c<true>(prvp, nxtp, tbuffer, prv_pitch, nxt_pitch, tpitch, width, height);
    else
      buildABSDiffMask_c<false>(prvp, nxtp, tbuffer, prv_pitch, nxt_pitch, tpitch, width, height);
  }
}


void do_buildABSDiffMask2(const unsigned char* prvp, const unsigned char* nxtp, unsigned char* dstp,
  int prv_pitch, int nxt_pitch, int dst_pitch, int width, int height, bool YUY2_LumaOnly, int cpuFlags)
{
  if ((cpuFlags & CPUF_SSE2) && width >= 8)
  {
    int mod8Width = width / 8 * 8;
    buildABSDiffMask2_SSE2(prvp, nxtp, dstp, prv_pitch, nxt_pitch, dst_pitch, mod8Width, height);
    if (YUY2_LumaOnly)
      buildABSDiffMask2_c<true>(prvp + mod8Width, nxtp + mod8Width, dstp + mod8Width, prv_pitch, nxt_pitch, dst_pitch, width - mod8Width, height);
    else
      buildABSDiffMask2_c<false>(prvp + mod8Width, nxtp + mod8Width, dstp + mod8Width, prv_pitch, nxt_pitch, dst_pitch, width - mod8Width, height);
  }
  else {
    if (YUY2_LumaOnly)
      buildABSDiffMask2_c<true>(prvp, nxtp, dstp, prv_pitch, nxt_pitch, dst_pitch, width, height);
    else
      buildABSDiffMask2_c<false>(prvp, nxtp, dstp, prv_pitch, nxt_pitch, dst_pitch, width, height);
  }
}


// TDeint and TFM version
// fixme: AnalyzeDiffMask_Planar compare with AnalyzeDiffMask_YUY2
void AnalyzeDiffMask_Planar(unsigned char* dstp, int dst_pitch, unsigned char* tbuffer, int tpitch, int Width, int Height)
{
  const unsigned char* dppp = tbuffer - tpitch;
  const unsigned char* dpp = tbuffer;
  const unsigned char* dp = tbuffer + tpitch;
  const unsigned char* dpn = tbuffer + tpitch * 2;
  const unsigned char* dpnn = tbuffer + tpitch * 3;
  int count;
  bool upper, lower, upper2, lower2;

  for (int y = 2; y < Height - 2; y += 2) {
    for (int x = 1; x < Width - 1; x++) {
      int eax, esi, edi, edx;

      if (dp[x] <= 3) continue;
      if (dp[x - 1] <= 3 && dp[x + 1] <= 3 &&
        dpp[x - 1] <= 3 && dpp[x] <= 3 && dpp[x + 1] <= 3 &&
        dpn[x - 1] <= 3 && dpn[x] <= 3 && dpn[x + 1] <= 3) continue;
      dstp[x]++;
      if (dp[x] <= 19) continue;

      edi = 0;
      lower = 0;
      upper = 0;

      if (dpp[x - 1] > 19) edi++;
      if (dpp[x] > 19) edi++;
      if (dpp[x + 1] > 19) edi++;

      if (edi != 0) upper = 1;

      if (dp[x - 1] > 19) edi++;
      if (dp[x + 1] > 19) edi++;

      esi = edi;

      if (dpn[x - 1] > 19) edi++;
      if (dpn[x] > 19) edi++;
      if (dpn[x + 1] > 19) edi++;

      if (edi <= 2) continue;

      count = edi;
      if (count != esi) {
        lower = 1;
        if (upper != 0) {
          dstp[x] += 2;
          continue;
        }
      }

      eax = x - 4;
      if (eax < 0) eax = 0;

      edx = x + 5;
      lower2 = 0;
      upper2 = 0;
      if (edx > Width) edx = Width;
      if (y != 2) {
        int esi = eax;
        do {
          if (dppp[esi] > 19) {
            upper2 = 1;
            // ??? check others copied from here! todo change upper to upper2 if not upper 2 like in YUY2 part
            break;
          }
          esi++;
        } while (esi < edx);
      }

      { // blocked for local vars
        int esi = eax;
        do {
          if (dpp[esi] > 19)
            upper = 1;
          if (dpn[esi] > 19)
            lower = 1;
          if (upper != 0 && lower != 0)
            break;
          esi++;
        } while (esi < edx);
      }

      if (y != Height - 4) {

        int esi = eax;
        do {
          if (dpnn[esi] > 19) {
            lower2 = 1;
            break;
          }
          esi++;
        } while (esi < edx);
      }

      if (upper == 0) {
        if (lower == 0 || lower2 == 0) {
          if (count > 4)
            dstp[x] += 4;
        }
        else {
          dstp[x] += 2;
        }
      }
      else {
        if (lower != 0 || upper2 != 0) {
          dstp[x] += 2;
        }
        else {
          if (count > 4)
            dstp[x] += 4;
        }
      }
    }
    dppp += tpitch;
    dpp += tpitch;
    dp += tpitch;
    dpn += tpitch;
    dpnn += tpitch;
    dstp += dst_pitch;
  }
}

// TDeint and TFM version
// fixme: compare with AnalyzeDiffMask_Planar + Compare chroma and non-chroma parts
void AnalyzeDiffMask_YUY2(unsigned char* dstp, int dst_pitch, unsigned char* tbuffer, int tpitch, int Width, int Height, bool mChroma)
{
  const unsigned char* dppp = tbuffer - tpitch;
  const unsigned char* dpp = tbuffer;
  const unsigned char* dp = tbuffer + tpitch;
  const unsigned char* dpn = tbuffer + tpitch * 2;
  const unsigned char* dpnn = tbuffer + tpitch * 3;
  int count;
  bool upper, lower, upper2, lower2;

  // fixme: make if common with TFM
  // reconstructed from inline 700+ lines asm by pinterf

  if (mChroma) // TFM YUY2's mChroma bool parameter
  {
    for (int y = 2; y < Height - 2; y += 2) {
      for (int x = 4; x < Width - 4; x += 1) // the first ebx++ is in the code. later in TFMYUY2 1051: ebx += 2
      {
        int eax, esi, edi;

        if (dp[x] < 3) {
          goto chroma_sec;
        }

        if (dp[x - 2] < 3 && dp[x + 2] < 3 &&
          dpp[x - 2] < 3 && dpp[x] < 3 && dpp[x + 2] < 3 &&
          dpn[x - 2] < 3 && dpn[x] < 3 && dpn[x + 2] < 3)
        {
          goto chroma_sec;
        }

        dstp[x]++;
        if (dp[x] <= 19) {
          goto chroma_sec;
        }

        edi = 0;
        lower = 0;
        upper = 0;

        if (dpp[x - 2] > 19) edi++;
        if (dpp[x] > 19) edi++;
        if (dpp[x + 2] > 19) edi++;

        if (edi != 0) upper = 1;

        if (dp[x - 2] > 19) edi++;
        if (dp[x + 2] > 19) edi++;

        esi = edi;

        if (dpn[x - 2] > 19) edi++;
        if (dpn[x] > 19) edi++;
        if (dpn[x + 2] > 19) edi++;

        if (edi <= 2) {
          goto chroma_sec;
        }

        count = edi;
        if (count != esi) {
          lower = 1;
          if (upper != 0) {
            dstp[x] += 2;
            goto chroma_sec;
          }
        }

        eax = x - 2 * 4;
        if (eax < 0) eax = 0;

        lower2 = 0;
        upper2 = 0;
        { // blocked for local vars
          int edx = x + 2 * 5;
          if (edx > Width) edx = Width;
    //-----------
          if (y != 2) {
            int esi = eax;
            do {
              if (dppp[esi] > 19) {
                upper2 = 1;
                break;
              }
              esi += 2; // YUY2 inc
            } while (esi < edx);
          }

          int esi = eax;
          do {
            if (dpp[esi] > 19)
              upper = 1;
            if (dpn[esi] > 19)
              lower = 1;
            if (upper != 0 && lower != 0)
              break;
            esi += 2; // YUY2 inc
          } while (esi < edx);
          //---------
          if (y != Height - 4) {
            int esi = eax;
            do {
              if (dpnn[esi] > 19) {
                lower2 = 1;
                break;
              }
              esi += 2; // YUY2 inc
            } while (esi < edx);
          }
        }

        if (upper == 0) {
          if (lower == 0 || lower2 == 0) {
            if (count > 4)
              dstp[x] += 4;
          }
          else {
            dstp[x] += 2;
          }
        }
        else {
          if (lower != 0 || upper2 != 0) {
            dstp[x] += 2;
          }
          else {
            if (count > 4)
              dstp[x] += 4;
          }
        }
        //-------------- the other one ends here, with a x+=2

      chroma_sec:
        // skip to chroma
        x++;

        if (dp[x] < 3) {
          continue;
        }
        if (dp[x - 4] < 3 && dp[x + 4] < 3 &&
          dpp[x - 4] < 3 && dpp[x] < 3 && dpp[x + 4] < 3 &&
          dpn[x - 4] < 3 && dpn[x] < 3 && dpn[x + 4] < 3)
        {
          continue;
        }
        dstp[x]++;
        if (dp[x] <= 19) {
          continue;
        }
        edi = 0;
        lower = 0;
        upper = 0;
        if (dpp[x - 4] > 19) edi++;
        if (dpp[x] > 19) edi++;
        if (dpp[x + 4] > 19) edi++;
        if (edi != 0) upper = 1;
        if (dp[x - 4] > 19) edi++;
        if (dp[x + 4] > 19) edi++;
        esi = edi;
        if (dpn[x - 4] > 19) edi++;
        if (dpn[x] > 19) edi++;
        if (dpn[x + 4] > 19) edi++;
        if (edi <= 2) {
          continue;
        }
        count = edi;
        if (esi != edi) {
          lower = 1;
          if (upper != 0) {
            dstp[x] += 2;
            continue;
          }
        }

        eax = x - 8 - 8; // was: -8 in the first chroma part
        int edx_tmp = (x & 2) + 1; // diff from 1st part, comparison is with 0 there
        if (eax < edx_tmp) eax = edx_tmp; // diff from 1st part

        lower2 = 0;
        upper2 = 0;
        int edx = x + 10 + 8; // was: +10 in the first chroma part
        if (edx > Width) edx = Width;

        //-----------
        if (y != 2) {
          int esi = eax;
          do {
            if (dppp[esi] > 19) {
              upper2 = 1;
              break;
            }
            esi += 4; // was: += 2 in the first chroma part
          } while (esi < edx);
        }

        { // blocked for local vars
          int esi = eax;
          do {
            if (dpp[esi] > 19)
              upper = 1;
            if (dpn[esi] > 19)
              lower = 1;
            if (upper != 0 && lower != 0)
              break;
            esi += 4;  // was: += 2 in the first chroma part
          } while (esi < edx);
        }
        //---------
        if (y != Height - 4) {

          int esi = eax;
          do {
            if (dpnn[esi] > 19) {
              lower2 = 1;
              break;
            }
            esi += 4;  // was: += 2 in the first chroma part
          } while (esi < edx);
        }

        if (upper == 0) {
          if (lower == 0 || lower2 == 0) {
            if (count > 4)
              dstp[x] += 4;
          }
          else {
            dstp[x] += 2;
          }
        }
        else {
          if (lower != 0 || upper2 != 0) {
            dstp[x] += 2;
          }
          else {
            if (count > 4)
              dstp[x] += 4;
          }
        }

      }
      dppp += tpitch;
      dpp += tpitch;
      dp += tpitch;
      dpn += tpitch;
      dpnn += tpitch;
      dstp += dst_pitch;
    }
  }
  else {
    // no YUY2 chroma, LumaOnly
    for (int y = 2; y < Height - 2; y += 2) {
      for (int x = 4; x < Width - 4; x += 2)
      {
        if (dp[x] < 3) {
          continue; // goto chroma_sec;
        }
        if (dp[x - 2] < 3 && dp[x + 2] < 3 &&
          dpp[x - 2] < 3 && dpp[x] < 3 && dpp[x + 2] < 3 &&
          dpn[x - 2] < 3 && dpn[x] < 3 && dpn[x + 2] < 3)
        {
          continue; // goto chroma_sec;
        }
        dstp[x]++;
        if (dp[x] <= 19) {
          continue; //  goto chroma_sec;
        }

        int edi = 0;
        lower = 0;
        upper = 0;
        if (dpp[x - 2] > 19) edi++;
        if (dpp[x] > 19) edi++;
        if (dpp[x + 2] > 19) edi++;
        if (edi != 0) upper = 1;
        if (dp[x - 2] > 19) edi++;
        if (dp[x + 2] > 19) edi++;
        int esi = edi;
        if (dpn[x - 2] > 19) edi++;
        if (dpn[x] > 19) edi++;
        if (dpn[x + 2] > 19) edi++;
        if (edi <= 2) {
          continue; //  goto chroma_sec;
        }
        count = edi;
        if (esi != edi) {
          lower = 1;
          if (upper != 0) {
            dstp[x] += 2;
            continue; // goto chroma_sec;
          }
        }
        int eax = x - 2 * 4;
        if (eax < 0) eax = 0;

        lower2 = 0;
        upper2 = 0;
        int edx = x + 2 * 5;
        if (edx > Width) edx = Width;
        //-----------
        if (y != 2) {
          int esi = eax;
          do {
            if (dppp[esi] > 19) {
              upper2 = 1;
              break;
            }
            esi += 2;
          } while (esi < edx);
        }
        { // blocked for local vars
          int esi = eax;
          do {
            if (dpp[esi] > 19)
              upper = 1;
            if (dpn[esi] > 19)
              lower = 1;
            if (upper != 0 && lower != 0)
              break;
            esi += 2;
          } while (esi < edx);
        }
        //---------
        if (y != Height - 4) {

          int esi = eax;
          do {
            if (dpnn[esi] > 19) {
              lower2 = 1;
              break;
            }
            esi += 2;
          } while (esi < edx);
        }
        
        if (upper == 0) {
          if (lower == 0 || lower2 == 0) {
            if (count > 4)
              dstp[x] += 4;
          }
          else {
            dstp[x] += 2;
          }
        }
        else {
          if (lower != 0 || upper2 != 0) {
            dstp[x] += 2;
          }
          else {
            if (count > 4)
              dstp[x] += 4;
          }
        }
      }
      dppp += tpitch;
      dpp += tpitch;
      dp += tpitch;
      dpn += tpitch;
      dpnn += tpitch;
      dstp += dst_pitch;
    }
  }
}

template<bool YUY2_LumaOnly>
void buildABSDiffMask2_c(const unsigned char* prvp, const unsigned char* nxtp,
  unsigned char* dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width, int height)
{
  if (width <= 0)
    return;

  constexpr int inc = YUY2_LumaOnly ? 2 : 1;
  for (int y = 0; y < height; ++y)
  {
    for (int x = 0; x < width; x += inc)
    {
      const int diff = abs(prvp[x] - nxtp[x]);
      if (diff > 19) dstp[x] = 3;
      else if (diff > 3) dstp[x] = 1;
      else dstp[x] = 0;
    }
    prvp += prv_pitch;
    nxtp += nxt_pitch;
    dstp += dst_pitch;
  }
}



void buildABSDiffMask2_SSE2(const unsigned char* prvp, const unsigned char* nxtp,
  unsigned char* dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width,
  int height)
{
  __m128i onesMask = _mm_set1_epi8(0x01);
  __m128i twosMask = _mm_set1_epi8(0x02);
  __m128i all_ff = _mm_set1_epi8(-1);
  __m128i mask251 = _mm_set1_epi8((char)0xFB); // 1111 1011
  __m128i mask235 = _mm_set1_epi8((char)0xEB); // 1110 1011

  if (!(width & 15)) // exact mod16
  {
    while (height--) {
      for (int x = 0; x < width; x += 16)
      {
        __m128i src_prev = _mm_load_si128(reinterpret_cast<const __m128i*>(prvp + x));
        __m128i src_next = _mm_load_si128(reinterpret_cast<const __m128i*>(nxtp + x));
        __m128i diffpn = _mm_subs_epu8(src_prev, src_next);
        __m128i diffnp = _mm_subs_epu8(src_next, src_prev);
        __m128i diff = _mm_or_si128(diffpn, diffnp);
        /*
        const int diff = abs(prvp[x] - nxtp[x]);
        if (diff > 19) dstp[x] = 3;
        else if (diff > 3) dstp[x] = 1;
        else dstp[x] = 0;
        */
        __m128i added251 = _mm_adds_epu8(diff, mask251);
        __m128i added235 = _mm_adds_epu8(diff, mask235);
        __m128i cmp251 = _mm_cmpeq_epi8(added251, all_ff);
        __m128i cmp235 = _mm_cmpeq_epi8(added235, all_ff);
        __m128i tmp1 = _mm_and_si128(cmp251, onesMask);
        __m128i tmp2 = _mm_and_si128(cmp235, twosMask);
        __m128i tmp = _mm_or_si128(tmp1, tmp2);
        _mm_store_si128(reinterpret_cast<__m128i*>(dstp + x), tmp);
      }
      prvp += prv_pitch;
      nxtp += nxt_pitch;
      dstp += dst_pitch;
    }
  }
  else {
    width -= 8; // last chunk is 8 bytes instead of 16
    while (height--) {
      int x;
      for (x = 0; x < width; x += 16)
      {
        __m128i src_prev = _mm_load_si128(reinterpret_cast<const __m128i*>(prvp + x));
        __m128i src_next = _mm_load_si128(reinterpret_cast<const __m128i*>(nxtp + x));
        __m128i diffpn = _mm_subs_epu8(src_prev, src_next);
        __m128i diffnp = _mm_subs_epu8(src_next, src_prev);
        __m128i diff = _mm_or_si128(diffpn, diffnp);
        __m128i added251 = _mm_adds_epu8(diff, mask251);
        __m128i added235 = _mm_adds_epu8(diff, mask235);
        __m128i cmp251 = _mm_cmpeq_epi8(added251, all_ff);
        __m128i cmp235 = _mm_cmpeq_epi8(added235, all_ff);
        __m128i tmp1 = _mm_and_si128(cmp251, onesMask);
        __m128i tmp2 = _mm_and_si128(cmp235, twosMask);
        __m128i tmp = _mm_or_si128(tmp1, tmp2);
        _mm_store_si128(reinterpret_cast<__m128i*>(dstp + x), tmp);
      }
      __m128i src_prev = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(prvp + x));
      __m128i src_next = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(nxtp + x));
      __m128i diffpn = _mm_subs_epu8(src_prev, src_next);
      __m128i diffnp = _mm_subs_epu8(src_next, src_prev);
      __m128i diff = _mm_or_si128(diffpn, diffnp);
      __m128i added251 = _mm_adds_epu8(diff, mask251);
      __m128i added235 = _mm_adds_epu8(diff, mask235);
      __m128i cmp251 = _mm_cmpeq_epi8(added251, all_ff);
      __m128i cmp235 = _mm_cmpeq_epi8(added235, all_ff);
      __m128i tmp1 = _mm_and_si128(cmp251, onesMask);
      __m128i tmp2 = _mm_and_si128(cmp235, twosMask);
      __m128i tmp = _mm_or_si128(tmp1, tmp2);
      _mm_storel_epi64(reinterpret_cast<__m128i*>(dstp + x), tmp);
      prvp += prv_pitch;
      nxtp += nxt_pitch;
      dstp += dst_pitch;
    }
  }
}

// fixme: also in tfmasm
template<bool with_luma_mask>
static void check_combing_SSE2_generic_simd(const unsigned char *srcp, unsigned char *dstp, int width,
  int height, int src_pitch, int src_pitch2, int dst_pitch, __m128i threshb, __m128i thresh6w)
{
  __m128i all_ff = _mm_set1_epi8(-1);
  while (height--) {
    for (int x = 0; x < width; x += 16) {
      auto next = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + src_pitch + x));
      auto curr = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + x));
      auto prev = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp - src_pitch + x));
      auto diff_curr_next = _mm_subs_epu8(curr, next);
      auto diff_next_curr = _mm_subs_epu8(next, curr);
      auto diff_curr_prev = _mm_subs_epu8(curr, prev);
      auto diff_prev_curr = _mm_subs_epu8(prev, curr);
      // max(min(p-s,n-s), min(s-n,s-p))
      auto xmm2_max = _mm_max_epu8(_mm_min_epu8(diff_prev_curr, diff_next_curr), _mm_min_epu8(diff_curr_next, diff_curr_prev));
      auto xmm2_cmp = _mm_cmpeq_epi8(_mm_adds_epu8(xmm2_max, threshb), all_ff);
      if (with_luma_mask) { // YUY2 luma mask
        __m128i lumaMask = _mm_set1_epi16(0x00FF);
        xmm2_cmp = _mm_and_si128(xmm2_cmp, lumaMask);
      }
      auto res_part1 = xmm2_cmp;
      bool cmpres_is_allzero;
#ifdef _M_X64
      cmpres_is_allzero = (_mm_cvtsi128_si64(xmm2_cmp) | _mm_cvtsi128_si64(_mm_srli_si128(xmm2_cmp, 8))) == 0; // _si64: only at x64 platform
#else
      cmpres_is_allzero = (_mm_cvtsi128_si32(xmm2_cmp) |
        _mm_cvtsi128_si32(_mm_srli_si128(xmm2_cmp, 4)) |
        _mm_cvtsi128_si32(_mm_srli_si128(xmm2_cmp, 8)) |
        _mm_cvtsi128_si32(_mm_srli_si128(xmm2_cmp, 12))
        ) == 0;
#endif
        if (!cmpres_is_allzero) {
          // output2
          auto zero = _mm_setzero_si128();
          // compute 3*(p+n)
          auto next_lo = _mm_unpacklo_epi8(next, zero);
          auto prev_lo = _mm_unpacklo_epi8(prev, zero);
          auto next_hi = _mm_unpackhi_epi8(next, zero);
          auto prev_hi = _mm_unpackhi_epi8(prev, zero);
          __m128i threeMask = _mm_set1_epi16(3);
          auto mul_lo = _mm_mullo_epi16(_mm_adds_epu16(next_lo, prev_lo), threeMask);
          auto mul_hi = _mm_mullo_epi16(_mm_adds_epu16(next_hi, prev_hi), threeMask);

          // compute (pp+c*4+nn)
          auto prevprev = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp - src_pitch * 2 + x));
          auto prevprev_lo = _mm_unpacklo_epi8(prevprev, zero);
          auto prevprev_hi = _mm_unpackhi_epi8(prevprev, zero);
          auto curr_lo = _mm_unpacklo_epi8(curr, zero);
          auto curr_hi = _mm_unpackhi_epi8(curr, zero);
          auto sum2_lo = _mm_adds_epu16(_mm_slli_epi16(curr_lo, 2), prevprev_lo); // pp + c*4
          auto sum2_hi = _mm_adds_epu16(_mm_slli_epi16(curr_hi, 2), prevprev_hi); // pp + c*4

          auto nextnext = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + src_pitch * 2 + x));
          auto nextnext_lo = _mm_unpacklo_epi8(nextnext, zero);
          auto nextnext_hi = _mm_unpackhi_epi8(nextnext, zero);
          auto sum3_lo = _mm_adds_epu16(sum2_lo, nextnext_lo);
          auto sum3_hi = _mm_adds_epu16(sum2_hi, nextnext_hi);

          // working with sum3=(pp+c*4+nn)   and  mul=3*(p+n)
          auto diff_sum3lo_mullo = _mm_subs_epu16(sum3_lo, mul_lo);
          auto diff_mullo_sum3lo = _mm_subs_epu16(mul_lo, sum3_lo);
          auto diff_sum3hi_mulhi = _mm_subs_epu16(sum3_hi, mul_hi);
          auto diff_mulhi_sum3hi = _mm_subs_epu16(mul_hi, sum3_hi);
          // abs( (pp+c*4+nn) - mul=3*(p+n) )
          auto max_lo = _mm_max_epi16(diff_sum3lo_mullo, diff_mullo_sum3lo);
          auto max_hi = _mm_max_epi16(diff_sum3hi_mulhi, diff_mulhi_sum3hi);
          // abs( (pp+c*4+nn) - mul=3*(p+n) ) + thresh6w
          auto lo_thresh6w_added = _mm_adds_epu16(max_lo, thresh6w);
          auto hi_thresh6w_added = _mm_adds_epu16(max_hi, thresh6w);
          // maximum reached?
          auto cmp_lo = _mm_cmpeq_epi16(lo_thresh6w_added, all_ff);
          auto cmp_hi = _mm_cmpeq_epi16(hi_thresh6w_added, all_ff);

          auto res_lo = _mm_srli_epi16(cmp_lo, 8);
          auto res_hi = _mm_srli_epi16(cmp_hi, 8);
          auto res_part2 = _mm_packus_epi16(res_lo, res_hi);

          auto res = _mm_and_si128(res_part1, res_part2);
          _mm_store_si128(reinterpret_cast<__m128i *>(dstp + x), res);
        }
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}


// src_pitch2: src_pitch*2 for inline asm speed reasons
void check_combing_SSE2(const unsigned char *srcp, unsigned char *dstp, int width,
  int height, int src_pitch, int src_pitch2, int dst_pitch, __m128i threshb, __m128i thresh6w)
{
  // no luma masking
  check_combing_SSE2_generic_simd<false>(srcp, dstp, width, height, src_pitch, src_pitch2, dst_pitch, threshb, thresh6w);
}

void check_combing_SSE2_Luma(const unsigned char *srcp, unsigned char *dstp, int width,
  int height, int src_pitch, int src_pitch2, int dst_pitch, __m128i threshb, __m128i thresh6w)
{
  // with luma masking
  check_combing_SSE2_generic_simd<true>(srcp, dstp, width, height, src_pitch, src_pitch2, dst_pitch, threshb, thresh6w);
}

void check_combing_SSE2_M1(const unsigned char *srcp, unsigned char *dstp,
  int width, int height, int src_pitch, int dst_pitch, __m128i thresh)
{
  __m128i zero = _mm_setzero_si128();
  __m128i lumaMask = _mm_set1_epi16(0x00FF);

  while (height--) {
    for (int x = 0; x < width; x += 16) {
      auto next = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + src_pitch + x));
      auto curr = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + x));
      auto prev = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp - src_pitch + x));

      auto prev_lo = _mm_unpacklo_epi8(prev, zero);
      auto prev_hi = _mm_unpackhi_epi8(prev, zero);
      auto curr_lo = _mm_unpacklo_epi8(curr, zero);
      auto curr_hi = _mm_unpackhi_epi8(curr, zero);
      auto next_lo = _mm_unpacklo_epi8(next, zero);
      auto next_hi = _mm_unpackhi_epi8(next, zero);

      auto diff_prev_curr_lo = _mm_subs_epi16(prev_lo, curr_lo);
      auto diff_next_curr_lo = _mm_subs_epi16(next_lo, curr_lo);
      auto diff_prev_curr_hi = _mm_subs_epi16(prev_hi, curr_hi);
      auto diff_next_curr_hi = _mm_subs_epi16(next_hi, curr_hi);

      // -- lo
      auto diff_prev_curr_lo_lo = _mm_unpacklo_epi16(diff_prev_curr_lo, zero);
      auto diff_prev_curr_lo_hi = _mm_unpackhi_epi16(diff_prev_curr_lo, zero);
      auto diff_next_curr_lo_lo = _mm_unpacklo_epi16(diff_next_curr_lo, zero);
      auto diff_next_curr_lo_hi = _mm_unpackhi_epi16(diff_next_curr_lo, zero);

      auto res_lo_lo = _mm_madd_epi16(diff_prev_curr_lo_lo, diff_next_curr_lo_lo);
      auto res_lo_hi = _mm_madd_epi16(diff_prev_curr_lo_hi, diff_next_curr_lo_hi);

      // -- hi
      auto diff_prev_curr_hi_lo = _mm_unpacklo_epi16(diff_prev_curr_hi, zero);
      auto diff_prev_curr_hi_hi = _mm_unpackhi_epi16(diff_prev_curr_hi, zero);
      auto diff_next_curr_hi_lo = _mm_unpacklo_epi16(diff_next_curr_hi, zero);
      auto diff_next_curr_hi_hi = _mm_unpackhi_epi16(diff_next_curr_hi, zero);

      auto res_hi_lo = _mm_madd_epi16(diff_prev_curr_hi_lo, diff_next_curr_hi_lo);
      auto res_hi_hi = _mm_madd_epi16(diff_prev_curr_hi_hi, diff_next_curr_hi_hi);

      auto cmp_lo_lo = _mm_cmpgt_epi32(res_lo_lo, thresh);
      auto cmp_lo_hi = _mm_cmpgt_epi32(res_lo_hi, thresh);
      auto cmp_hi_lo = _mm_cmpgt_epi32(res_hi_lo, thresh);
      auto cmp_hi_hi = _mm_cmpgt_epi32(res_hi_hi, thresh);

      auto cmp_lo = _mm_packs_epi32(cmp_lo_lo, cmp_lo_hi);
      auto cmp_hi = _mm_packs_epi32(cmp_hi_lo, cmp_hi_hi);
      auto cmp_lo_masked = _mm_and_si128(cmp_lo, lumaMask);
      auto cmp_hi_masked = _mm_and_si128(cmp_hi, lumaMask);

      auto res = _mm_packus_epi16(cmp_lo_masked, cmp_hi_masked);
      _mm_store_si128(reinterpret_cast<__m128i *>(dstp + x), res);
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }

}


void check_combing_SSE2_Luma_M1(const unsigned char *srcp, unsigned char *dstp,
  int width, int height, int src_pitch, int dst_pitch, __m128i thresh)
{
  __m128i lumaMask = _mm_set1_epi16(0x00FF);
  __m128i zero = _mm_setzero_si128();
  while (height--) {
    for (int x = 0; x < width; x += 16) {
      auto next = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + src_pitch + x));
      auto curr = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + x));
      auto prev = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp - src_pitch + x));
      
      next = _mm_and_si128(next, lumaMask);
      curr = _mm_and_si128(curr, lumaMask);
      prev = _mm_and_si128(prev, lumaMask);

      auto diff_prev_curr = _mm_subs_epi16(prev, curr);
      auto diff_next_curr = _mm_subs_epi16(next, curr);

      auto diff_prev_curr_lo = _mm_unpacklo_epi16(diff_prev_curr, zero);
      auto diff_prev_curr_hi = _mm_unpackhi_epi16(diff_prev_curr, zero);
      auto diff_next_curr_lo = _mm_unpacklo_epi16(diff_next_curr, zero);
      auto diff_next_curr_hi = _mm_unpackhi_epi16(diff_next_curr, zero);

      auto res_lo = _mm_madd_epi16(diff_prev_curr_lo, diff_next_curr_lo);
      auto res_hi = _mm_madd_epi16(diff_prev_curr_hi, diff_next_curr_hi);

      auto cmp_lo = _mm_cmpgt_epi32(res_lo, thresh);
      auto cmp_hi = _mm_cmpgt_epi32(res_hi, thresh);

      auto cmp = _mm_packs_epi32(cmp_lo, cmp_hi);
      auto cmp_masked = _mm_and_si128(cmp, lumaMask);

      _mm_store_si128(reinterpret_cast<__m128i *>(dstp + x), cmp_masked);
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

template<int blockSizeY>
void compute_sum_8xN_sse2(const unsigned char *srcp, int pitch, int &sum)
{
  // sums masks
  // if (cmkppT[x + v] == 0xFF && cmkpT[x + v] == 0xFF && cmkpnT[x + v] == 0xFF) sum++;
  // scrp is prev
  auto onesMask = _mm_set1_epi8(1);
  auto all_ff = _mm_set1_epi8(-1);
  auto prev = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(srcp));
  auto curr = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(srcp + pitch));
  auto summa = _mm_setzero_si128();
  srcp += pitch * 2; // points to next
  // unroll 2
  for (int i = 0; i < blockSizeY / 2; i++) { // 4x2=8
    /*
    p  #
    c  # #
    n  # #
    nn   #
    */
    auto next = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(srcp));
    auto nextnext = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(srcp + pitch));

    auto anded_common = _mm_and_si128(curr, next);
    auto with_prev = _mm_and_si128(prev, anded_common);
    auto with_nextnext = _mm_and_si128(anded_common, nextnext);

    // these were missing from the original assembler code (== 0xFF)
    with_prev = _mm_cmpeq_epi8(with_prev, all_ff);
    with_nextnext = _mm_cmpeq_epi8(with_nextnext, all_ff);

    with_prev = _mm_and_si128(with_prev, onesMask);
    with_nextnext = _mm_and_si128(with_nextnext, onesMask);

    prev = next;
    curr = nextnext;

    summa = _mm_adds_epu8(summa, with_prev);
    summa = _mm_adds_epu8(summa, with_nextnext);
    srcp += pitch * 2;
  }
  // now we have to sum up lower 8 bytes
  // in sse2, we use sad
  auto zero = _mm_setzero_si128();
  auto tmpsum = _mm_sad_epu8(summa, zero);  // sum(lo 8 bytes)(needed) / sum(hi 8 bytes)(not needed)
  sum = _mm_cvtsi128_si32(tmpsum);
}

// instantiate for 8x8
template void compute_sum_8xN_sse2<8>(const unsigned char* srcp, int pitch, int& sum);

// YUY2 luma only case
void compute_sum_16x8_sse2_luma(const unsigned char *srcp, int pitch, int &sum)
{
  // sums masks
  // if (cmkppT[x + v] == 0xFF && cmkpT[x + v] == 0xFF && cmkpnT[x + v] == 0xFF) sum++;
  // scrp is prev
  auto onesMask = _mm_set1_epi16(0x0001); // ones where luma
  auto all_ff = _mm_set1_epi8(-1);
  auto prev = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp));
  auto curr = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + pitch));
  auto summa = _mm_setzero_si128();
  srcp += pitch * 2; // points to next
  for (int i = 0; i < 4; i++) { // 4x2=8
    /*
    p  #
    c  # #
    n  # #
    nn   #
    */
    auto next = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp));
    auto nextnext = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + pitch));

    auto anded_common = _mm_and_si128(curr, next);
    auto with_prev = _mm_and_si128(prev, anded_common);
    auto with_nextnext = _mm_and_si128(anded_common, nextnext);

    // these were missing from the original assembler code (== 0xFF)
    with_prev = _mm_cmpeq_epi8(with_prev, all_ff);
    with_nextnext = _mm_cmpeq_epi8(with_nextnext, all_ff);

    with_prev = _mm_and_si128(with_prev, onesMask);
    with_nextnext = _mm_and_si128(with_nextnext, onesMask);

    prev = next;
    curr = nextnext;

    summa = _mm_adds_epu8(summa, with_prev);
    summa = _mm_adds_epu8(summa, with_nextnext);
    srcp += pitch * 2;
  }

  // now we have to sum up lower and upper 8 bytes
  // in sse2, we use sad
  auto zero = _mm_setzero_si128();
  auto tmpsum = _mm_sad_epu8(summa, zero);  // sum(lo 8 bytes) / sum(hi 8 bytes)
  tmpsum = _mm_add_epi32(tmpsum, _mm_srli_si128(tmpsum, 8)); // lo + hi
  sum = _mm_cvtsi128_si32(tmpsum);
}


