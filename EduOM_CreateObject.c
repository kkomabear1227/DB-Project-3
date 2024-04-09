/******************************************************************************/
/*                                                                            */
/*    ODYSSEUS/EduCOSMOS Educational-Purpose Object Storage System            */
/*                                                                            */
/*    Developed by Professor Kyu-Young Whang et al.                           */
/*                                                                            */
/*    Database and Multimedia Laboratory                                      */
/*                                                                            */
/*    Computer Science Department and                                         */
/*    Advanced Information Technology Research Center (AITrc)                 */
/*    Korea Advanced Institute of Science and Technology (KAIST)              */
/*                                                                            */
/*    e-mail: kywhang@cs.kaist.ac.kr                                          */
/*    phone: +82-42-350-7722                                                  */
/*    fax: +82-42-350-8380                                                    */
/*                                                                            */
/*    Copyright (c) 1995-2013 by Kyu-Young Whang                              */
/*                                                                            */
/*    All rights reserved. No part of this software may be reproduced,        */
/*    stored in a retrieval system, or transmitted, in any form or by any     */
/*    means, electronic, mechanical, photocopying, recording, or otherwise,   */
/*    without prior written permission of the copyright owner.                */
/*                                                                            */
/******************************************************************************/
/*
 * Module : EduOM_CreateObject.c
 * 
 * Description :
 *  EduOM_CreateObject() creates a new object near the specified object.
 *
 * Exports:
 *  Four EduOM_CreateObject(ObjectID*, ObjectID*, ObjectHdr*, Four, char*, ObjectID*)
 */

#include <string.h>
#include "EduOM_common.h"
#include "RDsM.h"		/* for the raw disk manager call */
#include "BfM.h"		/* for the buffer manager call */
#include "EduOM_Internal.h"

/*@================================
 * EduOM_CreateObject()
 *================================*/
/*
 * Function: Four EduOM_CreateObject(ObjectID*, ObjectID*, ObjectHdr*, Four, char*, ObjectID*)
 * 
 * Description :
 * (Following description is for original ODYSSEUS/COSMOS OM.
 *  For ODYSSEUS/EduCOSMOS EduOM, refer to the EduOM project manual.)
 *
 * (1) What to do?
 * EduOM_CreateObject() creates a new object near the specified object.
 * If there is no room in the page holding the specified object,
 * it trys to insert into the page in the available space list. If fail, then
 * the new object will be put into the newly allocated page.
 *
 * (2) How to do?
 *	a. Read in the near slotted page
 *	b. See the object header
 *	c. IF large object THEN
 *	       call the large object manager's lom_ReadObject()
 *	   ELSE 
 *		   IF moved object THEN 
 *				call this function recursively
 *		   ELSE 
 *				copy the data into the buffer
 *		   ENDIF
 *	   ENDIF
 *	d. Free the buffer page
 *	e. Return
 *
 * Returns:
 *  error code
 *    eBADCATALOGOBJECT_OM
 *    eBADLENGTH_OM
 *    eBADUSERBUF_OM
 *    some error codes from the lower level
 *
 * Side Effects :
 *  0) A new object is created.
 *  1) parameter oid
 *     'oid' is set to the ObjectID of the newly created object.
 */
