
/**
 * Title:        <p>
 * Description:  Java object database; also code analysis tool<p>
 * <p>Copyright (c) 2000-2005  Michael A. MacDonald<p>
 * ----- - - -- - - --
 * <p>
 *     This package is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * <p>
 *     This package is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * <p>
 *     You should have received a copy of the GNU General Public License
 *     along with the package (see gpl.txt); if not, see www.gnu.org
 * <p>
 * ----- - - -- - - --
 * Company:      <p>
 * @author Michael MacDonald
 * @version 1.0
 */
package com.antlersoft.classwriter;

import java.io.DataOutputStream;
import java.io.IOException;

import java.util.Collection;
import java.util.Stack;

public abstract class OpCode
{
	private int value;
	private String mnemonic;

	OpCode( int v, String m)
	{
	    value=v;
	    mnemonic=m;
	    if ( opCodes[v]!=null)
	    {
			throw new IllegalStateException();
	    }
	    opCodes[v]=this;
	}

 	public final String getMnemonic()
    {
        return mnemonic;
    }

	abstract Instruction read( InstructionPointer cr, byte[] code)
 		throws CodeCheckException;
 	abstract Stack stackUpdate( Instruction instruction, Stack current,
  		CodeAttribute attribute)
  		throws CodeCheckException;
    abstract void traverse( Instruction instruction, Collection next,
    	CodeAttribute attribute)
    	throws CodeCheckException;

    boolean isValidOperandLength( int len, boolean wide)
    {
        return true;
    }

    void fixStartAddress( Instruction instruction,
        int start, int oldPostEnd, int newPostEnd)
    {
        if ( instruction.instructionStart>=oldPostEnd)
            instruction.instructionStart+=newPostEnd-oldPostEnd;
    }

    /**
     * Fix operands to reflect changed code positions;
     * no op except for instructions that jump or branch
     */
    void fixDestinationAddress( Instruction instruction,
                                         int start,
                                         int oldPostEnd,
                                         int newPostEnd)
        throws CodeCheckException
    {

    }

    /**
     * When a method is edited, *Switch instructions may have to end up
     * with different operand lengths because they are supposed to be
     * padded to a 4 byte boundary.  This method returns null for
     * regular instructions; for repaddable instructions it returns a
     * new instruction with the operands updated to reflect the correct
     * padding.
     */
	Instruction repadInstruction( Instruction to_repad)
        throws CodeCheckException
	{
		return null;
	}

 	void write( DataOutputStream out, Instruction instruction)
  		throws IOException
    {
    	out.writeByte( value);
        if ( instruction.operands!=null)
            out.write( instruction.operands);
    }

    static OpCode[] opCodes;

    static
    {
    	opCodes=new OpCode[256];
    }

    public static OpCode getOpCodeByMnemonic( String mnemonic)
        throws CodeCheckException
    {
    	for ( int i=0; i<256; i++)
     	{
      		if ( opCodes[i]!=null && opCodes[i].mnemonic.equals( mnemonic))
        	{
         		return opCodes[i];
         	}
      	}
        throw new CodeCheckException( mnemonic+" not found");
    }

    static byte[] getSubArray( byte[] code, int offset, int length)
    	throws CodeCheckException
    {
        if ( length==0)
        	return null;
        byte[] result=new byte[length];
        if ( code.length<offset+length)
        	throw new CodeCheckException(
         	"Code segment is too short for instruction");
        for ( int i=0; i<length; i++)
        {
            result[i]=code[offset+i];
        }
        return result;
    }

