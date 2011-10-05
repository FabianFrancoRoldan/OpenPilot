/**
******************************************************************************
* @addtogroup UAVObjects OpenPilot UAVObjects
* @{ 
* @addtogroup UAV Object Manager 
* @brief The core UAV Objects functions, most of which are wrappered by
* autogenerated defines
* @{ 
*
*
* @file       uavobjectmanager.h
* @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
* @brief      Object manager library. This library holds a collection of all objects.
*             It can be used by all modules/libraries to find an object reference.
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

#include "WProgram.h"
#include "UAVObj.h"

// Constants

// Private types
typedef void *xQueueHandle;

#define LL_FOREACH(head,el)			\
  for(el=head;el;el=el->next)

/******************************************************************************
 * doubly linked list macros (non-circular)                                   *
 *****************************************************************************/
#define DL_PREPEND(head,add)			\
  do {						\
    (add)->next = head;				\
    if (head) {					\
      (add)->prev = (head)->prev;		\
      (head)->prev = (add);			\
    } else {					\
      (add)->prev = (add);			\
    }						\
    (head) = (add);				\
  } while (0)

#define LL_APPEND(head,add)			\
  do {						\
    __typeof__(head) _tmp;			\
    (add)->next=NULL;				\
    if (head) {					\
      _tmp = head;				\
      while (_tmp->next) { _tmp = _tmp->next; }	\
      _tmp->next=(add);				\
    } else {					\
      (head)=(add);				\
    }						\
  } while (0)

#define LL_DELETE(head,del)				\
  do {							\
    __typeof__(head) _tmp;				\
    if ((head) == (del)) {				\
      (head)=(head)->next;				\
    } else {						\
      _tmp = head;					\
      while (_tmp->next && (_tmp->next != (del))) {	\
	_tmp = _tmp->next;				\
      }							\
      if (_tmp->next) {					\
	_tmp->next = ((del)->next);			\
      }							\
    }							\
  } while (0)

/**
 * List of event queues and the eventmask associated with the queue.
 */
struct ObjectEventListStruct {
  xQueueHandle queue;
  UAVObjEventCallback cb;
  int32_t eventMask;
  struct ObjectEventListStruct *next;
};
typedef struct ObjectEventListStruct ObjectEventList;

/**
 * List of object instances, holds the actual data structure and instance ID
 */
struct ObjectInstListStruct {
  void *data;
  uint16_t instId;
  struct ObjectInstListStruct *next;
};
typedef struct ObjectInstListStruct ObjectInstList;

/**
 * List of objects registered in the object manager
 */
struct ObjectListStruct {
  uint32_t id;
  /** The object ID */
  const char *name;
  /** The object name */
  int8_t isMetaobject;
  /** Set to 1 if this is a metaobject */
  int8_t isSingleInstance;
  /** Set to 1 if this object has a single instance */
  int8_t isSettings;
  /** Set to 1 if this object is a settings object */
  uint16_t numBytes;
  /** Number of data bytes contained in the object (for a single instance) */
  uint16_t numInstances;
  /** Number of instances */
  struct ObjectListStruct *linkedObj;
  /** Linked object, for regular objects this is the metaobject and for metaobjects it is the parent object */
  ObjectInstList instances;
  /** List of object instances, instance 0 always exists */
  ObjectEventList *events;
  /** Event queues registered on the object */
  struct ObjectListStruct *next;
  /** Needed by linked list library (utlist.h) */
};
typedef struct ObjectListStruct ObjectList;

// Private functions
static int32_t sendEvent(ObjectList * obj, uint16_t instId,
			 UAVObjEventType event);
static ObjectInstList *createInstance(ObjectList * obj, uint16_t instId);
static ObjectInstList *getInstance(ObjectList * obj, uint16_t instId);
static int32_t connectObj(UAVObjHandle obj, xQueueHandle queue,
			  UAVObjEventCallback cb, int32_t eventMask);
static int32_t disconnectObj(UAVObjHandle obj, xQueueHandle queue,
			     UAVObjEventCallback cb);
static bool xQueueSend(xQueueHandle h, void *msg, int32_t block_time);
static bool EventCallbackDispatch(UAVObjEvent *msg, UAVObjEventCallback cb);

// Private variables
static ObjectList *objList;
static UAVObjMetadata defMetadata;
static UAVObjStats stats;

