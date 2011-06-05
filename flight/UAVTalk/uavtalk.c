/**
 ******************************************************************************
 * @addtogroup OpenPilotSystem OpenPilot System
 * @{
 * @addtogroup OpenPilotLibraries OpenPilot System Libraries
 * @{
 *
 * @file       uavtalk.c
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
 * @brief      UAVTalk library, implements to telemetry protocol. See the wiki for more details.
 * 	       This library should not be called directly by the application, it is only used by the
 * 	       Telemetry module.
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

#include "openpilot.h"

// Private constants
#define SYNC_VAL			0x3C
#define TYPE_MASK			0xF8
#define TYPE_VER			0x20
#define TYPE_OBJ			(TYPE_VER | 0x00)
#define TYPE_OBJ_REQ		(TYPE_VER | 0x01)
#define TYPE_OBJ_ACK		(TYPE_VER | 0x02)
#define TYPE_ACK			(TYPE_VER | 0x03)
#define TYPE_NACK			(TYPE_VER | 0x04)

#define MIN_HEADER_LENGTH	8	// sync(1), type (1), size (2), object ID (4)
#define MAX_HEADER_LENGTH	10	// sync(1), type (1), size (2), object ID (4), instance ID (2, not used in single objects)

#define CHECKSUM_LENGTH		1

#define MAX_PAYLOAD_LENGTH	256

#define MAX_PACKET_LENGTH	(MAX_HEADER_LENGTH + MAX_PAYLOAD_LENGTH + CHECKSUM_LENGTH)


// Private types
typedef enum {STATE_SYNC, STATE_TYPE, STATE_SIZE, STATE_OBJID, STATE_INSTID, STATE_DATA, STATE_CS} RxState;

// Private variables
static UAVTalkOutputStream outStream;
static xSemaphoreHandle lock;
static xSemaphoreHandle transLock;
static xSemaphoreHandle respSema;
static UAVObjHandle respObj;
static uint16_t respInstId;
static uint8_t rxBuffer[MAX_PACKET_LENGTH];
static uint8_t txBuffer[MAX_PACKET_LENGTH];
static UAVTalkStats stats;

// Private functions
static int32_t objectTransaction(UAVObjHandle objectId, uint16_t instId, uint8_t type, int32_t timeout);
static int32_t sendObject(UAVObjHandle obj, uint16_t instId, uint8_t type);
static int32_t sendSingleObject(UAVObjHandle obj, uint16_t instId, uint8_t type);
static int32_t sendNack(uint32_t objId);
static int32_t receiveObject(uint8_t type, uint32_t objId, uint16_t instId, uint8_t* data, int32_t length);
static void updateAck(UAVObjHandle obj, uint16_t instId);

/**
 * Initialize the UAVTalk library
 * \param[in] outputStream Function pointer that is called to send a data buffer
 * \return 0 Success
 * \return -1 Failure
 */
int32_t UAVTalkInitialize(UAVTalkOutputStream outputStream)
{
	outStream = outputStream;
	lock = xSemaphoreCreateRecursiveMutex();
	transLock = xSemaphoreCreateRecursiveMutex();
	vSemaphoreCreateBinary(respSema);
	xSemaphoreTake(respSema, 0); // reset to zero
	UAVTalkResetStats();
	return 0;
}

/**
 * Get communication statistics counters
 * @param[out] statsOut Statistics counters
 */
void UAVTalkGetStats(UAVTalkStats* statsOut)
{
	// Lock
	xSemaphoreTakeRecursive(lock, portMAX_DELAY);
	
	// Copy stats
	memcpy(statsOut, &stats, sizeof(UAVTalkStats));
	
	// Release lock
	xSemaphoreGiveRecursive(lock);
}

/**
 * Reset the statistics counters.
 */
void UAVTalkResetStats()
{
	// Lock
	xSemaphoreTakeRecursive(lock, portMAX_DELAY);
	
	// Clear stats
	memset(&stats, 0, sizeof(UAVTalkStats));
	
	// Release lock
	xSemaphoreGiveRecursive(lock);
}

/**
 * Request an update for the specified object, on success the object data would have been
 * updated by the GCS.
 * \param[in] obj Object to update
 * \param[in] instId The instance ID or UAVOBJ_ALL_INSTANCES for all instances.
 * \param[in] timeout Time to wait for the response, when zero it will return immediately
 * \return 0 Success
 * \return -1 Failure
 */
