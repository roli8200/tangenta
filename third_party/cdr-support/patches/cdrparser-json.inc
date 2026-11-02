  // SPDX-License-Identifier: MPL-2.0
  // TANGENTA: Textstile neuerer Dateien sind JSON; gelesen mit QJsonDocument statt boost::property_tree
  const QJsonDocument doc = QJsonDocument::fromJson(QByteArray(styleString.cstr()));
  if (!doc.isObject())
    return;
  const QJsonObject root = doc.object();
  auto value = [&root](const char *path)
  {
    QJsonValue v = root;
    for (const QString &part : QString::fromLatin1(path).split(QLatin1Char('.')))
    {
      if (!v.isObject())
        return QJsonValue(QJsonValue::Undefined);
      v = v.toObject().value(part);
    }
    return v;
  };
  auto text = [&value](const char *path, std::string &out)
  {
    const QJsonValue v = value(path);
    if (v.isString())
      out = v.toString().toStdString();
    else if (v.isDouble())
      out = QString::number(v.toDouble()).toStdString();
    else
      return false;
    return true;
  };
  auto number = [&value](const char *path, unsigned &out)
  {
    const QJsonValue v = value(path);
    if (v.isDouble())
    {
      out = (unsigned)v.toDouble();
      return true;
    }
    bool ok = false;
    if (v.isString())
      out = v.toString().toUInt(&ok);
    return ok;
  };
  auto exists = [&value](const char *path)
  {
    return !value(path).isUndefined();
  };

  if (root.contains(QStringLiteral("character")))
  {
    std::string fontName;
    if (text("character.latin.font", fontName))
      style.m_fontName = fontName.c_str();
    unsigned charset = 0;
    number("character.latin.charset", charset);
    const auto encoding = (unsigned short)charset;
    if (encoding || style.m_charSet == (unsigned short)-1)
      style.m_charSet = encoding;
    processNameForEncoding(style.m_fontName, style.m_charSet);
    unsigned fontSize = 0;
    if (number("character.latin.size", fontSize))
      style.m_fontSize = (double)fontSize / 254000.0;

    if (exists("character.outline"))
    {
      style.m_lineStyle.lineType = 0;
      unsigned lineWidth = 0;
      if (number("character.outline.width", lineWidth))
        style.m_lineStyle.lineWidth = (double)lineWidth / 254000.0;
      std::string color;
      if (text("character.outline.color", color))
      {
        double opacity = 1.0;
        parseColourString(color.c_str(), style.m_lineStyle.color, opacity);
        _resolveColorPalette(style.m_lineStyle.color);
      }
    }

    if (exists("character.fill"))
    {
      unsigned type = 0;
      if (number("character.fill.type", type))
        style.m_fillStyle.fillType = (unsigned short)type;
      std::string color;
      if (text("character.fill.primaryColor", color))
      {
        double opacity = 1.0;
        parseColourString(color.c_str(), style.m_fillStyle.color1, opacity);
        _resolveColorPalette(style.m_fillStyle.color1);
        parseColourString(color.c_str(), style.m_fillStyle.color2, opacity);
        _resolveColorPalette(style.m_fillStyle.color2);
      }
    }
  }

  if (root.contains(QStringLiteral("paragraph")))
  {
    unsigned align = 0;
    if (number("paragraph.justify", align))
      style.m_align = align;
  }
}

