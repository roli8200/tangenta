  // TANGENTA: Maske hinter dem Bild lesen ("RI", 12 Bytes, dann Kopf wie beim Bild, ohne Palette)
  {
    TangentaBitmapMask &mask = tangentaBitmapMask();
    mask = TangentaBitmapMask();
    if (getRemainingLength(input) > 2 + 12 + 64 && readU8(input) == 'R' && readU8(input) == 'I')
    {
      input->seek(12, librevenge::RVNG_SEEK_CUR);
      readU32(input); // Farbmodell der Maske
      input->seek(4, librevenge::RVNG_SEEK_CUR);
      const unsigned maskWidth = readU32(input);
      const unsigned maskHeight = readU32(input);
      input->seek(4, librevenge::RVNG_SEEK_CUR);
      const unsigned maskBpp = readU32(input);
      input->seek(4, librevenge::RVNG_SEEK_CUR);
      const unsigned maskSize = readU32(input);
      input->seek(32, librevenge::RVNG_SEEK_CUR);
      if (maskWidth == width && maskHeight == height && maskBpp == 8 && maskHeight
          && maskSize >= (unsigned long)maskWidth * maskHeight && maskSize <= getRemainingLength(input))
      {
        unsigned long numBytesRead = 0;
        const unsigned char *buffer = input->read(maskSize, numBytesRead);
        if (buffer && numBytesRead == maskSize)
        {
          mask.width = maskWidth;
          mask.height = maskHeight;
          mask.data.assign(buffer, buffer + maskSize);
        }
      }
    }
  }