Four EduOM_CreateObject(
    ObjectID  *catObjForFile,	/* IN file in which object is to be placed */
    ObjectID  *nearObj,		/* IN create the new object near this object */
    ObjectHdr *objHdr,		/* IN from which tag is to be set */
    Four      length,		/* IN amount of data */
    char      *data,		/* IN the initial data for the object */
    ObjectID  *oid)		/* OUT the object's ObjectID */
{
    Four        e;		/* error number */
    ObjectHdr   objectHdr;	/* ObjectHdr with tag set from parameter */


    /*@ parameter checking */
    
    if (catObjForFile == NULL) ERR(eBADCATALOGOBJECT_OM);
    
    if (length < 0) ERR(eBADLENGTH_OM);

    if (length > 0 && data == NULL) return(eBADUSERBUF_OM);

	/* Error check whether using not supported functionality by EduOM */
	if(ALIGNED_LENGTH(length) > LRGOBJ_THRESHOLD) ERR(eNOTSUPPORTED_EDUOM);
    
    /* 
    parameter로 준 objHdr를 활용해 초기화를 진행하고,
    eduom_CreateObject() 내부함수를 활용해 page에 object를 삽입한다.
    */

    // 1. Header initialization
    objectHdr.properties = 0x0;
    objectHdr.length = 0;
    if (objHdr != NULL)
        objectHdr.tag = objHdr;
    else 
        objectHdr.tag = 0;

    // 2. Call eduom_CreateObject() to insert an object into the page
    // and return the ID of the object inserted
    return eduom_CreateObject(catObjForFile, nearObj, &objectHdr, length, data, oid);
    
    return(eNOERROR);
}

/*@================================
 * eduom_CreateObject()
 *================================*/
/*
 * Function: Four eduom_CreateObject(ObjectID*, ObjectID*, ObjectHdr*, Four, char*, ObjectID*)
 *
 * Description :
 * (Following description is for original ODYSSEUS/COSMOS OM.
 *  For ODYSSEUS/EduCOSMOS EduOM, refer to the EduOM project manual.)
 *
 *  eduom_CreateObject() creates a new object near the specified object; the near
 *  page is the page holding the near object.
 *  If there is no room in the near page and the near object 'nearObj' is not
 *  NULL, a new page is allocated for object creation (In this case, the newly
 *  allocated page is inserted after the near page in the list of pages
 *  consiting in the file).
 *  If there is no room in the near page and the near object 'nearObj' is NULL,
 *  it trys to create a new object in the page in the available space list. If
 *  fail, then the new object will be put into the newly allocated page(In this
 *  case, the newly allocated page is appended at the tail of the list of pages
 *  cosisting in the file).
 *
 * Returns:
 *  error Code
 *    eBADCATALOGOBJECT_OM
 *    eBADOBJECTID_OM
 *    some errors caused by fuction calls
 */