int32_t UAVTalkSendObjectRequest(UAVObjHandle obj, uint16_t instId, int32_t timeout)
{
	return objectTransaction(obj, instId, TYPE_OBJ_REQ, timeout);
}

/**
 * Send the specified object through the telemetry link.
 * \param[in] obj Object to send
 * \param[in] instId The instance ID or UAVOBJ_ALL_INSTANCES for all instances.
 * \param[in] acked Selects if an ack is required (1:ack required, 0: ack not required)
 * \param[in] timeoutMs Time to wait for the ack, when zero it will return immediately
 * \return 0 Success
 * \return -1 Failure
 */
int32_t UAVTalkSendObject(UAVObjHandle obj, uint16_t instId, uint8_t acked, int32_t timeoutMs)
{
	// Send object
	if (acked == 1)
	{
		return objectTransaction(obj, instId, TYPE_OBJ_ACK, timeoutMs);
	}
	else
	{
		return objectTransaction(obj, instId, TYPE_OBJ, timeoutMs);
	}
}

/**
 * Execute the requested transaction on an object.
 * \param[in] obj Object
 * \param[in] instId The instance ID of UAVOBJ_ALL_INSTANCES for all instances.
 * \param[in] type Transaction type
 * 			  TYPE_OBJ: send object,
 * 			  TYPE_OBJ_REQ: request object update
 * 			  TYPE_OBJ_ACK: send object with an ack
 * \return 0 Success
 * \return -1 Failure
 */
static int32_t objectTransaction(UAVObjHandle obj, uint16_t instId, uint8_t type, int32_t timeoutMs)
{
	int32_t respReceived;
	
	// Send object depending on if a response is needed
	if (type == TYPE_OBJ_ACK || type == TYPE_OBJ_REQ)
	{
		// Get transaction lock (will block if a transaction is pending)
		xSemaphoreTakeRecursive(transLock, portMAX_DELAY);
		// Send object
		xSemaphoreTakeRecursive(lock, portMAX_DELAY);
		respObj = obj;
		respInstId = instId;
		sendObject(obj, instId, type);
		xSemaphoreGiveRecursive(lock);
		// Wait for response (or timeout)
		respReceived = xSemaphoreTake(respSema, timeoutMs/portTICK_RATE_MS);
		// Check if a response was received
		if (respReceived == pdFALSE)
		{
			// Cancel transaction
			xSemaphoreTakeRecursive(lock, portMAX_DELAY);
			xSemaphoreTake(respSema, 0); // non blocking call to make sure the value is reset to zero (binary sema)
			respObj = 0;
			xSemaphoreGiveRecursive(lock);
			xSemaphoreGiveRecursive(transLock);
			return -1;
		}
		else
		{
			xSemaphoreGiveRecursive(transLock);
			return 0;
		}
	}
	else if (type == TYPE_OBJ)
	{
		xSemaphoreTakeRecursive(lock, portMAX_DELAY);
		sendObject(obj, instId, TYPE_OBJ);
		xSemaphoreGiveRecursive(lock);
		return 0;
	}
	else
	{
		return -1;
	}
}

/**
 * Process an byte from the telemetry stream.
 * \param[in] rxbyte Received byte
 * \return 0 Success
 * \return -1 Failure
 */