    static
    {
		try
		{
		    new SimpleOpCode( 50, 1, "aaload", new Cat1Stack( 2, 1));
		    new SimpleOpCode( 83, 1, "aastore", new Cat1Stack( 3, 0));
		    new SimpleOpCode( 1, 1, "aconst_null", new Cat1Stack( 0, 1));
		    new WSimpleOpCode( 25, 2, "aload", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 42, 1, "aload_0", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 43, 1, "aload_1", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 44, 1, "aload_2", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 45, 1, "aload_3", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 189, 3, "anewarray");
		    new ReturnOpCode( 176, 1, "areturn", new Cat1Stack( 1, 0));
		    new SimpleOpCode( 190, 1, "arraylength");
		    new WSimpleOpCode( 58, 2, "astore", new Cat1Stack( 1, 0));
		    new SimpleOpCode( 75, 1, "astore_0", new Cat1Stack( 1, 0));
		    new SimpleOpCode( 76, 1, "astore_1", new Cat1Stack( 1, 0));
		    new SimpleOpCode( 77, 1, "astore_2", new Cat1Stack( 1, 0));
		    new SimpleOpCode( 78, 1, "astore_3", new Cat1Stack( 1, 0));
		    new ReturnOpCode( 191, 1, "athrow", new Cat1Stack( 1, 0));
		    new SimpleOpCode( 51, 1, "baload", new Cat1Stack( 2, 1));
		    new SimpleOpCode( 84, 1, "bastore", new Cat1Stack( 3, 0));
		    new SimpleOpCode( 16, 2, "bipush", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 52, 1, "caload", new Cat1Stack( 2, 1));
		    new SimpleOpCode( 85, 1, "castore", new Cat1Stack( 3, 0));
		    new SimpleOpCode( 192, 3, "checkcast");
		    new SimpleOpCode( 144, 1, "d2f", new ConvertStack( ProcessStack.CAT2, ProcessStack.CAT1));
		    new SimpleOpCode( 142, 1, "d2i", new ConvertStack( ProcessStack.CAT2, ProcessStack.CAT1));
		    new SimpleOpCode( 143, 1, "d2l", new ConvertStack( ProcessStack.CAT2, ProcessStack.CAT1));
		    new SimpleOpCode( 99, 1, "dadd", new Cat2Stack( 2, 1));
		    new SimpleOpCode( 49, 1, "daload", new ComboStack( new Cat1Stack( 2, 0), new Cat2Stack( 0, 1)));
		    new SimpleOpCode( 82, 1, "dastore", new ComboStack( new Cat1Stack( 2, 0), new Cat2Stack( 1, 0)));
		    new SimpleOpCode( 152, 1, "dcmpg", new ComboStack( new Cat2Stack( 2, 0), new Cat1Stack( 0, 1)));
		    new SimpleOpCode( 151, 1, "dcmpl", new ComboStack( new Cat2Stack( 2, 0), new Cat1Stack( 0, 1)));
		    new SimpleOpCode( 14, 1, "dconst_0", new Cat2Stack( 0, 1));
		    new SimpleOpCode( 15, 1, "dconst_1", new Cat2Stack( 0, 1));
		    new SimpleOpCode( 111, 1, "ddiv", new Cat2Stack( 2, 1));
		    new WSimpleOpCode( 24, 2, "dload", new Cat2Stack( 0, 1));
		    new SimpleOpCode( 38, 1, "dload_0", new Cat2Stack( 0, 1));
		    new SimpleOpCode( 39, 1, "dload_1", new Cat2Stack( 0, 1));
		    new SimpleOpCode( 40, 1, "dload_2", new Cat2Stack( 0, 1));
		    new SimpleOpCode( 41, 1, "dload_3", new Cat2Stack( 0, 1));
		    new SimpleOpCode( 107, 1, "dmul", new Cat2Stack( 2, 1));
		    new SimpleOpCode( 119, 1, "dneg", new Cat2Stack( 1, 1));
		    new SimpleOpCode( 115, 1, "drem", new Cat2Stack( 2, 1));
		    new ReturnOpCode( 175, 1, "dreturn", new Cat2Stack( 1, 0));
		    new WSimpleOpCode( 57, 2, "dstore", new Cat2Stack( 1, 0));
		    new SimpleOpCode( 71, 1, "dstore_0", new Cat2Stack( 1, 0));
		    new SimpleOpCode( 72, 1, "dstore_1", new Cat2Stack( 1, 0));
		    new SimpleOpCode( 73, 1, "dstore_2", new Cat2Stack( 1, 0));
		    new SimpleOpCode( 74, 1, "dstore_3", new Cat2Stack( 1, 0));
		    new SimpleOpCode( 103, 1, "dsub", new Cat2Stack( 2, 1));
		    new SimpleOpCode( 89, 1, "dup", new Cat1Stack( 1, 2));
		    new SimpleOpCode( 90, 1, "dup_x1", new Cat1Stack( 2, 3));
		    new SimpleOpCode( 91, 1, "dup_x2", new DupX2Stack());
		    new SimpleOpCode( 92, 1, "dup2", new Dup2Stack());
		    new SimpleOpCode( 93, 1, "dup2_x1", new Dup2X1Stack());
		    new SimpleOpCode( 94, 1, "dup2_x2", new Dup2X2Stack());
		    new SimpleOpCode( 141, 1, "f2d", new ConvertStack( ProcessStack.CAT1, ProcessStack.CAT2));
		    new SimpleOpCode( 139, 1, "f2i");
		    new SimpleOpCode( 140, 1, "f2l", new ConvertStack( ProcessStack.CAT1, ProcessStack.CAT2));
		    new SimpleOpCode( 98, 1, "fadd", new Cat1Stack( 2, 1));
		    new SimpleOpCode( 48, 1, "faload", new Cat1Stack( 2, 1));
		    new SimpleOpCode( 81, 1, "fastore", new Cat1Stack( 3, 0));
		    new SimpleOpCode( 150, 1, "fcmpg", new Cat1Stack( 2, 1));
		    new SimpleOpCode( 149, 1, "fcmpl", new Cat1Stack( 2, 1));
		    new SimpleOpCode( 11, 1, "fconst_0", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 12, 1, "fconst_1", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 13, 1, "fconst_2", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 110, 1, "fdiv", new Cat1Stack( 2, 1));
		    new WSimpleOpCode( 23, 2, "fload", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 34, 1, "fload_0", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 35, 1, "fload_1", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 36, 1, "fload_2", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 37, 1, "fload_3", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 106, 1, "fmul", new Cat1Stack( 2, 1));
		    new SimpleOpCode( 118, 1, "fneg");
		    new SimpleOpCode( 114, 1, "frem", new Cat1Stack( 2, 1));
		    new ReturnOpCode( 174, 1, "freturn", new Cat1Stack( 1, 0));
		    new WSimpleOpCode( 56, 2, "fstore", new Cat1Stack( 1, 0));
		    new SimpleOpCode( 67, 1, "fstore_0", new Cat1Stack( 1, 0));
		    new SimpleOpCode( 68, 1, "fstore_1", new Cat1Stack( 1, 0));
		    new SimpleOpCode( 69, 1, "fstore_2", new Cat1Stack( 1, 0));
		    new SimpleOpCode( 70, 1, "fstore_3", new Cat1Stack( 1, 0));
		    new SimpleOpCode( 102, 1, "fsub", new Cat1Stack( 2, 1));
		    new GetOpCode( 180, 3, "getfield");
		    new GetOpCode( 178, 3, "getstatic");
		    new GotoOpCode( 167, 3, "goto");
		    new GotoOpCode( 200, 5, "goto_w");
		    new SimpleOpCode( 145, 1, "i2b");
		    new SimpleOpCode( 146, 1, "i2c");
		    new SimpleOpCode( 135, 1, "i2d", new ConvertStack( ProcessStack.CAT1, ProcessStack.CAT2));
		    new SimpleOpCode( 134, 1, "i2f");
		    new SimpleOpCode( 133, 1, "i2l", new ConvertStack( ProcessStack.CAT1, ProcessStack.CAT2));
		    new SimpleOpCode( 147, 1, "i2s");
		    new SimpleOpCode( 96, 1, "iadd", new Cat1Stack( 2, 1));
		    new SimpleOpCode( 46, 1, "iaload", new Cat1Stack( 2, 1));
		    new SimpleOpCode( 126, 1, "iand", new Cat1Stack( 2, 1));
		    new SimpleOpCode( 79, 1, "iastore", new Cat1Stack( 3, 0));
		    new SimpleOpCode( 2, 1, "iconst_m1", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 3, 1, "iconst_0", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 4, 1, "iconst_1", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 5, 1, "iconst_2", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 6, 1, "iconst_3", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 7, 1, "iconst_4", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 8, 1, "iconst_5", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 108, 1, "idiv", new Cat1Stack( 2, 1));
		    new BranchOpCode( 165, 3, "if_acmpeq", 2);
		    new BranchOpCode( 166, 3, "if_acmpne", 2);
		    new BranchOpCode( 159, 3, "if_icmpeq", 2);
		    new BranchOpCode( 160, 3, "if_icmpne", 2);
		    new BranchOpCode( 161, 3, "if_icmplt", 2);
		    new BranchOpCode( 162, 3, "if_icmpge", 2);
		    new BranchOpCode( 163, 3, "if_icmpgt", 2);
		    new BranchOpCode( 164, 3, "if_icmple", 2);
		    new BranchOpCode( 153, 3, "ifeq", 1);
		    new BranchOpCode( 154, 3, "ifne", 1);
		    new BranchOpCode( 155, 3, "iflt", 1);
		    new BranchOpCode( 156, 3, "ifge", 1);
		    new BranchOpCode( 157, 3, "ifgt", 1);
		    new BranchOpCode( 158, 3, "ifle", 1);
		    new BranchOpCode( 199, 3, "ifnonnull", 1);
		    new BranchOpCode( 198, 3, "ifnull", 1);
		    new WSimpleOpCode( 132, 3, "iinc", new Cat1Stack( 0, 0));
		    new WSimpleOpCode( 21, 2, "iload", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 26, 1, "iload_0", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 27, 1, "iload_1", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 28, 1, "iload_2", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 29, 1, "iload_3", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 104, 1, "imul", new Cat1Stack( 2, 1));
		    new SimpleOpCode( 116, 1, "ineg");
		    new SimpleOpCode( 193, 3, "instanceof");
		    new InvokeOpCode( 185, 5, "invokeinterface");
		    new InvokeOpCode( 183, 3, "invokespecial");
		    new InvokeOpCode( 184, 3, "invokestatic");
		    new InvokeOpCode( 182, 3, "invokevirtual");
		    new SimpleOpCode( 128, 1, "ior", new Cat1Stack( 2, 1));
		    new SimpleOpCode( 112, 1, "irem", new Cat1Stack( 2, 1));
		    new ReturnOpCode( 172, 1, "ireturn", new Cat1Stack( 1, 0));
		    new SimpleOpCode( 120, 1, "ishl", new Cat1Stack( 2, 1));
		    new SimpleOpCode( 122, 1, "ishr", new Cat1Stack( 2, 1));
		    new WSimpleOpCode( 54, 2, "istore", new Cat1Stack( 1, 0));
		    new SimpleOpCode( 59, 1, "istore_0", new Cat1Stack( 1, 0));
		    new SimpleOpCode( 60, 1, "istore_1", new Cat1Stack( 1, 0));
		    new SimpleOpCode( 61, 1, "istore_2", new Cat1Stack( 1, 0));
		    new SimpleOpCode( 62, 1, "istore_3", new Cat1Stack( 1, 0));
		    new SimpleOpCode( 100, 1, "isub", new Cat1Stack( 2, 1));
		    new SimpleOpCode( 124, 1, "iushr", new Cat1Stack( 2, 1));
		    new SimpleOpCode( 130, 1, "ixor", new Cat1Stack( 2, 1));
		    new JsrOpCode( 168, 3, "jsr"); // Special handling in check code
		    new JsrOpCode( 201, 5, "jsr_w"); // ''
		    new SimpleOpCode( 138, 1, "l2d", new ConvertStack( ProcessStack.CAT2, ProcessStack.CAT2));
		    new SimpleOpCode( 137, 1, "l2f", new ConvertStack( ProcessStack.CAT2, ProcessStack.CAT1));
		    new SimpleOpCode( 136, 1, "l2i", new ConvertStack( ProcessStack.CAT2, ProcessStack.CAT1));
		    new SimpleOpCode( 97, 1, "ladd", new Cat2Stack( 2, 1));
		    new SimpleOpCode( 47, 1, "laload", new ComboStack( new Cat1Stack( 2, 0), new Cat2Stack( 0, 1)));
		    new SimpleOpCode( 127, 1, "land", new Cat2Stack( 2, 1));
		    new SimpleOpCode( 80, 1, "lastore", new ComboStack( new Cat1Stack( 2, 0), new Cat2Stack( 1, 0)));
		    new SimpleOpCode( 148, 1, "lcmp", new ComboStack( new Cat2Stack( 2, 0), new Cat1Stack( 0, 1)));
		    new SimpleOpCode( 9, 1, "lconst_0", new Cat2Stack( 0, 1));
		    new SimpleOpCode( 10, 1, "lconst_1", new Cat2Stack( 0, 1));
		    new SimpleOpCode( 18, 2, "ldc", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 19, 3, "ldc_w", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 20, 3, "ldc2_w", new Cat2Stack( 0, 1));
		    new SimpleOpCode( 109, 1, "ldiv", new Cat2Stack( 2, 1));
		    new WSimpleOpCode( 22, 2, "lload", new Cat2Stack( 0, 1));
		    new SimpleOpCode( 30, 1, "lload_0", new Cat2Stack( 0, 1));
		    new SimpleOpCode( 31, 1, "lload_1", new Cat2Stack( 0, 1));
		    new SimpleOpCode( 32, 1, "lload_2", new Cat2Stack( 0, 1));
		    new SimpleOpCode( 33, 1, "lload_3", new Cat2Stack( 0, 1));
		    new SimpleOpCode( 105, 1, "lmul", new Cat2Stack( 2, 1));
		    new SimpleOpCode( 117, 1, "lneg", new Cat2Stack( 1, 1));
		    new LookupSwitch( 171, "lookupswitch");
		    new SimpleOpCode( 129, 1, "lor", new Cat2Stack( 2, 1));
		    new SimpleOpCode( 113, 1, "lrem", new Cat2Stack( 2, 1));
		    new ReturnOpCode( 173, 1, "lreturn", new Cat2Stack( 1, 0));
		    new SimpleOpCode( 121, 1, "lshl", new ComboStack( new Cat2Stack( 1, 0), new ComboStack( new Cat1Stack( 1, 0), new Cat2Stack( 0, 1))));
		    new SimpleOpCode( 123, 1, "lshr", new ComboStack( new Cat2Stack( 1, 0), new ComboStack( new Cat1Stack( 1, 0), new Cat2Stack( 0, 1))));
		    new WSimpleOpCode( 55, 2, "lstore", new Cat2Stack( 1, 0));
		    new SimpleOpCode( 63, 1, "lstore_0", new Cat2Stack( 1, 0));
		    new SimpleOpCode( 64, 1, "lstore_1", new Cat2Stack( 1, 0));
		    new SimpleOpCode( 65, 1, "lstore_2", new Cat2Stack( 1, 0));
		    new SimpleOpCode( 66, 1, "lstore_3", new Cat2Stack( 1, 0));
		    new SimpleOpCode( 101, 1, "lsub", new Cat2Stack( 2, 1));
		    new SimpleOpCode( 125, 1, "lushr", new ComboStack( new Cat2Stack( 1, 0), new ComboStack( new Cat1Stack( 1, 0), new Cat2Stack( 0, 1))));
		    new SimpleOpCode( 131, 1, "lxor", new Cat2Stack( 2, 1));
		    new SimpleOpCode( 194, 1, "monitorenter", new Cat1Stack( 1, 0));
		    new SimpleOpCode( 195, 1, "monitorexit", new Cat1Stack( 1, 0));
		    new MultiArrayOpCode( 197, 4, "multianewarray");
		    new SimpleOpCode( 187, 3, "new", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 188, 2, "newarray");
		    new SimpleOpCode( 0, 1, "nop", new Cat1Stack( 0, 0));
		    new SimpleOpCode( 87, 1, "pop", new Cat1Stack( 1, 0));
		    new SimpleOpCode( 88, 1, "pop2", new Pop2Stack());
		    new GetOpCode( 181, 3, "putfield");
		    new GetOpCode( 179, 3, "putstatic");
		    new ReturnOpCode( 169, 2, "ret", new Cat1Stack( 0, 0));
		    new ReturnOpCode( 177, 1, "return", new Cat1Stack( 0, 0));
		    new SimpleOpCode( 53, 1, "saload", new Cat1Stack( 2,1));
		    new SimpleOpCode( 86, 1, "sastore", new Cat1Stack( 3, 0));
		    new SimpleOpCode( 17, 3, "sipush", new Cat1Stack( 0, 1));
		    new SimpleOpCode( 95, 1, "swap", new Cat1Stack( 2, 2));
		    new TableSwitch( 170, "tableswitch");
		    new Wide( 196, "wide");
		}
		catch ( Throwable t)
		{
		    t.printStackTrace();
		}
    }
}