Four eduom_CreateObject(
                        ObjectID	*catObjForFile,	/* IN file in which object is to be placed */
                        ObjectID 	*nearObj,	/* IN create the new object near this object */
                        ObjectHdr	*objHdr,	/* IN from which tag & properties are set */
                        Four	length,		/* IN amount of data */
                        char	*data,		/* IN the initial data for the object */
                        ObjectID	*oid)		/* OUT the object's ObjectID */
{
    Four        e;		/* error number */
    Four	neededSpace;	/* space needed to put new object [+ header] */
    SlottedPage *apage;		/* pointer to the slotted page buffer */
    Four        alignedLen;	/* aligned length of initial data */
    Boolean     needToAllocPage;/* Is there a need to alloc a new page? */
    PageID      pid;            /* PageID in which new object to be inserted */
    PageID      nearPid;
    Four        firstExt;	/* first Extent No of the file */
    Object      *obj;		/* point to the newly created object */
    Two         i;		/* index variable */
    sm_CatOverlayForData *catEntry; /* pointer to data file catalog information */
    SlottedPage *catPage;	/* podinter to buffer containing the catalog */
    FileID      fid;		/* ID of file where the new object is placed */
    Two         eff;		/* extent fill factor of file */
    Boolean     isTmp;
    PhysicalFileID pFid;
    
    
    /*@ parameter checking */
    
    if (catObjForFile == NULL) ERR(eBADCATALOGOBJECT_OM);
    
    if (objHdr == NULL) ERR(eBADOBJECTID_OM);
    
    /* Error check whether using not supported functionality by EduOM */
    if(ALIGNED_LENGTH(length) > LRGOBJ_THRESHOLD) ERR(eNOTSUPPORTED_EDUOM);
    
    /* 
    File을 구성하는 page 중 parameter로 들어온 object와 같거나 인접한 page에 삽입하고, object id를 반환
    */

    // 0. 사전에 setting 되어야할 값
    // catPage - manual 49
    e = BfM_GetTrain((TrainID*)catObjForFile, (char**)&catPage, PAGE_BUF);
    if (e < 0) ERR(e);
    // -> catEntry까지 설정
    GET_PTR_TO_CATENTRY_FOR_DATA(catObjForFile, catPage, catEntry);

    fid = catEntry->fid;
    MAKE_PAGEID(pFid, catEntry->fid.volNo, catEntry->firstPage);

    // pFid - manual p44
    e = RDsM_PageIdToExtNo((PageID *)&pFid, &firstExt);
    if (e < 0) ERR(e);

    // 1. Free space를 계산
    // sizeof(ObjectHdr) + size of the aligned object data area + sizeof(SlottedPageSlot)
    alignedLen = ALIGNED_LENGTH(length);
    neededSpace = sizeof(ObjectHdr) + alignedLen + sizeof(SlottedPageSlot);

    // 2. page 선정
    // 2-1. nearObj가 NULL이 아닌 경우
    if (nearObj != NULL) {
        MAKE_PAGEID(nearPid, nearObj->volNo, nearObj->pageNo);
        BfM_GetTrain((TrainID *)&nearPid, (char **)&apage, PAGE_BUF);

        // a. nearObj가 저장된 page에 충분한 여유 공간이 있는 경우
        // available space list에서 삭제 후 object를 삽입
        if (SP_FREE(apage) >= neededSpace) {
            pid = nearPid;
            om_RemoveFromAvailSpaceList(catObjForFile, &pid, apage);

            // compaction이 필요할 경우 compaction을 수행
            if (SP_CFREE(apage) < neededSpace) {
                EduOM_CompactPage(apage, nearObj->slotNo);
            }
        }
        // b. nearObj가 저장된 page에 충분한 여유 공간이 없는 경우
        // nearObj가 저장된 page 옆에 새 page를 할당받아 삽입한다.
        else {
            BfM_FreeTrain((TrainID *)&nearPid, PAGE_BUF);
            // page 47
            // 새로운 page 하나를 할당하고, ID를 pid에 저장한다.
            e = RDsM_AllocTrains(catEntry->fid.volNo, firstExt, &nearPid, catEntry->eff, 1, PAGESIZE2, &pid);
            if (e < 0) ERR(e);
        
            // page 51
            // disk에 새로 할당된 page를 fix하고, 포인터를 apage에 담아 반환한다.
            e = BfM_GetNewTrain((TrainID *)&pid, (char **)&apage, PAGE_BUF);
            if (e < 0) ERR(e);

            // header 초기화
            SET_PAGE_TYPE(apage, SLOTTED_PAGE_TYPE);
            apage->header.fid = catEntry->fid;
            apage->header.free = 0;
            apage->header.unused = 0;

            //nearObj가 저장된 page의 다음 page로 insert한다.
            om_FileMapAddPage(catObjForFile, &nearPid, &pid);
        }
    }
    else { // nearObj == NULL
        PageNo pageCandidate = NIL;
        // available한 space들 중 가장 size가 딱 맞는 page를 찾아낸다.
        if (neededSpace <= SP_50SIZE) pageCandidate = catEntry->availSpaceList50;
        else if (neededSpace <= SP_40SIZE) pageCandidate = catEntry->availSpaceList40;
        else if (neededSpace <= SP_30SIZE) pageCandidate = catEntry->availSpaceList30;
        else if (neededSpace <= SP_20SIZE) pageCandidate = catEntry->availSpaceList20;
        else pageCandidate = catEntry->availSpaceList10;

        // a. object를 삽입할 공간이 있는 free page를 찾았다.
        if (pageCandidate != NIL) {
            // available space list에서 해당 entry를 삭제한다.
            MAKE_PAGEID(pid, pFid.volNo, pageCandidate);
            BfM_GetNewTrain((TrainID *)&pid, (char **)&apage, PAGE_BUF);
            om_RemoveFromAvailSpaceList(catObjForFile, &pid, apage);

            // Compaction if neccessary
            if (SP_CFREE(apage) < neededSpace) {
                EduOM_CompactPage(apage, nearObj->slotNo);
            }
        } 
        // b. avail list에 object를 삽입할 수 있는 page를 찾지 못했다.
        // 이 경우, file의 last page를 확인한다.
        else {
            MAKE_PAGEID(pid, pFid.volNo, catEntry->lastPage);
            BfM_GetTrain((TrainID *)&pid, (char **)&apage, PAGE_BUF);

            // b-1. last page에 object를 삽입할 수 있다.
            if (SP_FREE(apage) >= neededSpace) {
                om_RemoveFromAvailSpaceList(catObjForFile, &pid, apage);

                // compaction이 필요할 경우 compaction을 수행
                if (SP_CFREE(apage) < neededSpace) {
                    EduOM_CompactPage(apage, nearObj->slotNo);
                }
            }
            // b-2. last page에 object를 삽입할 수 없어, 새 page를 할당 받아야함.
            else {
                BfM_FreeTrain((TrainID *)&pid, PAGE_BUF); 
                MAKE_PAGEID(nearPid, pFid.volNo, catEntry->lastPage);
                // 새로운 page 하나를 할당하고, ID를 pid에 저장한다.
                e = RDsM_AllocTrains(catEntry->fid.volNo, firstExt, &nearPid, catEntry->eff, 1, PAGESIZE2, &pid);
                if (e < 0) ERR(e);
        
                // disk에 새로 할당된 page를 fix하고, 포인터를 apage에 담아 반환한다.
                e = BfM_GetNewTrain((TrainID *)&pid, (char **)&apage, PAGE_BUF);
                if (e < 0) ERR(e);

                // header 초기화
                SET_PAGE_TYPE(apage, SLOTTED_PAGE_TYPE);
                apage->header.fid = catEntry->fid;
                apage->header.free = 0;
                apage->header.unused = 0;

                //nearObj가 저장된 page의 다음 page로 insert한다.
                om_FileMapAddPage(catObjForFile, &nearPid, &pid);
            }
        }
    }

    // 3. 선정된 page에 object를 삽입
    // 3-1. object header update
    obj = (Object *)&(apage->data[apage->header.free]);
    obj->header.properties = objHdr->properties;
    obj->header.length = length;
    obj->header.tag = objHdr->tag;

    // 3-2. free area에 object를 복사
    memcpy(obj->data, data, length);

    // 3-3. Slot array에서 slot을 할당 받아 object 정보를 저장
    for (i = 0; i < apage->header.nSlots; i++) {
        if (apage->slot[-i].offset == EMPTYSLOT) break;
    }

    SlottedPageSlot* insertedSlot = &(apage->slot[-i]);
    insertedSlot->offset = apage->header.free;
    om_GetUnique(&pid, &(insertedSlot->unique));

    // 3-4. page header 갱신
    // free space 갱신
    if (i == apage->header.nSlots) apage->header.nSlots++;
    apage->header.free += sizeof(ObjectHdr) + alignedLen;

    // page를 알맞는 available space list에 삽입함
    om_PutInAvailSpaceList(catObjForFile, &pid, apage);
    MAKE_OBJECTID(*oid, pid.volNo, pid.pageNo, i, insertedSlot->unique);

    // Free resource
    e = BfM_SetDirty(&pid, PAGE_BUF);
    if (e < 0) ERRB1(e, &pid, PAGE_BUF);

    BfM_FreeTrain(&pid, PAGE_BUF);
    e = BfM_FreeTrain((TrainID *)catObjForFile, PAGE_BUF);
    if (e < 0) ERR(e);
    
    return(eNOERROR);
    
} /* eduom_CreateObject() */