int32_t UAVTalkProcessInputStream(uint8_t rxbyte)
{
	static UAVObjHandle obj;
	static uint8_t type;
	static uint16_t packet_size;
	static uint32_t objId;
	static uint16_t instId;
	static uint32_t length;
	static uint32_t cs, csRx;
	static int32_t rxCount;
	static RxState state = STATE_SYNC;
	static uint16_t rxPacketLength = 0;
	
	++stats.rxBytes;
	
	if (rxPacketLength < 0xffff)
		rxPacketLength++;   // update packet byte count
	
	// Receive state machine
	switch (state)
	{
		case STATE_SYNC:
			if (rxbyte != SYNC_VAL)
				break;
			
			// Initialize and update the CRC
			cs = PIOS_CRC_updateByte(0, rxbyte);
			
			rxPacketLength = 1;
			
			state = STATE_TYPE;
			break;
			
		case STATE_TYPE:
			
			// update the CRC
			cs = PIOS_CRC_updateByte(cs, rxbyte);
			
			if ((rxbyte & TYPE_MASK) != TYPE_VER)
			{
				state = STATE_SYNC;
				break;
			}
			
			type = rxbyte;
			
			packet_size = 0;
			
			state = STATE_SIZE;
			rxCount = 0;
			break;
			
		case STATE_SIZE:
			
			// update the CRC
			cs = PIOS_CRC_updateByte(cs, rxbyte);
			
			if (rxCount == 0)
			{
				packet_size += rxbyte;
				rxCount++;
				break;
			}
			
			packet_size += rxbyte << 8;
			
			if (packet_size < MIN_HEADER_LENGTH || packet_size > MAX_HEADER_LENGTH + MAX_PAYLOAD_LENGTH)
			{   // incorrect packet size
				state = STATE_SYNC;
				break;
			}
			
			rxCount = 0;
			objId = 0;
			state = STATE_OBJID;
			break;
			
		case STATE_OBJID:
			
			// update the CRC
			cs = PIOS_CRC_updateByte(cs, rxbyte);
			
			objId += rxbyte << (8*(rxCount++));

			if (rxCount < 4)
				break;
			
			// Search for object, if not found reset state machine
			// except if we got a OBJ_REQ for an object which does not
			// exist, in which case we'll send a NACK

			obj = UAVObjGetByID(objId);
			if (obj == 0 && type != TYPE_OBJ_REQ)
			{
				stats.rxErrors++;
				state = STATE_SYNC;
				break;
			}
			
			// Determine data length
			if (type == TYPE_OBJ_REQ || type == TYPE_ACK || type == TYPE_NACK)
				length = 0;
			else
				length = UAVObjGetNumBytes(obj);
			
			// Check length and determine next state
			if (length >= MAX_PAYLOAD_LENGTH)
			{
				stats.rxErrors++;
				state = STATE_SYNC;
				break;
			}
			
			// Check the lengths match
			if ((rxPacketLength + length) != packet_size)
			{   // packet error - mismatched packet size
				stats.rxErrors++;
				state = STATE_SYNC;
				break;
			}
			
			instId = 0;
			if (obj == 0)
			{
				// If this is a NACK, we skip to Checksum
				state = STATE_CS;
				rxCount = 0;

			}
			// Check if this is a single instance object (i.e. if the instance ID field is coming next)
			else if (UAVObjIsSingleInstance(obj))
			{
				// If there is a payload get it, otherwise receive checksum
				if (length > 0)
					state = STATE_DATA;
				else
					state = STATE_CS;

				rxCount = 0;
			}
			else
			{
				state = STATE_INSTID;
				rxCount = 0;
			}
			
			break;
			
		case STATE_INSTID:
			
			// update the CRC
			cs = PIOS_CRC_updateByte(cs, rxbyte);
			
			instId += rxbyte << (8*(rxCount++));

			if (rxCount < 2)
				break;
			
			rxCount = 0;
			
			// If there is a payload get it, otherwise receive checksum
			if (length > 0)
				state = STATE_DATA;
			else
				state = STATE_CS;
			
			break;
			
		case STATE_DATA:
			
			// update the CRC
			cs = PIOS_CRC_updateByte(cs, rxbyte);
			
			rxBuffer[rxCount++] = rxbyte;
			if (rxCount < length)
				break;
			
			state = STATE_CS;
			rxCount = 0;
			break;
			
		case STATE_CS:
			
			// the CRC byte
			csRx = rxbyte;
			cs = (uint8_t) cs;
			if (csRx != cs)
			{   // packet error - faulty CRC
				stats.rxErrors++;
				state = STATE_SYNC;
				break;
			}
			
			if (rxPacketLength != (packet_size + 1))
			{   // packet error - mismatched packet size
				stats.rxErrors++;
				state = STATE_SYNC;
				break;
			}
			
			xSemaphoreTakeRecursive(lock, portMAX_DELAY);
			receiveObject(type, objId, instId, rxBuffer, length);
			stats.rxObjectBytes += length;
			stats.rxObjects++;
			xSemaphoreGiveRecursive(lock);
			
			state = STATE_SYNC;
			break;
			
		default:
			stats.rxErrors++;
			state = STATE_SYNC;
	}
	
	// Done
	return 0;
}