/**
 * Initialize the object manager
 * \return 0 Success
 * \return -1 Failure
 */
int32_t UAVObjInitialize() {
  // Initialize variables
  objList = NULL;
  memset(&stats, 0, sizeof(UAVObjStats));

  // Initialize default metadata structure (metadata of metaobjects)
  defMetadata.access = ACCESS_READWRITE;
  defMetadata.gcsAccess = ACCESS_READWRITE;
  defMetadata.telemetryAcked = 1;
  defMetadata.telemetryUpdateMode = UPDATEMODE_ONCHANGE;
  defMetadata.telemetryUpdatePeriod = 0;
  defMetadata.gcsTelemetryAcked = 1;
  defMetadata.gcsTelemetryUpdateMode = UPDATEMODE_ONCHANGE;
  defMetadata.gcsTelemetryUpdatePeriod = 0;
  defMetadata.loggingUpdateMode = UPDATEMODE_ONCHANGE;
  defMetadata.loggingUpdatePeriod = 0;

  // Done
  return 0;
}

/**
 * Get the statistics counters
 * @param[out] statsOut The statistics counters will be copied there
 */
void UAVObjGetStats(UAVObjStats * statsOut) {
  memcpy(statsOut, &stats, sizeof(UAVObjStats));
}

/**
 * Clear the statistics counters
 */
void UAVObjClearStats() {
  memset(&stats, 0, sizeof(UAVObjStats));
}

/**
 * Register and new object in the object manager.
 * \param[in] id Unique object ID
 * \param[in] name Object name
 * \param[in] nameName Metaobject name
 * \param[in] isMetaobject Is this a metaobject (1:true, 0:false)
 * \param[in] isSingleInstance Is this a single instance or multi-instance object
 * \param[in] isSettings Is this a settings object
 * \param[in] numBytes Number of bytes of object data (for one instance)
 * \param[in] initCb Default field and metadata initialization function
 * \return Object handle, or NULL if failure.
 * \return
 */
UAVObjHandle
UAVObjRegister(uint32_t id, const char *name,
	       const char *metaName, int32_t isMetaobject,
	       int32_t isSingleInstance, int32_t isSettings,
	       uint32_t numBytes,
	       UAVObjInitializeCallback initCb)
{
  ObjectList *objEntry;
  ObjectInstList *instEntry;
  ObjectList *metaObj;

  // Check that the object is not already registered
  LL_FOREACH(objList, objEntry) {
    if (objEntry->id == id) {
      return NULL;
    }
  }

  // Create and append entry
  objEntry = (ObjectList *) malloc(sizeof(ObjectList));
  if (objEntry == NULL) {
    return NULL;
  }
  objEntry->id = id;
  objEntry->name = name;
  objEntry->isMetaobject = (int8_t) isMetaobject;
  objEntry->isSingleInstance = (int8_t) isSingleInstance;
  objEntry->isSettings = (int8_t) isSettings;
  objEntry->numBytes = numBytes;
  objEntry->events = NULL;
  objEntry->numInstances = 0;
  objEntry->instances.data = NULL;
  objEntry->instances.instId = 0xFFFF;
  objEntry->instances.next = NULL;
  objEntry->linkedObj = NULL;	// will be set later
  LL_APPEND(objList, objEntry);

  // Create instance zero
  instEntry = createInstance(objEntry, 0);
  if (instEntry == NULL) {
    return NULL;
  }
  // Create metaobject and update linkedObj
  if (isMetaobject) {
    objEntry->linkedObj = NULL;	// will be set later
  } else {
    // Create metaobject
    metaObj = (ObjectList *)UAVObjRegister(id + 1, metaName,
					   NULL, 1, 1, 0,
					   sizeof
					   (UAVObjMetadata),
					   NULL);
    // Link two objects
    objEntry->linkedObj = metaObj;
    metaObj->linkedObj = objEntry;
  }

  // Initialize object fields and metadata to default values
  if (initCb != NULL) {
    initCb((UAVObjHandle) objEntry, 0);
  }

  return (UAVObjHandle) objEntry;
}

/**
 * Retrieve an object from the list given its id
 * \param[in] The object ID
 * \return The object or NULL if not found.
 */
UAVObjHandle UAVObjGetByID(uint32_t id)
{
  ObjectList *objEntry;

  // Look for object
  LL_FOREACH(objList, objEntry) {
    if (objEntry->id == id) {
      // Done, object found
      return (UAVObjHandle) objEntry;
    }
  }

  return NULL;
}

