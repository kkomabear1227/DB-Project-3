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
 * Module: EduOM_PrevObject.c
 *
 * Description: 
 *  Return the previous object of the given current object.
 *
 * Exports:
 *  Four EduOM_PrevObject(ObjectID*, ObjectID*, ObjectID*, ObjectHdr*)
 */


#include "EduOM_common.h"
#include "BfM.h"
#include "EduOM_Internal.h"

/*@================================
 * EduOM_PrevObject()
 *================================*/
/*
 * Function: Four EduOM_PrevObject(ObjectID*, ObjectID*, ObjectID*, ObjectHdr*)
 *
 * Description: 
 * (Following description is for original ODYSSEUS/COSMOS OM.
 *  For ODYSSEUS/EduCOSMOS EduOM, refer to the EduOM project manual.)
 *
 *  Return the previous object of the given current object. Find the object in
 *  the same page which has the current object and  if there  is no previous
 *  object in the same page, find it from the previous page.
 *  If the current object is NULL, return the last object of the file.
 *
 * Returns:
 *  error code
 *    eBADCATALOGOBJECT_OM
 *    eBADOBJECTID_OM
 *    some errors caused by function calls
 *
 * Side effect:
 *  1) parameter prevOID
 *     prevOID is filled with the previous object's identifier
 *  2) parameter objHdr
 *     objHdr is filled with the previous object's header
 */
Four EduOM_PrevObject(
    ObjectID *catObjForFile,	/* IN informations about a data file */
    ObjectID *curOID,		/* IN a ObjectID of the current object */
    ObjectID *prevOID,		/* OUT the previous object of a current object */
    ObjectHdr*objHdr)		/* OUT the object header of previous object */
{
    Four e;			/* error */
    Two  i;			/* index */
    Four offset;		/* starting offset of object within a page */
    PageID pid;			/* a page identifier */
    PageNo pageNo;		/* a temporary var for previous page's PageNo */
    SlottedPage *apage;		/* a pointer to the data page */
    Object *obj;		/* a pointer to the Object */
    SlottedPage *catPage;	/* buffer page containing the catalog object */
    sm_CatOverlayForData *catEntry; /* overlay structure for catalog object access */



    /*@ parameter checking */
    if (catObjForFile == NULL) ERR(eBADCATALOGOBJECT_OM);
    
    if (prevOID == NULL) ERR(eBADOBJECTID_OM);

    // 0. catEntry를 불러온다.
    BfM_GetTrain((TrainID *)catObjForFile, (char **)&catPage, PAGE_BUF);
    GET_PTR_TO_CATENTRY_FOR_DATA(catObjForFile, catPage, catEntry);
    //MAKE_PAGEID(pFid, catEntry->fid.volNo, catEntry->firstPage);

    // 1. curOID가 NULL인 경우
    if (curOID == NULL) {
        // File에 들어있는 마지막 object의 ID를 반환한다.
        //pageNo = catEntry->firstPage;
        pageNo = catEntry->lastPage;

        MAKE_PAGEID(pid, catEntry->fid.volNo, pageNo);
        BfM_GetTrain((TrainID *)&pid, (char **)&apage, PAGE_BUF);

        prevOID->volNo = pid.volNo;
        prevOID->pageNo = pageNo;
        prevOID->slotNo = apage->header.nSlots - 1; // 마지막 object
        prevOID->unique = apage->slot[-prevOID->slotNo].unique;

        //BfM_FreeTrain((TrainID *)&pid, PAGE_BUF);
    }
    // 2. curOID가 NULL이 아닌 경우
    else {
        // 2-1. curOID에 대응되는 object를 찾는다.
        MAKE_PAGEID(pid, curOID->volNo, curOID->pageNo);
        BfM_GetTrain((TrainID *)&pid, (char **)&apage, PAGE_BUF);

        // 2-2. slot array에서 curOID 이전에 있는 object를 찾아 ID를 반환한다.
        //      만약 curOID object가 현재 page의 첫 object라면 이전 page의 마지막 object를 찾아 반환한다.
        //      만약 이전 page가 EMPTY라면 EOS를 반환한다.
        //      만약 curOID object가 첫 page의 첫 object라면 EOS를 반환한다.

        // Case 1. curOID가 해당 page의 첫 object가 아님
        if (curOID->slotNo != 0) {
            prevOID->volNo = curOID->volNo;
            prevOID->pageNo = curOID->pageNo;
            prevOID->slotNo = curOID->slotNo - 1;
            prevOID->unique = apage->slot[-prevOID->slotNo].unique;
        }
        // Case 2. curOID가 해당 page의 첫 object임
        else {
            // Case 2-1. curOID가 마지막 page의 마지막 object 였음
            //if (apage->header.pid.pageNo == catEntry->lastPage) continue;

            // Case 2-2. curOID가 prevPage가 있는 page의 마지막 object 였음
            if (apage->header.pid.pageNo != catEntry->firstPage) {
                pageNo = apage->header.prevPage;

                prevOID->volNo = curOID->volNo;
                prevOID->pageNo = pageNo;
                prevOID->slotNo = apage->header.nSlots - 1; //첫 object

                BfM_FreeTrain((TrainID *)&pid, PAGE_BUF);
                MAKE_PAGEID(pid, prevOID->volNo, prevOID->pageNo);
                BfM_GetTrain((TrainID *)&pid, (char **)&apage, PAGE_BUF);

                prevOID->unique = apage->slot[-prevOID->slotNo].unique;
            }
        }
    }

    BfM_FreeTrain((TrainID *)&pid, PAGE_BUF);
    BfM_FreeTrain((TrainID *)catObjForFile, PAGE_BUF);
    

    return(EOS);
    
} /* EduOM_PrevObject() */
