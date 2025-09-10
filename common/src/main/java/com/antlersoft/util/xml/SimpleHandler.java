package com.antlersoft.util.xml;

import org.xml.sax.Attributes;
import org.xml.sax.ContentHandler;
import org.xml.sax.SAXException;
import org.xml.sax.helpers.AttributesImpl;
import org.xml.sax.helpers.DefaultHandler;

public class SimpleHandler extends DefaultHandler
{
  private String m_current_element_name;
  private StringBuffer m_current_element_contents;
  private Attributes m_current_attributes;
  private ISimpleElement m_element;
  private IHandlerStack m_impl;
  public static AttributesImpl m_empty = new AttributesImpl();

  public SimpleHandler(IHandlerStack paramIHandlerStack, ISimpleElement paramISimpleElement)
  {
    this.m_impl = paramIHandlerStack;
    this.m_element = paramISimpleElement;
    this.m_current_element_contents = new StringBuffer();
  }

  public void startElement(String paramString1, String paramString2, String paramString3, Attributes paramAttributes)
    throws SAXException
  {
    this.m_current_element_name = paramString2;
    this.m_current_element_contents.setLength(0);
    AttributesImpl localAttributesImpl = new AttributesImpl();
    this.m_current_attributes = localAttributesImpl;
    int i = paramAttributes.getLength();
    for (int j = 0; j < i; j++)
      localAttributesImpl.addAttribute(paramAttributes.getURI(j), paramAttributes.getLocalName(j), paramAttributes.getQName(j), paramAttributes.getType(j), paramAttributes.getValue(j));
  }

  public void endElement(String paramString1, String paramString2, String paramString3)
    throws SAXException
  {
    this.m_element.gotElement(this.m_current_element_name, this.m_current_element_contents.toString(), this.m_current_attributes);
    if (this.m_impl != null)
      this.m_impl.popHandlerStack();
  }

  public void characters(char[] paramArrayOfChar, int paramInt1, int paramInt2)
  {
    this.m_current_element_contents.append(paramArrayOfChar, paramInt1, paramInt2);
  }

  public void ignorableWhitespace(char[] paramArrayOfChar, int paramInt1, int paramInt2)
  {
    this.m_current_element_contents.append(paramArrayOfChar, paramInt1, paramInt2);
  }

  String getCurrentElementName()
  {
    return this.m_current_element_name;
  }

  StringBuffer getContents()
  {
    return this.m_current_element_contents;
  }

  Attributes getAttributes()
  {
    return this.m_current_attributes;
  }

  public static void writeElement(ContentHandler paramContentHandler, String paramString1, String paramString2)
    throws SAXException
  {
    paramContentHandler.startElement("", paramString1, "", m_empty);
    char[] arrayOfChar = paramString2.toCharArray();
    paramContentHandler.characters(arrayOfChar, 0, arrayOfChar.length);
    paramContentHandler.endElement("", paramString1, "");
  }
}
