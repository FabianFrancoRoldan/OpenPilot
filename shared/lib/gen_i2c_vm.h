/**
 ******************************************************************************
 * @addtogroup OpenPilotModules OpenPilot Modules
 * @{ 
 * @addtogroup Generic I2C Virtual Machine Module
 * @brief Generic Programmable I2C Virtual Machine Module
 * @{ 
 *
 * @file       
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2012.
 * @brief      Generic Programmable I2C Virtual Machine Module
 *
 * @see        The GNU Public License (GPL) Version 3
 *
 *****************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef GEN_I2C_VM_H_
#define GEN_I2C_VM_H_

#define MAX_PRGM_SIZE 20 //This is the maximum number of instructions that the program can have

/* Can support up to 256 op codes */
enum i2c_vm_opcodes {
	I2C_VM_OP_HALT,         //Halt
	I2C_VM_OP_NOP,          //No operation
	I2C_VM_OP_STORE,        //Store value
	I2C_VM_OP_LOAD_BE,      //Load big endian
	I2C_VM_OP_LOAD_LE,      //Load little endian
	I2C_VM_OP_SET_CTR,      //Set counter
	I2C_VM_OP_DEC_CTR,      //Decrement counter
	I2C_VM_OP_BNZ,          //Branch if counter not equal to zero
	I2C_VM_OP_JUMP,         //Jump to 
	I2C_VM_OP_SET_DEV_ADDR, //Set I2C device address
	I2C_VM_OP_READ,         //Read from I2C bus
	I2C_VM_OP_WRITE,        //Write to I2C bus
	I2C_VM_OP_SEND_UAVO,    //Send UAV Object
	I2C_VM_OP_DELAY,        //Wait (ms)
};

enum i2c_exported_regs {
	VM_R0,
	VM_R1,
	VM_F0,
};

#define I2C_VM_ASM(operator, op1, op2, op3) (((operator & 0xFF) << 24) | \
						((op1 & 0xFF) << 16) | \
						((op2 & 0xFF) << 8) | \
						((op3 & 0xFF)))

#define I2C_VM_ASM_NOP() (I2C_VM_ASM(I2C_VM_OP_NOP, 0, 0, 0))
#define I2C_VM_ASM_HALT() (I2C_VM_ASM(I2C_VM_OP_HALT, 0, 0, 0))
#define I2C_VM_ASM_SET_DEV_ADDR(addr) (I2C_VM_ASM(I2C_VM_OP_SET_DEV_ADDR, (addr), 0, 0))
#define I2C_VM_ASM_WRITE_I2C(length) (I2C_VM_ASM(I2C_VM_OP_WRITE, (length), 0, 0))
#define I2C_VM_ASM_READ_I2C(length) (I2C_VM_ASM(I2C_VM_OP_READ, (length), 0, 0))
#define I2C_VM_ASM_DELAY(ms) (I2C_VM_ASM(I2C_VM_OP_DELAY, (ms), 0, 0))
#define I2C_VM_ASM_JUMP(rel_addr) (I2C_VM_ASM(I2C_VM_OP_JUMP, (rel_addr), 0, 0))
#define I2C_VM_ASM_STORE(value, addr) (I2C_VM_ASM(I2C_VM_OP_STORE, (value), (addr), 0))
#define I2C_VM_ASM_BNZ(rel_addr) (I2C_VM_ASM(I2C_VM_OP_BNZ, (rel_addr), 0, 0))
#define I2C_VM_ASM_SET_CTR(ctr_val) (I2C_VM_ASM(I2C_VM_OP_SET_CTR, (ctr_val), 0, 0))
#define I2C_VM_ASM_DEC_CTR() (I2C_VM_ASM(I2C_VM_OP_DEC_CTR, 0, 0, 0))
#define I2C_VM_ASM_LOAD_BE(addr, length, dest_reg) (I2C_VM_ASM(I2C_VM_OP_LOAD_BE, (addr), (length), (dest_reg)))
#define I2C_VM_ASM_LOAD_LE(addr, length, dest_reg) (I2C_VM_ASM(I2C_VM_OP_LOAD_LE, (addr), (length), (dest_reg)))


#endif //GEN_I2C_VM_H_

/**
 * @}
 * @}
 */
