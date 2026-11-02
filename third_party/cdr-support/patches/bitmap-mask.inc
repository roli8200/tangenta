// SPDX-License-Identifier: MPL-2.0
// TANGENTA: Hinter den Bilddaten eines Bitmaps steht oft eine Maske gleicher Grösse ("RI", 8 Bit,
// 0 = durchsichtig). readBmp legt sie hier ab, collectBmp schreibt sie als Alphakanal ins BMP.
#include <vector>

namespace libcdr
{

struct TangentaBitmapMask
{
  unsigned width = 0;
  unsigned height = 0;
  std::vector<unsigned char> data;
};

inline TangentaBitmapMask &tangentaBitmapMask()
{
  static thread_local TangentaBitmapMask mask;
  return mask;
}

} // namespace libcdr
