
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

import java.util.List;

import com.antlersoft.util.NetByte;

public class Instruction
{
	OpCode opCode;
 	int instructionStart;
	byte[] operands;
 	boolean wideFlag;

    public Instruction( OpCode o, int is, byte[] ops, boolean wide)
    {
    	opCode=o;
     	instructionStart=is;
     	operands=ops;
      	wideFlag=wide;
    }

    public int getInstructionStart()
    {
        return instructionStart;
    }

    public static void addNextInstruction( List instructionList,
        String mnemonic, byte[] ops, boolean wide)
        throws CodeCheckException
    {
        int start=0;
        if ( instructionList.size()>=1)
        {
            Instruction lastInstruction=(Instruction)instructionList.
                get( instructionList.size()-1);
            start=lastInstruction.instructionStart+lastInstruction.getLength();
        }
        OpCode op_code=OpCode.getOpCodeByMnemonic( mnemonic);
        if ( ! op_code.isValidOperandLength( (ops == null) ? 0 : ops.length, wide))
            throw new CodeCheckException( "Operands wrong length in added instruction");
        instructionList.add( new Instruction(
            OpCode.getOpCodeByMnemonic( mnemonic),
            start,
            ops, wide));
    }

    public static void addNextInstruction( List instructionList,
        String mnemonic, int constIndex)
        throws CodeCheckException
    {
        byte[] operands=new byte[2];
        NetByte.intToPair( constIndex, operands, 0);
        addNextInstruction( instructionList, mnemonic, operands, false);
    }

    public int getLength()
    {
        if ( operands==null)
        	return 1;
        return operands.length+1;
    }

    public OpCode getOpCode() { return opCode; }
    public int operandsAsInt()
        throws CodeCheckException
    {
        if ( operands!=null)
        {
            switch ( operands.length)
            {
            case 1 : return NetByte.mU( operands[0]);
            case 2 : return NetByte.pairToInt( operands, 0);
            case 4 : return NetByte.quadToInt( operands, 0);
            }
        }
        throw new CodeCheckException( "Operands not integer");
    }

    /**
     * Interpret operands as symbolic reference
     */
    public ClassWriter.CPTypeRef getSymbolicReference( ClassWriter writer)
    	throws CodeCheckException
    {
        return (ClassWriter.CPTypeRef)
        	writer.constantPool.get( NetByte.pairToInt(
         	operands, 0));
    }

    /**
     * Interpret operands as destination opcode address
     */
    int getOffsetDestination()
    {
        int offset;
        if ( operands.length==4)
        {
            offset=NetByte.quadToInt( operands, 0);
        }
        else
        	offset=NetByte.pairToInt( operands, 0);
        return instructionStart+offset;
    }

    /**
     *  Fix operands for "regular" jump or branch instructions when
     * code has moved.
     */
    void fixDestinationAddress( int start, int oldPostEnd, int newPostEnd)
        throws CodeCheckException
    {
        int oldDestination=getOffsetDestination();
        if ( instructionStart>=newPostEnd)
        	oldDestination+=oldPostEnd-newPostEnd;
        if ( oldDestination>start && oldDestination<oldPostEnd)
        {
            throw new CodeCheckException(
                "Branch into code replaced by an inserted segment");
        }
        if ( oldDestination>=oldPostEnd)
        {
            int newDestination=oldDestination+newPostEnd-oldPostEnd-
                instructionStart;
            if ( operands.length==4)
                NetByte.intToQuad( newDestination, operands, 0);
            else
                NetByte.intToPair( newDestination, operands, 0);
        }
        else if ( instructionStart>=newPostEnd)
        {
            int newDestination=oldDestination-instructionStart;
	        if ( operands.length==4)
	            NetByte.intToQuad( newDestination, operands, 0);
	        else
	            NetByte.intToPair( newDestination, operands, 0);      	
        }
    }

    public static Instruction appropriateLdc( int index, boolean wide)
        throws CodeCheckException
    {
        byte[] operands;
        String mnemonic;

        if ( wide)
        {
            mnemonic="ldc2_w";
            operands=new byte[2];
            NetByte.intToPair( index, operands, 0);
        }
        else
        {
            if ( index<256)
            {
                mnemonic="ldc";
                operands=new byte[1];
                operands[0]=(byte)index;
            }
            else
            {
                mnemonic="ldc_w";
                operands=new byte[2];
                NetByte.intToPair( index, operands, 0);
            }
        }
        return new Instruction( OpCode.getOpCodeByMnemonic( mnemonic), 0,
            operands, false);
    }
}
