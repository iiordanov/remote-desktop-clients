package com.antlersoft.util.xml;

import org.xml.sax.ContentHandler;
import org.xml.sax.SAXException;
import org.xml.sax.helpers.DefaultHandler;

public abstract interface IElement
{
  public abstract DefaultHandler readFromXML(IHandlerStack paramIHandlerStack);

  public abstract void writeToXML(ContentHandler paramContentHandler)
    throws SAXException;

  public abstract String getElementTag();
}
