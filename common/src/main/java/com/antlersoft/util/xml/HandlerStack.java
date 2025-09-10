package com.antlersoft.util.xml;

import java.util.Stack;
import org.xml.sax.Attributes;
import org.xml.sax.SAXException;
import org.xml.sax.XMLReader;
import org.xml.sax.helpers.DefaultHandler;

public class HandlerStack
  implements IHandlerStack
{
  private Stack m_handler_stack = new Stack();
  private XMLReader m_reader;

  public HandlerStack(XMLReader paramXMLReader)
  {
    this.m_reader = paramXMLReader;
  }

  public void pushHandlerStack(DefaultHandler paramDefaultHandler)
  {
    this.m_handler_stack.push(paramDefaultHandler);
    if (this.m_reader != null)
    {
      this.m_reader.setContentHandler(paramDefaultHandler);
      this.m_reader.setErrorHandler(paramDefaultHandler);
    }
  }

  public void popHandlerStack()
  {
    this.m_handler_stack.pop();
    if ((this.m_reader != null) && (this.m_handler_stack.size() > 0))
    {
      DefaultHandler localDefaultHandler = (DefaultHandler)this.m_handler_stack.peek();
      this.m_reader.setContentHandler(localDefaultHandler);
      this.m_reader.setErrorHandler(localDefaultHandler);
    }
  }

  public void startWithHandler(DefaultHandler paramDefaultHandler, String paramString1, String paramString2, String paramString3, Attributes paramAttributes)
    throws SAXException
  {
    pushHandlerStack(paramDefaultHandler);
    paramDefaultHandler.startElement(paramString1, paramString2, paramString3, paramAttributes);
  }
}
