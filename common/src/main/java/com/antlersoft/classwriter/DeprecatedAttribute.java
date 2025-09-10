package com.antlersoft.classwriter;

import java.io.DataOutputStream;
import java.io.IOException;

class DeprecatedAttribute implements Attribute {
	
	static final String typeString="Deprecated";

	public void write(DataOutputStream classStream) throws IOException {
	}

	public String getTypeString() {
		return typeString;
	}

}
