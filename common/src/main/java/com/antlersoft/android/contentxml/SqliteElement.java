/**
 * Copyright (C) 2010 Michael A. MacDonald
 */
package com.antlersoft.android.contentxml;

import java.io.IOException;
import java.io.Reader;
import java.io.Writer;
import java.util.ArrayList;
import java.util.Stack;

import android.content.ContentValues;
import android.database.Cursor;
import android.database.DatabaseUtils;
import android.database.SQLException;
import net.sqlcipher.database.SQLiteDatabase;
import android.util.Xml;

import org.xml.sax.Attributes;
import org.xml.sax.ContentHandler;
import org.xml.sax.Locator;
import org.xml.sax.SAXException;
import org.xml.sax.helpers.DefaultHandler;
import org.xmlpull.v1.XmlSerializer;

import com.antlersoft.util.xml.HandlerStack;
import com.antlersoft.util.xml.IElement;
import com.antlersoft.util.xml.IHandlerStack;
import com.antlersoft.util.xml.SimpleAttributes;

/**
 * Save or restore the contents of an entire SQLiteDatabase (or entire tables in a SQLite database)
 * as an XML element.
 * <p>
 * This assumes that SQLite table names and
 * SQLite column contents can be stored correctly in an XML attribute.
 * @author Michael A. MacDonald
 *
 */
public class SqliteElement implements IElement {
	
	/**
	 * android xml parsing does not use an XmlReader that
	 * HandlerStack was designed around; instead we'll interpose
	 * this content handler layer that manages a stack of
	 * ContentHandlers.
	 * @author Michael A. MacDonald
	 *
	 */
	public static class StackContentHandler implements ContentHandler, IHandlerStack {

		private Stack<ContentHandler> _stack = new Stack<ContentHandler>();
		/* (non-Javadoc)
		 * @see com.antlersoft.util.xml.IHandlerStack#popHandlerStack()
		 */
		@Override
		public void popHandlerStack() {
			_stack.pop();
		}

		/* (non-Javadoc)
		 * @see com.antlersoft.util.xml.IHandlerStack#pushHandlerStack(org.xml.sax.helpers.DefaultHandler)
		 */
		@Override
		public void pushHandlerStack(DefaultHandler handler) {
			_stack.push(handler);
		}

		/* (non-Javadoc)
		 * @see com.antlersoft.util.xml.IHandlerStack#startWithHandler(org.xml.sax.helpers.DefaultHandler, java.lang.String, java.lang.String, java.lang.String, org.xml.sax.Attributes)
		 */
		@Override
		public void startWithHandler(DefaultHandler handler, String uri,
				String localName, String qName, Attributes attributes)
				throws SAXException {
			_stack.push(handler);
			handler.startElement(uri, localName, qName, attributes);
		}

	
		/* (non-Javadoc)
		 * @see org.xml.sax.ContentHandler#characters(char[], int, int)
		 */
		@Override
		public void characters(char[] arg0, int arg1, int arg2)
				throws SAXException {
			_stack.peek().characters(arg0, arg1, arg2);
		}

		/* (non-Javadoc)
		 * @see org.xml.sax.ContentHandler#endDocument()
		 */
		@Override
		public void endDocument() throws SAXException {
			_stack.peek().endDocument();
		}

		/* (non-Javadoc)
		 * @see org.xml.sax.ContentHandler#endElement(java.lang.String, java.lang.String, java.lang.String)
		 */
		@Override
		public void endElement(String arg0, String arg1, String arg2)
				throws SAXException {
			_stack.peek().endElement(arg0, arg1, arg2);
		}

		/* (non-Javadoc)
		 * @see org.xml.sax.ContentHandler#endPrefixMapping(java.lang.String)
		 */
		@Override
		public void endPrefixMapping(String arg0) throws SAXException {
			_stack.peek().endPrefixMapping(arg0);
		}

		/* (non-Javadoc)
		 * @see org.xml.sax.ContentHandler#ignorableWhitespace(char[], int, int)
		 */
		@Override
		public void ignorableWhitespace(char[] arg0, int arg1, int arg2)
				throws SAXException {
			_stack.peek().ignorableWhitespace(arg0, arg1, arg2);
		}

