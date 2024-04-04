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
 * Module : EduOM_CompactPage.c
 * 
 * Description : 
 *  EduOM_CompactPage() reorganizes the page to make sure the unused bytes
 *  in the page are located contiguously "in the middle", between the tuples
 *  and the slot array. 
 *
 * Exports:
 *  Four EduOM_CompactPage(SlottedPage*, Two)
 */


#include <string.h>
#include "EduOM_common.h"
#include "LOT.h"
#include "EduOM_Internal.h"



/*@================================
 * EduOM_CompactPage()
 *================================*/
/*
 * Function: Four EduOM_CompactPage(SlottedPage*, Two)
 * 
 * Description : 
 * (Following description is for original ODYSSEUS/COSMOS OM.
 *  For ODYSSEUS/EduCOSMOS EduOM, refer to the EduOM project manual.)
 *
 *  (1) What to do?
 *  EduOM_CompactPage() reorganizes the page to make sure the unused bytes
 *  in the page are located contiguously "in the middle", between the tuples
 *  and the slot array. To compress out holes, objects must be moved toward
 *  the beginning of the page.
 *
 *  (2) How to do?
 *  a. Save the given page into the temporary page
 *  b. FOR each nonempty slot DO
 *	Fill the original page by copying the object from the saved page
 *          to the data area of original page pointed by 'apageDataOffset'
 *	Update the slot offset
 *	Get 'apageDataOffet' to point the next moved position
 *     ENDFOR
 *   c. Update the 'freeStart' and 'unused' field of the page
 *   d. Return
 *	
 * Returns:
 *  error code
 *    eNOERROR
 *
 * Side Effects :
 *  The slotted page is reorganized to comact the space.
 */
Four EduOM_CompactPage(
    SlottedPage	*apage,		/* IN slotted page to compact */
    Two         slotNo)		/* IN slotNo to go to the end */
{
    SlottedPage	tpage;		/* temporay page used to save the given page */
    Object *obj;		/* pointer to the object in the data area */
    Two    apageDataOffset;	/* where the next object is to be moved */
    Four   len;			/* length of object + length of ObjectHdr */
    Two    lastSlot;		/* last non empty slot */
    Two    i;			/* index variable */

    // 하나의 slotted page 안에 있는 object가 연속할 수 있게 offset을 재조정
    // apage 원본을 keep 해놓는다.
    tpage = *apage;

    // 1. slotNo가 NIL(-1)이 아니라면
    if (slotNo != NIL) {
        //slotNo에 대응되는 object를 제외한 모든 object들을
        //앞에서부터 연속되게 저장한다.
        apageDataOffset = 0;
        for (i = 0; i < tpage.header.nSlots; i++) {
            if (i == slotNo) continue;
            if (tpage.slot[-i].offset == EMPTYSLOT) continue;

            obj = &(tpage.data[tpage.slot[-i].offset]);
            len = ALIGNED_LENGTH(obj->header.length) + sizeof(ObjectHdr);
            
            //object를 복사
            memcpy(&(apage->data[apageDataOffset]), (char *)obj, len);

            //offset 정보를 slot에 기록, metadata update
            apage->slot[-i].offset = apageDataOffset;
            apageDataOffset += len;
        }
    }
    // 2. slotNo가 NIL(-1)이라면
    else {
        //모든 object들을 앞에서부터 연속되게 저장
        apageDataOffset = 0;
        for (i = 0; i<tpage.header.nSlots; i++) {
            obj = &(tpage.data[tpage.slot[-i].offset]);
            len = ALIGNED_LENGTH(obj->header.length) + sizeof(ObjectHdr);

            //object를 복사
            memcpy(&(apage->data[apageDataOffset]), (char *)obj, len);

            //offset 정보를 slot에 기록, metadata update
            apage->slot[-i].offset = apageDataOffset;
            apageDataOffset += len;
        }
    }

    apage->header.unused = 0;
    apage->header.free = apageDataOffset;

    return(eNOERROR);
    
} /* EduOM_CompactPage */