  // TANGENTA: Maske als Alphakanal (viertes Byte jedes Pixels; ohne Maske bleibt es 0)
  {
    TangentaBitmapMask &mask = tangentaBitmapMask();
    if (storeBMP && mask.width == width && mask.height == height && !mask.data.empty())
    {
      std::vector<unsigned char> bytes(image.getDataBuffer(), image.getDataBuffer() + image.size());
      const unsigned long maskLine = mask.data.size() / height;
      for (unsigned j = 0; j < height; ++j)
        for (unsigned k = 0; k < width; ++k)
        {
          const unsigned long pixel = tmpDIBOffsetBits + ((unsigned long)j * width + k) * 4 + 3;
          if (pixel < bytes.size())
            bytes[pixel] = mask.data[j * maskLine + k] ? 0xff : 0x00;
        }
      image = librevenge::RVNGBinaryData(bytes.data(), bytes.size());
    }
    mask = TangentaBitmapMask();
  }
