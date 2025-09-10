package com.antlersoft.util.xml;

import org.xml.sax.Attributes;
import org.xml.sax.SAXException;
import org.xml.sax.helpers.DefaultHandler;

public abstract interface IHandlerStack
{
  public abstract void pushHandlerStack(DefaultHandler paramDefaultHandler);

  public abstract void startWithHandler(DefaultHandler paramDefaultHandler, String paramString1, String paramString2, String paramString3, Attributes paramAttributes)
    throws SAXException;

  public abstract void popHandlerStack();
}
