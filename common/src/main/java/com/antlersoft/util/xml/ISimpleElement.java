package com.antlersoft.util.xml;

import org.xml.sax.Attributes;
import org.xml.sax.SAXException;

public abstract interface ISimpleElement
{
  public abstract void gotElement(String paramString1, String paramString2, Attributes paramAttributes)
    throws SAXException;
}