/**
 * Retrieve an object from the list given its name
 * \param[in] name The name of the object
 * \return The object or NULL if not found.
 */
UAVObjHandle UAVObjGetByName(char *name) {
  ObjectList *objEntry;

  // Look for object
  LL_FOREACH(objList, objEntry) {
    if (objEntry->name != NULL
	&& strcmp(objEntry->name, name) == 0) {
      // Done, object found
      return (UAVObjHandle) objEntry;
    }
  }

  return NULL;
}

/**
 * Get the object's ID
 * \param[in] obj The object handle
 * \return The object ID
 */
uint32_t UAVObjGetID(UAVObjHandle obj) {
  return ((ObjectList *) obj)->id;
}

/**
 * Get the object's name
 * \param[in] obj The object handle
 * \return The object's name
 */
const char *UAVObjGetName(UAVObjHandle obj) {
  return ((ObjectList *) obj)->name;
}

/**
 * Get the number of bytes of the object's data (for one instance)
 * \param[in] obj The object handle
 * \return The number of bytes
 */
uint32_t UAVObjGetNumBytes(UAVObjHandle obj) {
  return ((ObjectList *) obj)->numBytes;
}

/**
 * Get the object this object is linked to. For regular objects, the linked object
 * is the metaobject. For metaobjects the linked object is the parent object.
 * This function is normally only needed by the telemetry module.
 * \param[in] obj The object handle
 * \return The object linked object handle
 */
UAVObjHandle UAVObjGetLinkedObj(UAVObjHandle obj) {
  return (UAVObjHandle) (((ObjectList *) obj)->linkedObj);
}

/**
 * Get the number of instances contained in the object.
 * \param[in] obj The object handle
 * \return The number of instances
 */
uint16_t UAVObjGetNumInstances(UAVObjHandle obj) {
  uint32_t numInstances;
  numInstances = ((ObjectList *) obj)->numInstances;
  return numInstances;
}

/**
 * Create a new instance in the object.
 * \param[in] obj The object handle
 * \return The instance ID or 0 if an error
 */
uint16_t UAVObjCreateInstance(UAVObjHandle obj,
			      UAVObjInitializeCallback initCb) {
  ObjectList *objEntry;
  ObjectInstList *instEntry;

  // Create new instance
  objEntry = (ObjectList *) obj;
  instEntry = createInstance(objEntry, objEntry->numInstances);
  if (instEntry == NULL) {
    return -1;
  }
  // Initialize instance data
  if (initCb != NULL) {
    initCb(obj, instEntry->instId);
  }
  return instEntry->instId;
}

/**
 * Does this object contains a single instance or multiple instances?
 * \param[in] obj The object handle
 * \return True (1) if this is a single instance object
 */
int32_t UAVObjIsSingleInstance(UAVObjHandle obj) {
  return ((ObjectList *) obj)->isSingleInstance;
}

/**
 * Is this a metaobject?
 * \param[in] obj The object handle
 * \return True (1) if this is metaobject
 */
int32_t UAVObjIsMetaobject(UAVObjHandle obj) {
  return ((ObjectList *) obj)->isMetaobject;
}

/**
 * Is this a settings object?
 * \param[in] obj The object handle
 * \return True (1) if this is a settings object
 */
int32_t UAVObjIsSettings(UAVObjHandle obj) {
  return ((ObjectList *) obj)->isSettings;
}

/**
 * Unpack an object from a byte array
 * \param[in] obj The object handle
 * \param[in] instId The instance ID
 * \param[in] dataIn The byte array
 * \return 0 if success or -1 if failure
 */
int32_t UAVObjUnpack(UAVObjHandle obj, uint16_t instId,
		     const uint8_t * dataIn) {
  ObjectList *objEntry;
  ObjectInstList *instEntry;

  // Cast handle to object
  objEntry = (ObjectList *) obj;

  // Get the instance
  instEntry = getInstance(objEntry, instId);

  // If the instance does not exist create it and any other instances before it
  if (instEntry == NULL) {
    instEntry = createInstance(objEntry, instId);
    if (instEntry == NULL) {
      return -1;
    }
  }
  // Set the data
  memcpy(instEntry->data, dataIn, objEntry->numBytes);

  // Fire event
  sendEvent(objEntry, instId, EV_UNPACKED);

  return 0;
}