		/* (non-Javadoc)
		 * @see org.xml.sax.ContentHandler#processingInstruction(java.lang.String, java.lang.String)
		 */
		@Override
		public void processingInstruction(String arg0, String arg1)
				throws SAXException {
			_stack.peek().processingInstruction(arg0, arg1);
		}

		/* (non-Javadoc)
		 * @see org.xml.sax.ContentHandler#setDocumentLocator(org.xml.sax.Locator)
		 */
		@Override
		public void setDocumentLocator(Locator arg0) {
			_stack.peek().setDocumentLocator(arg0);
		}

		/* (non-Javadoc)
		 * @see org.xml.sax.ContentHandler#skippedEntity(java.lang.String)
		 */
		@Override
		public void skippedEntity(String arg0) throws SAXException {
			_stack.peek().skippedEntity(arg0);
		}

		/* (non-Javadoc)
		 * @see org.xml.sax.ContentHandler#startDocument()
		 */
		@Override
		public void startDocument() throws SAXException {
			_stack.peek().startDocument();
		}

		/* (non-Javadoc)
		 * @see org.xml.sax.ContentHandler#startElement(java.lang.String, java.lang.String, java.lang.String, org.xml.sax.Attributes)
		 */
		@Override
		public void startElement(String arg0, String arg1, String arg2,
				Attributes arg3) throws SAXException {
			_stack.peek().startElement(arg0, arg1, arg2, arg3);
		}

		/* (non-Javadoc)
		 * @see org.xml.sax.ContentHandler#startPrefixMapping(java.lang.String, java.lang.String)
		 */
		@Override
		public void startPrefixMapping(String arg0, String arg1)
				throws SAXException {
			_stack.peek().startPrefixMapping(arg0, arg1);
		}

	}
	private ArrayList<String> _tableNames;
	SQLiteDatabase _db;
	private ReplaceStrategy _replaceStrategy;
	private String _databaseTag;
	
	static final String[] TABLE_ARRAY = new String[] { "name" };
	static final String TABLE_ELEMENT = "table";
	static final String TABLE_NAME_ATTRIBUTE = "table_name";
	static final String ROW_ELEMENT = "row";
	
	/**
	 * Determines how existing rows in the table are handled when reading a row from XML causes
	 * constraint violation.
	 * @author Michael A. MacDonald
	 *
	 */
	public static enum ReplaceStrategy
	{
		/**
		 * All existing rows are dropped from the table before any rows are added
		 */
		REPLACE_ALL,
		/**
		 * If an added row causes a constraint violation, drop the existing row.  This is the default
		 * behavior.  Depends on the existence of the _id column to work correctly.
		 */
		REPLACE_EXISTING,
		/**
		 * If an added row causes a constraint violation, it is ignored and the existing row is
		 * preserved.
		 */
		REPLACE_NONE
	}
	
	/**
	 * Convenience function to write a complete database as Xml to a Writer
	 * @param db
	 * @param output
	 * @throws SAXException Problem with Xml serialization
	 * @throws IOException Problem interacting with output
	 */
	public static void exportDbAsXmlToStream(SQLiteDatabase db, Writer output)
	throws SAXException, IOException
	{
		XmlSerializer serializer = Xml.newSerializer();
		serializer.setOutput(output);
		new SqliteElement(db, "database").writeToXML(new XmlSerializerHandler(serializer));
		serializer.flush();
	}
	
	/**
	 * Convenience function to read a complete database from an XML stream
	 * @param db Writable database
	 * @param input Character stream for XML representation of database as written by SqliteElement
	 * @param replace How to handle existing rows with same id's
	 * @throws SAXException Problem interpreting XML stream
	 * @throws IOException Problem interacting with reader
	 */
	public static void importXmlStreamToDb(SQLiteDatabase db, Reader input, ReplaceStrategy replace)
	throws SAXException, IOException
	{
		SqliteElement element = new SqliteElement(db, "database");
		element.setReplaceStrategy(replace);
		StackContentHandler handler = new StackContentHandler();
		handler.pushHandlerStack(element.readFromXML(handler));
		Xml.parse(input, handler);
	}
	