/**
 * Receive an object. This function process objects received through the telemetry stream.
 * \param[in] type Type of received message (TYPE_OBJ, TYPE_OBJ_REQ, TYPE_OBJ_ACK, TYPE_ACK, TYPE_NACK)
 * \param[in] objId ID of the object to work on
 * \param[in] instId The instance ID of UAVOBJ_ALL_INSTANCES for all instances.
 * \param[in] data Data buffer
 * \param[in] length Buffer length
 * \return 0 Success
 * \return -1 Failure
 */
static int32_t receiveObject(uint8_t type, uint32_t objId, uint16_t instId, uint8_t* data, int32_t length)
{
	static UAVObjHandle obj;
	int32_t ret = 0;

	// Get the handle to the Object. Will be zero
	// if object does not exist.
	obj = UAVObjGetByID(objId);
	
	// Process message type
	switch (type) {
		case TYPE_OBJ:
			// All instances, not allowed for OBJ messages
			if (instId != UAVOBJ_ALL_INSTANCES)
			{
				// Unpack object, if the instance does not exist it will be created!
				UAVObjUnpack(obj, instId, data);
				// Check if an ack is pending
				updateAck(obj, instId);
			}
			else
			{
				ret = -1;
			}
			break;
		case TYPE_OBJ_ACK:
			// All instances, not allowed for OBJ_ACK messages
			if (instId != UAVOBJ_ALL_INSTANCES)
			{
				// Unpack object, if the instance does not exist it will be created!
				if ( UAVObjUnpack(obj, instId, data) == 0 )
				{
					// Transmit ACK
					sendObject(obj, instId, TYPE_ACK);
				}
				else
				{
					ret = -1;
				}
			}
			else
			{
				ret = -1;
			}
			break;
		case TYPE_OBJ_REQ:
			// Send requested object if message is of type OBJ_REQ
			if (obj == 0)
				sendNack(objId);
			else
				sendObject(obj, instId, TYPE_OBJ);
			break;
		case TYPE_NACK:
			// Do nothing on flight side, let it time out.
			break;
		case TYPE_ACK:
			// All instances, not allowed for ACK messages
			if (instId != UAVOBJ_ALL_INSTANCES)
			{
				// Check if an ack is pending
				updateAck(obj, instId);
			}
			else
			{
				ret = -1;
			}
			break;
		default:
			ret = -1;
	}
	// Done
	return ret;
}

/**
 * Check if an ack is pending on an object and give response semaphore
 */
static void updateAck(UAVObjHandle obj, uint16_t instId)
{
	if (respObj == obj && (respInstId == instId || respInstId == UAVOBJ_ALL_INSTANCES))
	{
		xSemaphoreGive(respSema);
		respObj = 0;
	}
}

/**
 * Send an object through the telemetry link.
 * \param[in] obj Object handle to send
 * \param[in] instId The instance ID or UAVOBJ_ALL_INSTANCES for all instances
 * \param[in] type Transaction type
 * \return 0 Success
 * \return -1 Failure
 */
static int32_t sendObject(UAVObjHandle obj, uint16_t instId, uint8_t type)
{
	uint32_t numInst;
	uint32_t n;
	
	// If all instances are requested and this is a single instance object, force instance ID to zero
	if ( instId == UAVOBJ_ALL_INSTANCES && UAVObjIsSingleInstance(obj) )
	{
		instId = 0;
	}
	
	// Process message type
	if ( type == TYPE_OBJ || type == TYPE_OBJ_ACK )
	{
		if (instId == UAVOBJ_ALL_INSTANCES)
		{
			// Get number of instances
			numInst = UAVObjGetNumInstances(obj);
			// Send all instances
			for (n = 0; n < numInst; ++n)
			{
				sendSingleObject(obj, n, type);
			}
			return 0;
		}
		else
		{
			return sendSingleObject(obj, instId, type);
		}
	}
	else if (type == TYPE_OBJ_REQ)
	{
		return sendSingleObject(obj, instId, TYPE_OBJ_REQ);
	}
	else if (type == TYPE_ACK)
	{
		if ( instId != UAVOBJ_ALL_INSTANCES )
		{
			return sendSingleObject(obj, instId, TYPE_ACK);
		}
		else
		{
			return -1;
		}
	}
	else
	{
		return -1;
	}
}

