package com.antlersoft.classwriter;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

public class SourceFileAttribute implements Attribute {
    public final static String typeString="SourceFile";
    private ClassWriter _containing;
    private String _sourceFile;

    public void write(DataOutputStream classStream) throws IOException {
        classStream.writeShort( _containing.getStringConstantIndex( _sourceFile));
    }
    public String getTypeString() {
        return typeString;
    }
    public String getSourceFile()
    {
        return _sourceFile;
    }

    SourceFileAttribute( String name, ClassWriter containing)
    {
        _containing=containing;
        _sourceFile=name;
    }

    SourceFileAttribute( DataInputStream classStream, ClassWriter containing)
    throws IOException
    {
        _containing=containing;
        _sourceFile=containing.getString( classStream.readUnsignedShort());
    }
}