	/**
	 * 
	 * @param db Database to save/load
	 * @param dbName Name of XML element containing entire database
	 */
	public SqliteElement(SQLiteDatabase db, String dbName)
	{
		_db = db;
		_tableNames = new ArrayList<String>();
		_databaseTag = dbName;
		_replaceStrategy = ReplaceStrategy.REPLACE_EXISTING;
	}
	
	public ReplaceStrategy getReplaceStrategy()
	{
		return _replaceStrategy;
	}
	
	public void setReplaceStrategy(ReplaceStrategy newStrategy)
	{
		_replaceStrategy = newStrategy;
	}
	
	public void addTable(String tableName)
	{
		if (! _tableNames.contains(tableName))
		{
			_tableNames.add(tableName);
		}
	}
	
	public void removeTable(String tableName)
	{
		getTableNames().remove(tableName);
	}
	
	ArrayList<String> getTableNames()
	{
		if (_tableNames.size() == 0)
		{
			Cursor c = _db.query("sqlite_master", TABLE_ARRAY, "type = 'table'", null, null, null, null);
			try
			{
				while (c.moveToNext())
				{
					String t = c.getString(0);
					String test = t.toLowerCase();
					if (! test.equals("android_metadata") && ! test.equals("sqlite_sequence"))
					{
						_tableNames.add(t);
					}
				}
			}
			finally
			{
				c.close();
			}
		}
		return _tableNames;
	}

	/* (non-Javadoc)
	 * @see com.antlersoft.util.xml.IElement#getElementTag()
	 */
	@Override
	public String getElementTag() {
		return _databaseTag;
	}

	/* (non-Javadoc)
	 * @see com.antlersoft.util.xml.IElement#readFromXML(com.antlersoft.util.xml.IHandlerStack)
	 */
	@Override
	public DefaultHandler readFromXML(IHandlerStack handlerStack) {
		return new SqliteElementHandler(handlerStack);
	}

	/* (non-Javadoc)
	 * @see com.antlersoft.util.xml.IElement#writeToXML(org.xml.sax.ContentHandler)
	 */
	@Override
	public void writeToXML(ContentHandler xmlWriter) throws SAXException {
		xmlWriter.startElement("", "", getElementTag(), null);
		for (String s : getTableNames())
		{
			SimpleAttributes attributes = new SimpleAttributes();
			attributes.addValue(TABLE_NAME_ATTRIBUTE, s);
			xmlWriter.startElement("", "", TABLE_ELEMENT, attributes.getAttributes());
			Cursor c = _db.query(s, null, null, null, null, null, null);
			try
			{
				if (c.moveToFirst())
				{
					ContentValues cv = new ContentValues();
					do
					{
						DatabaseUtils.cursorRowToContentValues(c, cv);
						new ContentValuesElement(cv, ROW_ELEMENT).writeToXML(xmlWriter);
					}
					while (c.moveToNext());
				}
			}
			finally
			{
				c.close();
			}
			xmlWriter.endElement("", "", TABLE_ELEMENT);
		}
		xmlWriter.endElement("", "", getElementTag());
	}

	/**
	 * DefaultHandler implementation that puts recognized XML into a database
	 * 
	 * @author Michael A. MacDonald
	 *
	 */
	class SqliteElementHandler extends DefaultHandler {
		
		private IHandlerStack _stack;
		private String _currentTable;
		private ContentValues _lastRow;
		private static final long INSERT_FAILED = -1L;
		
		public SqliteElementHandler(IHandlerStack handlerStack) {
			_stack = handlerStack;
		}

		/* (non-Javadoc)
		 * @see org.xml.sax.helpers.DefaultHandler#endElement(java.lang.String, java.lang.String, java.lang.String)
		 */
		@Override
		public void endElement(String uri, String localName, String qName)
				throws SAXException {
			saveLastRow();
		}

