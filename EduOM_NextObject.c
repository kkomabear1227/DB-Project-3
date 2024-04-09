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
 * Module: EduOM_NextObject.c
 *
 * Description:
 *  Return the next Object of the given Current Object. 
 *
 * Export:
 *  Four EduOM_NextObject(ObjectID*, ObjectID*, ObjectID*, ObjectHdr*)
 */


#include "EduOM_common.h"
#include "BfM.h"
#include "EduOM_Internal.h"

/*@================================
 * EduOM_NextObject()
 *================================*/
/*
 * Function: Four EduOM_NextObject(ObjectID*, ObjectID*, ObjectID*, ObjectHdr*)
 *
 * Description:
 * (Following description is for original ODYSSEUS/COSMOS OM.
 *  For ODYSSEUS/EduCOSMOS EduOM, refer to the EduOM project manual.)
 *
 *  Return the next Object of the given Current Object.  Find the Object in the
 *  same page which has the current Object and  if there  is no next Object in
 *  the same page, find it from the next page. If the Current Object is NULL,
 *  return the first Object of the file.
 *
 * Returns:
 *  error code
 *    eBADCATALOGOBJECT_OM
 *    eBADOBJECTID_OM
 *    some errors caused by function calls
 *
 * Side effect:
 *  1) parameter nextOID
 *     nextOID is filled with the next object's identifier
 *  2) parameter objHdr
 *     objHdr is filled with the next object's header
 */
Four EduOM_NextObject(
    ObjectID  *catObjForFile,	/* IN informations about a data file */
    ObjectID  *curOID,		/* IN a ObjectID of the current Object */
    ObjectID  *nextOID,		/* OUT the next Object of a current Object */
    ObjectHdr *objHdr)		/* OUT the object header of next object */
{
    Four e;			/* error */
    Two  i;			/* index */
    Four offset;		/* starting offset of object within a page */
    PageID pid;			/* a page identifier */
    PageNo pageNo;		/* a temporary var for next page's PageNo */
    SlottedPage *apage;		/* a pointer to the data page */
    Object *obj;		/* a pointer to the Object */
    PhysicalFileID pFid;	/* file in which the objects are located */
    SlottedPage *catPage;	/* buffer page containing the catalog object */
    sm_CatOverlayForData *catEntry; /* data structure for catalog object access */

    /*@
     * parameter checking
     */
    if (catObjForFile == NULL) ERR(eBADCATALOGOBJECT_OM);
    
    if (nextOID == NULL) ERR(eBADOBJECTID_OM);

    // 0. catEntry를 불러온다.
    BfM_GetTrain((TrainID *)catObjForFile, (char **)&catPage, PAGE_BUF);
    GET_PTR_TO_CATENTRY_FOR_DATA(catObjForFile, catPage, catEntry);
    MAKE_PAGEID(pFid, catEntry->fid.volNo, catEntry->firstPage);

    // 1. curOID가 NULL인 경우
    if (curOID == NULL) {
        // File에 들어있는 첫 object의 ID를 반환한다.
        pageNo = catEntry->firstPage;

        MAKE_PAGEID(pid, pFid.volNo, pageNo);
        BfM_GetTrain((TrainID *)&pid, (char **)&apage, PAGE_BUF);

        nextOID->volNo = pid.volNo;
        nextOID->pageNo = pageNo;
        nextOID->slotNo = 0; // 첫 object
        nextOID->unique = apage->slot[0].unique;

        //BfM_FreeTrain((TrainID *)&pid, PAGE_BUF);
    }
    // 2. curOID가 NULL이 아닌 경우
    else {
        // 2-1. curOID에 대응되는 object를 찾는다.
        MAKE_PAGEID(pid, curOID->volNo, curOID->pageNo);
        BfM_GetTrain((TrainID *)&pid, (char **)&apage, PAGE_BUF);

        // 2-2. slot array에서 curOID 다음에 있는 object를 찾아 ID를 반환한다.
        //      만약 curOID object가 현재 page의 마지막 object라면 다음 page의 첫 object를 찾아 반환한다.
        //      만약 curOID object가 마지막 page의 마지막 object라면 EOS를 반환한다.

        // Case 1. curOID가 해당 page의 마지막 object가 아님
        if (curOID->slotNo + 1 != apage->header.nSlots) {
            nextOID->volNo = curOID->volNo;
            nextOID->pageNo = curOID->pageNo;
            nextOID->slotNo = curOID->slotNo + 1;
            nextOID->unique = apage->slot[-nextOID->slotNo].unique;
        }
        // Case 2. curOID가 해당 page의 마지막 object임
        else {
            // Case 2-1. curOID가 마지막 page의 마지막 object 였음
            //if (apage->header.pid.pageNo == catEntry->lastPage) continue;

            // Case 2-2. curOID가 nextPage가 있는 page의 마지막 object 였음
            if (apage->header.pid.pageNo != catEntry->lastPage) {
                pageNo = apage->header.nextPage;

                nextOID->volNo = curOID->volNo;
                nextOID->pageNo = pageNo;
                nextOID->slotNo = 0; //첫 object

                BfM_FreeTrain((TrainID *)&pid, PAGE_BUF);
                MAKE_PAGEID(pid, nextOID->volNo, nextOID->pageNo);
                BfM_GetTrain((TrainID *)&pid, (char **)&apage, PAGE_BUF);

                nextOID->unique = apage->slot[0].unique;
            }
        }
    }

    BfM_FreeTrain((TrainID *)&pid, PAGE_BUF);
    BfM_FreeTrain((TrainID *)catObjForFile, PAGE_BUF);
    return(EOS);		/* end of scan */
    
} /* EduOM_NextObject() */
