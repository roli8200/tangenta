// SPDX-License-Identifier: MPL-2.0
// TANGENTA: CMYK-Umrechnung mit 16-Bit-Eingabe (Little CMS rechnet dann über eine vorberechnete
// Tabelle statt jedes Pixel einzeln) und Zwischenspeicher (Schlüssel: je Kanal 0..255)
#include <cstdint>
#include <vector>

namespace
{

void cachedCmykToRgb(cmsHTRANSFORM transform, const double cmyk[4], unsigned char rgb[3])
{
  // direkt adressiert: Fotos haben viele Farben, Nachbarpixel meist dieselben
  struct Slot
  {
    std::uint32_t key;
    std::uint32_t value;
  };
  static thread_local cmsHTRANSFORM owner = nullptr;
  static thread_local std::vector<Slot> cache;
  if (owner != transform || cache.empty())
  {
    cache.assign(1u << 16, Slot{ 0xffffffffu, 0 });
    owner = transform;
  }
  std::uint32_t key = 0;
  cmsUInt16Number in[4];
  for (int i = 0; i < 4; ++i)
  {
    const double v = cmyk[i] < 0 ? 0 : cmyk[i] > 100 ? 100 : cmyk[i];
    const auto byte = (std::uint32_t)(v * 2.55 + 0.5);
    key = (key << 8) | byte;
    in[i] = (cmsUInt16Number)(byte * 257);
  }
  Slot &slot = cache[(key * 2654435761u) >> 16];
  if (slot.key != key)
  {
    cmsDoTransform(transform, in, rgb, 1);
    slot.key = key;
    slot.value = ((std::uint32_t)rgb[0] << 16) | ((std::uint32_t)rgb[1] << 8) | rgb[2];
  }
  rgb[0] = (unsigned char)(slot.value >> 16);
  rgb[1] = (unsigned char)(slot.value >> 8);
  rgb[2] = (unsigned char)slot.value;
}

} // namespace