		/* (non-Javadoc)
		 * @see org.xml.sax.helpers.DefaultHandler#startElement(java.lang.String, java.lang.String, java.lang.String, org.xml.sax.Attributes)
		 */
		@Override
		public void startElement(String uri, String localName, String qName,
				Attributes attributes) throws SAXException {
			saveLastRow();
			if (localName.equals(TABLE_ELEMENT))
			{
				_currentTable = attributes.getValue(TABLE_NAME_ATTRIBUTE);
				if (_currentTable == null)
				{
					throw new SAXException(TABLE_NAME_ATTRIBUTE + " not found in " + TABLE_ELEMENT + " element.");
				}
				if (getTableNames().contains(_currentTable))
				{
					if (getReplaceStrategy() == ReplaceStrategy.REPLACE_ALL)
					{
						_db.delete(_currentTable, null, null);
					}
				}
				else
					_currentTable = null;
			}
			else if (localName.equals(ROW_ELEMENT))
			{
				_lastRow = new ContentValues();
				ContentValuesElement rowElement = new ContentValuesElement(_lastRow, ROW_ELEMENT);
				_stack.startWithHandler(rowElement.readFromXML(_stack), uri, localName, qName, attributes);
			}
		}

		private void saveLastRow()
		{
			if (_lastRow != null)
			{
				try
				{
					// See if it is a table we care about
					if (_currentTable != null)
					{
						long id = _db.insert(_currentTable, null, _lastRow);
						if (id == INSERT_FAILED)
						{
							switch (getReplaceStrategy())
							{
							case REPLACE_ALL :
								throw new SQLException("Failed to insert row in "+_currentTable+" after emptying");
							case REPLACE_EXISTING :
								_db.delete(_currentTable, "_id = ?", new String[] { _lastRow.getAsString("_id") });
								_db.insertOrThrow(_currentTable, null, _lastRow);
								break;
							}
						}
					}
				}
				finally
				{
					_lastRow = null;
				}
			}
		}
	}
	/**
	 * Copies data to an XmlSerializer; not complete; only implemented enough to work with
	 * SqliteElement
	 * @author Michael A. MacDonald
	 *
	 */
	static class XmlSerializerHandler extends DefaultHandler {
		
		XmlSerializer _serializer;
		boolean _first;
		
		XmlSerializerHandler(XmlSerializer serializer)
		{
			_serializer = serializer;
			_first = true;
		}

		/* (non-Javadoc)
		 * @see org.xml.sax.helpers.DefaultHandler#characters(char[], int, int)
		 */
		@Override
		public void characters(char[] ch, int start, int length)
				throws SAXException {
			try
			{
				_serializer.text(ch, start, length);
			}
			catch (IOException ioe)
			{
				throw new SAXException(ioe.getMessage(), ioe);
			}
		}

		/* (non-Javadoc)
		 * @see org.xml.sax.helpers.DefaultHandler#endElement(java.lang.String, java.lang.String, java.lang.String)
		 */
		@Override
		public void endElement(String uri, String localName, String qName)
				throws SAXException {
			try
			{
				_serializer.endTag(uri, qName);
			}
			catch (IOException ioe)
			{
				throw new SAXException(ioe.getMessage(), ioe);
			}
		}

		/* (non-Javadoc)
		 * @see org.xml.sax.helpers.DefaultHandler#ignorableWhitespace(char[], int, int)
		 */
		@Override
		public void ignorableWhitespace(char[] ch, int start, int length)
				throws SAXException {
			try
			{
				_serializer.ignorableWhitespace(new String(ch, start, length));
			}
			catch (IOException ioe)
			{
				throw new SAXException(ioe.getMessage(), ioe);
			}
		}

		/* (non-Javadoc)
		 * @see org.xml.sax.helpers.DefaultHandler#startElement(java.lang.String, java.lang.String, java.lang.String, org.xml.sax.Attributes)
		 */
		@Override
		public void startElement(String uri, String localName, String qName,
				Attributes attributes) throws SAXException {
			try
			{
				if (_first)
					_first = false;
				else
					_serializer.ignorableWhitespace("\r\n");
				_serializer.startTag(uri, qName);
				if (attributes != null)
				{
					int l = attributes.getLength();
					for (int i = 0; i < l; i++)
					{
						_serializer.attribute(attributes.getURI(i), attributes.getQName(i), attributes.getValue(i));
					}
				}
			}
			catch (IOException ioe)
			{
				throw new SAXException(ioe.getMessage(), ioe);
			}
		}

	}

}