/**
 * Pack an object to a byte array
 * \param[in] obj The object handle
 * \param[in] instId The instance ID
 * \param[out] dataOut The byte array
 * \return 0 if success or -1 if failure
 */
int32_t UAVObjPack(UAVObjHandle obj, uint16_t instId, uint8_t *dataOut) {
  ObjectList *objEntry;
  ObjectInstList *instEntry;

  // Cast handle to object
  objEntry = (ObjectList *) obj;

  // Get the instance
  instEntry = getInstance(objEntry, instId);
  if (instEntry == NULL) {
    return -1;
  }
  // Pack data
  memcpy(dataOut, instEntry->data, objEntry->numBytes);

  return 0;
}

/**
 * Set the object data
 * \param[in] obj The object handle
 * \param[in] dataIn The object's data structure
 * \return 0 if success or -1 if failure
 */
int32_t UAVObjSetData(UAVObjHandle obj, const void *dataIn)
{
  return UAVObjSetInstanceData(obj, 0, dataIn);
}

/**
 * Set the object data
 * \param[in] obj The object handle
 * \param[in] dataIn The object's data structure
 * \return 0 if success or -1 if failure
 */
int32_t UAVObjSetDataField(UAVObjHandle obj, const void* dataIn,
			   uint32_t offset, uint32_t size) {
  return UAVObjSetInstanceDataField(obj, 0, dataIn, offset, size);
}

/**
 * Get the object data
 * \param[in] obj The object handle
 * \param[out] dataOut The object's data structure
 * \return 0 if success or -1 if failure
 */
int32_t UAVObjGetData(UAVObjHandle obj, void *dataOut) {
  return UAVObjGetInstanceData(obj, 0, dataOut);
}

/**
 * Get the object data
 * \param[in] obj The object handle
 * \param[out] dataOut The object's data structure
 * \return 0 if success or -1 if failure
 */
int32_t UAVObjGetDataField(UAVObjHandle obj, void* dataOut, uint32_t offset,
			   uint32_t size) {
  return UAVObjGetInstanceDataField(obj, 0, dataOut, offset, size);
}

/**
 * Set the data of a specific object instance
 * \param[in] obj The object handle
 * \param[in] instId The object instance ID
 * \param[in] dataIn The object's data structure
 * \return 0 if success or -1 if failure
 */
int32_t UAVObjSetInstanceData(UAVObjHandle obj, uint16_t instId,
			      const void *dataIn) {
  ObjectList *objEntry;
  ObjectInstList *instEntry;
  UAVObjMetadata *mdata;

  // Cast to object info
  objEntry = (ObjectList *) obj;

  // Check access level
  if (!objEntry->isMetaobject) {
    mdata =
      (UAVObjMetadata *)(objEntry->linkedObj->instances.data);
    if (mdata->access == ACCESS_READONLY) {
      return -1;
    }
  }
  // Get instance information
  instEntry = getInstance(objEntry, instId);
  if (instEntry == NULL) {
    return -1;
  }
  // Set data
  memcpy(instEntry->data, dataIn, objEntry->numBytes);

  // Fire event
  sendEvent(objEntry, instId, EV_UPDATED);

  return 0;
}

/**
 * Set the data of a specific object instance
 * \param[in] obj The object handle
 * \param[in] instId The object instance ID
 * \param[in] dataIn The object's data structure
 * \return 0 if success or -1 if failure
 */
int32_t UAVObjSetInstanceDataField(UAVObjHandle obj, uint16_t instId,
				   const void* dataIn, uint32_t offset,
				   uint32_t size) {
  ObjectList* objEntry;
  ObjectInstList* instEntry;
  UAVObjMetadata* mdata;

  // Cast to object info
  objEntry = (ObjectList*)obj;

  // Check access level
  if ( !objEntry->isMetaobject ) {
    mdata = (UAVObjMetadata*)(objEntry->linkedObj->instances.data);
    if ( mdata->access == ACCESS_READONLY ) {
      return -1;
    }
  }

  // Get instance information
  instEntry = getInstance(objEntry, instId);
  if ( instEntry == NULL )
    return -1;

  // return if we set too much of what we have
  if ( (size + offset) > objEntry->numBytes)
    return -1;

  // Set data
  memcpy((char*)instEntry->data + offset, dataIn, size);

  // Fire event
  sendEvent(objEntry, instId, EV_UPDATED);

  return 0;
}