/**
 * Send an object through the telemetry link.
 * \param[in] obj Object handle to send
 * \param[in] instId The instance ID (can NOT be UAVOBJ_ALL_INSTANCES, use sendObject() instead)
 * \param[in] type Transaction type
 * \return 0 Success
 * \return -1 Failure
 */
static int32_t sendSingleObject(UAVObjHandle obj, uint16_t instId, uint8_t type)
{
	int32_t length;
	int32_t dataOffset;
	uint32_t objId;
	
	// Setup type and object id fields
	objId = UAVObjGetID(obj);
	txBuffer[0] = SYNC_VAL;  // sync byte
	txBuffer[1] = type;
	// data length inserted here below
	txBuffer[4] = (uint8_t)(objId & 0xFF);
	txBuffer[5] = (uint8_t)((objId >> 8) & 0xFF);
	txBuffer[6] = (uint8_t)((objId >> 16) & 0xFF);
	txBuffer[7] = (uint8_t)((objId >> 24) & 0xFF);
	
	// Setup instance ID if one is required
	if (UAVObjIsSingleInstance(obj))
	{
		dataOffset = 8;
	}
	else
	{
		txBuffer[8] = (uint8_t)(instId & 0xFF);
		txBuffer[9] = (uint8_t)((instId >> 8) & 0xFF);
		dataOffset = 10;
	}
	
	// Determine data length
	if (type == TYPE_OBJ_REQ || type == TYPE_ACK)
	{
		length = 0;
	}
	else
	{
		length = UAVObjGetNumBytes(obj);
	}
	
	// Check length
	if (length >= MAX_PAYLOAD_LENGTH)
	{
		return -1;
	}
	
	// Copy data (if any)
	if (length > 0)
	{
		if ( UAVObjPack(obj, instId, &txBuffer[dataOffset]) < 0 )
		{
			return -1;
		}
	}
	
	// Store the packet length
	txBuffer[2] = (uint8_t)((dataOffset+length) & 0xFF);
	txBuffer[3] = (uint8_t)(((dataOffset+length) >> 8) & 0xFF);
	
	// Calculate checksum
	txBuffer[dataOffset+length] = (uint8_t) PIOS_CRC_updateCRC(0, txBuffer, dataOffset+length);
	
	// Send buffer
	if (outStream!=NULL) (*outStream)(txBuffer, dataOffset+length+CHECKSUM_LENGTH);
	
	// Update stats
	++stats.txObjects;
	stats.txBytes += dataOffset+length+CHECKSUM_LENGTH;
	stats.txObjectBytes += length;
	
	// Done
	return 0;
}

/**
 * Send a NACK through the telemetry link.
 * \param[in] objId Object ID to send a NACK for
 * \return 0 Success
 * \return -1 Failure
 */
static int32_t sendNack(uint32_t objId)
{
	int32_t dataOffset;

	txBuffer[0] = SYNC_VAL;  // sync byte
	txBuffer[1] = TYPE_NACK;
	// data length inserted here below
	txBuffer[4] = (uint8_t)(objId & 0xFF);
	txBuffer[5] = (uint8_t)((objId >> 8) & 0xFF);
	txBuffer[6] = (uint8_t)((objId >> 16) & 0xFF);
	txBuffer[7] = (uint8_t)((objId >> 24) & 0xFF);

	dataOffset = 8;

	// Store the packet length
	txBuffer[2] = (uint8_t)((dataOffset) & 0xFF);
	txBuffer[3] = (uint8_t)(((dataOffset) >> 8) & 0xFF);

	// Calculate checksum
	txBuffer[dataOffset] = PIOS_CRC_updateCRC(0, txBuffer, dataOffset);

	// Send buffer
	if (outStream!=NULL) (*outStream)(txBuffer, dataOffset+CHECKSUM_LENGTH);

	// Update stats
	stats.txBytes += dataOffset+CHECKSUM_LENGTH;

	// Done
	return 0;
}

/**
 * @}
 * @}
 */
