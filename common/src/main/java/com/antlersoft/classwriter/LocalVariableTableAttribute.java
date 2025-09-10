/**
 * Copyright 2006 Michael A. MacDonald
 */
package com.antlersoft.classwriter;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

import java.util.ArrayList;
import java.util.Iterator;

/**
 * @author mike
 *
 */
public class LocalVariableTableAttribute implements Attribute {
	
	static final String typeString="LocalVariableTable";

	private ArrayList entries;
	
	static class LocalVariableTableEntry
	{
		int start_pc;
		int length;
		int name_index;
		int descriptor_index;
		int variable_index;
		
		LocalVariableTableEntry( DataInputStream stream)
		throws IOException
		{
			start_pc=stream.readUnsignedShort();
			length=stream.readUnsignedShort();
			name_index=stream.readUnsignedShort();
			descriptor_index=stream.readUnsignedShort();
			variable_index=stream.readUnsignedShort();
		}
	}
	
	LocalVariableTableAttribute( DataInputStream stream)
	throws IOException
	{
		int length=stream.readUnsignedShort();
		entries=new ArrayList(length);
		for ( int i=0; i<length; ++i)
		{
			entries.add( new LocalVariableTableEntry( stream));
		}
	}
	
	/**
	 * Return the name and descriptor of a local variable reference, or null
	 * if the appropriate local variable can't be found.
	 * 
	 * @param cw ClassWriter containing the method that includes the local variable
	 * @param pc Program counter (byte offset) of the variable reference in the code for the method
	 * @param index Index of the local variable in the method's frame
	 */
	public String getLocalVariable( ClassWriter cw, int pc, int index)
	{
		for ( Iterator i=entries.iterator(); i.hasNext();)
		{
			LocalVariableTableEntry entry=(LocalVariableTableEntry)i.next();
			if ( pc>=entry.start_pc && pc<entry.start_pc+entry.length && index==entry.variable_index)
			{
				return cw.getString( entry.name_index)+" ("+cw.getString( entry.descriptor_index)+")";
			}
		}
		return null;
	}

	/* (non-Javadoc)
	 * @see com.antlersoft.classwriter.Attribute#write(java.io.DataOutputStream)
	 */
	public void write(DataOutputStream classStream) throws IOException {
		int length=entries.size();
		classStream.writeShort( length);

		for ( Iterator i=entries.iterator(); i.hasNext();)
		{
			LocalVariableTableEntry entry=(LocalVariableTableEntry)i.next();
			classStream.writeShort( entry.start_pc);
			classStream.writeShort( entry.length);
			classStream.writeShort( entry.name_index);
			classStream.writeShort( entry.descriptor_index);
			classStream.writeShort( entry.variable_index);
		}
	}

	/* (non-Javadoc)
	 * @see com.antlersoft.classwriter.Attribute#getTypeString()
	 */
	public String getTypeString() {
		return typeString;
	}

    /**
     * Update the line number table for inserted/replaced code.  Entries
     * pointing to code that has been replaced are removed.
     */
    void fixOffsets( int start, int oldPostEnd, int newPostEnd)
    {
        for ( Iterator i=entries.iterator(); i.hasNext();)
        {
            LocalVariableTableEntry entry=(LocalVariableTableEntry)i.next();
            if ( entry.start_pc>start)
            {
                if ( entry.start_pc<oldPostEnd)
                    i.remove();
                else
                    entry.start_pc+=newPostEnd-oldPostEnd;
            }
        }
    }

}