/**
 * Get the data of a specific object instance
 * \param[in] obj The object handle
 * \param[in] instId The object instance ID
 * \param[out] dataOut The object's data structure
 * \return 0 if success or -1 if failure
 */
int32_t UAVObjGetInstanceData(UAVObjHandle obj, uint16_t instId,
			      void *dataOut) {
  ObjectList *objEntry;
  ObjectInstList *instEntry;

  // Cast to object info
  objEntry = (ObjectList *) obj;

  // Get instance information
  instEntry = getInstance(objEntry, instId);
  if (instEntry == NULL) {
    return -1;
  }
  // Set data
  memcpy(dataOut, instEntry->data, objEntry->numBytes);

  return 0;
}

/**
 * Get the data of a specific object instance
 * \param[in] obj The object handle
 * \param[in] instId The object instance ID
 * \param[out] dataOut The object's data structure
 * \return 0 if success or -1 if failure
 */
int32_t UAVObjGetInstanceDataField(UAVObjHandle obj, uint16_t instId,
				   void* dataOut, uint32_t offset,
				   uint32_t size) {
  ObjectList* objEntry;
  ObjectInstList* instEntry;

  // Cast to object info
  objEntry = (ObjectList*)obj;

  // Get instance information
  instEntry = getInstance(objEntry, instId);
  if ( instEntry == NULL )
    return -1;

  // return if we request too much of what we can give
  if ( (size + offset) > objEntry->numBytes) 
    return -1;
	
  // Set data
  memcpy(dataOut, (char*)instEntry->data + offset, size);

  return 0;
}

/**
 * Set the object metadata
 * \param[in] obj The object handle
 * \param[in] dataIn The object's metadata structure
 * \return 0 if success or -1 if failure
 */
int32_t UAVObjSetMetadata(UAVObjHandle obj, const UAVObjMetadata * dataIn) {
  ObjectList *objEntry;

  // Set metadata (metadata of metaobjects can not be modified)
  objEntry = (ObjectList *) obj;
  if (!objEntry->isMetaobject) {
    UAVObjSetData((UAVObjHandle) objEntry->linkedObj, dataIn);
  } else {
    return -1;
  }

  return 0;
}

/**
 * Get the object metadata
 * \param[in] obj The object handle
 * \param[out] dataOut The object's metadata structure
 * \return 0 if success or -1 if failure
 */
int32_t UAVObjGetMetadata(UAVObjHandle obj, UAVObjMetadata * dataOut) {
  ObjectList *objEntry;

  // Get metadata
  objEntry = (ObjectList *) obj;
  if (objEntry->isMetaobject) {
    memcpy(dataOut, &defMetadata, sizeof(UAVObjMetadata));
  } else {
    UAVObjGetData((UAVObjHandle) objEntry->linkedObj, dataOut);
  }

  return 0;
}

/**
 * Check if an object is read only
 * \param[in] obj The object handle
 * \return 
 *   \arg 0 if not read only 
 *   \arg 1 if read only
 *   \arg -1 if unable to get meta data
 */
int8_t UAVObjReadOnly(UAVObjHandle obj) {
  ObjectList *objEntry;
  UAVObjMetadata *mdata;

  // Cast to object info
  objEntry = (ObjectList *) obj;

  // Check access level
  if (!objEntry->isMetaobject) {
    mdata =
      (UAVObjMetadata *) (objEntry->linkedObj->instances.data);
    return mdata->access == ACCESS_READONLY;
  }
  return -1;
}

/**
 * Connect an event queue to the object, if the queue is already connected then the event mask is only updated.
 * All events matching the event mask will be pushed to the event queue.
 * \param[in] obj The object handle
 * \param[in] queue The event queue
 * \param[in] eventMask The event mask, if EV_MASK_ALL then all events are enabled (e.g. EV_UPDATED | EV_UPDATED_MANUAL)
 * \return 0 if success or -1 if failure
 */
int32_t UAVObjConnectQueue(UAVObjHandle obj, xQueueHandle queue,
			   int32_t eventMask) {
  int32_t res;
  res = connectObj(obj, queue, 0, eventMask);
  return res;
}

/**
 * Disconnect an event queue from the object.
 * \param[in] obj The object handle
 * \param[in] queue The event queue
 * \return 0 if success or -1 if failure
 */
