/**
 * Copyright (C) 2010 Michael A. MacDonald
 */
package com.antlersoft.android.contentxml;

import java.util.Map;

import android.content.ContentValues;

import org.xml.sax.Attributes;
import org.xml.sax.ContentHandler;
import org.xml.sax.SAXException;
import org.xml.sax.helpers.DefaultHandler;

import com.antlersoft.util.xml.IElement;
import com.antlersoft.util.xml.IHandlerStack;
import com.antlersoft.util.xml.ISimpleElement;
import com.antlersoft.util.xml.SimpleAttributes;
import com.antlersoft.util.xml.SimpleHandler;


/**
 * An IElement implementation for saving or loading a ContentValues to/from and XML file.  This
 * assumes that the ContentValues values can be stored correctly in an XML attribute.
 * 
 * @author Michael A. MacDonald
 *
 */
public class ContentValuesElement implements IElement, ISimpleElement {
	
	private ContentValues _values;
	private String _elementTag;
	
	public ContentValuesElement(ContentValues values, String elementTag)
	{
		_values = values;
		_elementTag = elementTag;
	}

	/* (non-Javadoc)
	 * @see com.antlersoft.util.xml.IElement#getElementTag()
	 */
	@Override
	public String getElementTag() {
		return _elementTag;
	}

	/* (non-Javadoc)
	 * @see com.antlersoft.util.xml.IElement#readFromXML(com.antlersoft.util.xml.IHandlerStack)
	 */
	@Override
	public DefaultHandler readFromXML(IHandlerStack handlerStack) {
		return new SimpleHandler(handlerStack,this);
	}

	/* (non-Javadoc)
	 * @see com.antlersoft.util.xml.IElement#writeToXML(org.xml.sax.ContentHandler)
	 */
	@Override
	public void writeToXML(ContentHandler xmlWriter) throws SAXException {
		SimpleAttributes a = new SimpleAttributes();
		
		for (Map.Entry<String,Object> entry : _values.valueSet())
		{
			if (entry.getKey() != null && entry.getValue() != null)
				a.addValue(entry.getKey(), entry.getValue());
		}
		
		xmlWriter.startElement( "", "", getElementTag(), a.getAttributes());
		xmlWriter.endElement( "", "", getElementTag());
	}

	/* (non-Javadoc)
	 * @see com.antlersoft.util.xml.ISimpleElement#gotElement(java.lang.String, java.lang.String, org.xml.sax.Attributes)
	 */
	@Override
	public void gotElement(String name, String contents, Attributes attributes)
			throws SAXException {
		int len = attributes.getLength();
		for (int i = 0; i < len; i++)
		{
			_values.put(attributes.getLocalName(i),attributes.getValue(i));
		}
	}

}