int32_t UAVObjDisconnectQueue(UAVObjHandle obj, xQueueHandle queue)
{
  int32_t res;
  res = disconnectObj(obj, queue, 0);
  return res;
}

/**
 * Connect an event callback to the object, if the callback is already connected then the event mask is only updated.
 * The supplied callback will be invoked on all events matching the event mask.
 * \param[in] obj The object handle
 * \param[in] cb The event callback
 * \param[in] eventMask The event mask, if EV_MASK_ALL then all events are enabled (e.g. EV_UPDATED | EV_UPDATED_MANUAL)
 * \return 0 if success or -1 if failure
 */
int32_t UAVObjConnectCallback(UAVObjHandle obj, UAVObjEventCallback cb,
			      int32_t eventMask) {
  int32_t res;
  res = connectObj(obj, 0, cb, eventMask);
  return res;
}

/**
 * Disconnect an event callback from the object.
 * \param[in] obj The object handle
 * \param[in] cb The event callback
 * \return 0 if success or -1 if failure
 */
int32_t UAVObjDisconnectCallback(UAVObjHandle obj, UAVObjEventCallback cb) {
  int32_t res;
  res = disconnectObj(obj, 0, cb);
  return res;
}

/**
 * Request an update of the object's data from the GCS. The call will not wait for the response, a EV_UPDATED event
 * will be generated as soon as the object is updated.
 * \param[in] obj The object handle
 */
void UAVObjRequestUpdate(UAVObjHandle obj) {
  UAVObjRequestInstanceUpdate(obj, UAVOBJ_ALL_INSTANCES);
}

/**
 * Request an update of the object's data from the GCS. The call will not wait for the response, a EV_UPDATED event
 * will be generated as soon as the object is updated.
 * \param[in] obj The object handle
 * \param[in] instId Object instance ID to update
 */
void UAVObjRequestInstanceUpdate(UAVObjHandle obj, uint16_t instId) {
  sendEvent((ObjectList *) obj, instId, EV_UPDATE_REQ);
}

/**
 * Send the object's data to the GCS (triggers a EV_UPDATED_MANUAL event on this object).
 * \param[in] obj The object handle
 */
void UAVObjUpdated(UAVObjHandle obj) {
  UAVObjInstanceUpdated(obj, UAVOBJ_ALL_INSTANCES);
}

/**
 * Send the object's data to the GCS (triggers a EV_UPDATED_MANUAL event on this object).
 * \param[in] obj The object handle
 * \param[in] instId The object instance ID
 */
void UAVObjInstanceUpdated(UAVObjHandle obj, uint16_t instId) {
  sendEvent((ObjectList *) obj, instId, EV_UPDATED_MANUAL);
}

/**
 * Iterate through all objects in the list.
 * \param iterator This function will be called once for each object,
 * the object will be passed as a parameter
 */
void UAVObjIterate(void (*iterator) (UAVObjHandle obj)) {
  ObjectList *objEntry;

  // Iterate through the list and invoke iterator for each object
  LL_FOREACH(objList, objEntry) {
    (*iterator) ((UAVObjHandle) objEntry);
  }
}

/**
 * Send an event to all event queues registered on the object.
 */
static int32_t sendEvent(ObjectList * obj, uint16_t instId,
			 UAVObjEventType event) {
  ObjectEventList *eventEntry;
  UAVObjEvent msg;

  // Setup event
  msg.obj = (UAVObjHandle) obj;
  msg.event = event;
  msg.instId = instId;

  // Go through each object and push the event message in the queue
  // (if event is activated for the queue)
  LL_FOREACH(obj->events, eventEntry) {
    if (eventEntry->eventMask == 0
	|| (eventEntry->eventMask & event) != 0) {
      // Send to queue if a valid queue is registered
      if (eventEntry->queue != 0) {
	// will not block
	if (xQueueSend(eventEntry->queue, &msg, 0) != true)
	  ++stats.eventErrors;
      }
      // Invoke callback (from event task) if a valid one is registered
      if (eventEntry->cb != 0) {
	// invoke callback from the event task, will not block
	if (EventCallbackDispatch(&msg, eventEntry->cb) != true)
	  ++stats.eventErrors;
      }
    }
  }

  // Done
  return 0;
}

/**
 * Create a new object instance, return the instance info or NULL if failure.
 */
static ObjectInstList *createInstance(ObjectList * obj, uint16_t instId) {
  ObjectInstList *instEntry;
  int32_t n;

  // For single instance objects, only instance zero is allowed
  if (obj->isSingleInstance && instId != 0) {
    return NULL;
  }
  // Make sure that the instance ID is within limits
  if (instId >= UAVOBJ_MAX_INSTANCES) {
    return NULL;
  }
  // Check if the instance already exists
  if (getInstance(obj, instId) != NULL) {
    return NULL;
  }
  // Create any missing instances (all instance IDs must be sequential)
  for (n = obj->numInstances; n < instId; ++n) {
    if (createInstance(obj, n) == NULL) {
      return NULL;
    }
  }

  /* Instance 0 ObjectInstList allocated with ObjectList element */
  if (instId == 0) {
    instEntry = &obj->instances;
    instEntry->data = malloc(obj->numBytes);
    if (instEntry->data == NULL)
      return NULL;
    memset(instEntry->data, 0, obj->numBytes);
    instEntry->instId = instId;
  } else {
    // Create the actual instance
    instEntry =
      (ObjectInstList *)
      malloc(sizeof(ObjectInstList));
    if (instEntry == NULL)
      return NULL;
    instEntry->data = malloc(obj->numBytes);
    if (instEntry->data == NULL)
      return NULL;
    memset(instEntry->data, 0, obj->numBytes);
    instEntry->instId = instId;
    LL_APPEND(obj->instances.next, instEntry);
  }
  ++obj->numInstances;

  // Fire event
  UAVObjInstanceUpdated((UAVObjHandle) obj, instId);

  // Done
  return instEntry;
}

/**
 * Get the instance information or NULL if the instance does not exist
 */
static ObjectInstList *getInstance(ObjectList * obj, uint16_t instId)
{
  ObjectInstList *instEntry;

  // Look for specified instance ID
  LL_FOREACH(&(obj->instances), instEntry) {
    if (instEntry->instId == instId) {
      return instEntry;
    }
  }
  // If this point is reached then instance id was not found
  return NULL;
}

/**
 * Connect an event queue to the object, if the queue is already connected then the event mask is only updated.
 * \param[in] obj The object handle
 * \param[in] queue The event queue
 * \param[in] cb The event callback
 * \param[in] eventMask The event mask, if EV_MASK_ALL then all events are enabled (e.g. EV_UPDATED | EV_UPDATED_MANUAL)
 * \return 0 if success or -1 if failure
 */
static int32_t connectObj(UAVObjHandle obj, xQueueHandle queue,
			  UAVObjEventCallback cb, int32_t eventMask)
{
  ObjectEventList *eventEntry;
  ObjectList *objEntry;

  // Check that the queue is not already connected,
  // if it is simply update event mask
  objEntry = (ObjectList *) obj;
  LL_FOREACH(objEntry->events, eventEntry) {
    if (eventEntry->queue == queue && eventEntry->cb == cb) {
      // Already connected, update event mask and return
      eventEntry->eventMask = eventMask;
      return 0;
    }
  }

  // Add queue to list
  eventEntry =
    (ObjectEventList *) malloc(sizeof(ObjectEventList));
  if (eventEntry == NULL) {
    return -1;
  }
  eventEntry->queue = queue;
  eventEntry->cb = cb;
  eventEntry->eventMask = eventMask;
  LL_APPEND(objEntry->events, eventEntry);

  // Done
  return 0;
}

/**
 * Disconnect an event queue from the object
 * \param[in] obj The object handle
 * \param[in] queue The event queue
 * \param[in] cb The event callback
 * \return 0 if success or -1 if failure
 */
static int32_t disconnectObj(UAVObjHandle obj, xQueueHandle queue,
			     UAVObjEventCallback cb)
{
  ObjectEventList *eventEntry;
  ObjectList *objEntry;

  // Find queue and remove it
  objEntry = (ObjectList *) obj;
  LL_FOREACH(objEntry->events, eventEntry) {
    if ((eventEntry->queue == queue
	 && eventEntry->cb == cb)) {
      LL_DELETE(objEntry->events, eventEntry);
      free(eventEntry);
      return 0;
    }
  }

  // If this point is reached the queue was not found
  return -1;
}

static bool xQueueSend(xQueueHandle h, void *msg, int32_t block_time) {
}

static bool EventCallbackDispatch(UAVObjEvent *msg, UAVObjEventCallback cb) {
  (*cb)(msg);
  return true;
}
