# EduOM Report

Name: 오형민

Student id: 20242502

# Problem Analysis

본 과제는 EduCOSMOS DBMS에서 object manager와 slotted page에 관련된 연산을 구현한다.
구체적으로 object의 생성/삭제/읽기 연산과, 이전/다음 object 찾기 연산, page 압축 알고리즘을 구현한다.

# Design For Problem Solving

## High Level

1. EduOM_CreateObject
File을 구성하는 page들 중 parameter로 지정한 object와 같거나 인접한 page에 새로운 object를 삽입하는 함수이다.
구체적인 동작 방식은 아래와 같다.
1-1. 삽입할 object의 header 초기화
1-2. eduom_CreateObject() 함수를 호출해 page에 object를 삽입

2. EduOM_CompactPage
page 내부에 object를 삽입할 연속된 free space가 부족할 경우, 모든 free space가 연속될 수 있도록 object들을 압축한다.
크게 2가지 경우로 나누어 구현한다.
2-1. slotNo가 NIL이 아닐 경우, slotNo에 대응되는 object를 제외한 object들을 앞부분부터 채우고, slotNo에 대응되는 object를 마지막에 배치한다.
2-2. slotNo가 NIL일 경우, 모든 object를 앞부분부터 채워넣는다.
2-3. page header를 갱신한다.

3. EduOM_DestroyObject
File을 구성하는 page에서 object를 삭제한다.
3-1. 삭제할 object를 available space list에서 삭제한다.
3-2. 삭제할 object에 대응되는 slot을 빈 slot으로 설정하고 page header를 갱신한다.
+ 삭제할 object에 대응되는 slot이 마지막 slot이라면 slot array의 크기를 바꾼다.
+ 삭제할 object의 위치에 따라 free, unused 변수값을 갱신한다.
3-3. 삭제할 object가 page의 유일한 object이고, page가 첫 페이지가 아니라면 해당 page를 file을 구성하는 page list에서 삭제한다.
3-4. 삭제된 object가 page의 유일한 object가 아니거나, page가 첫 페이지라면 page를 알맞은 available space list에 삽입한다.

4. EduOM_ReadObject
Object를 읽고, 데이터에 대한 포인터를 반환한다.
1) oid를 활용해 object에 접근한다.
2) start, length 값을 활용해 접근한 object의 데이터를 읽고 포인터를 반환한다.

5. EduOM_NextObject
현재 object의 다음 object oid를 반환한다.
1) curOID가 NULL이라면 첫 페이지의 첫 object ID를 반환한다.
2) curOID가 NULL이 아니라면, curOID에 대응되는 object를 탐색한 뒤 다음 object를 찾는다.
+ 경우에 따라, 다음 object가 같은 page에 존재하지 않을 수 있다. 이 경우 다음 page의 첫 object oid를 반환한다.
+ curOID에 대응되는 object가 마지막 page의 마지막 object라면 EOS를 반환한다.

6. EduOM_PrevObject
현재 object의 이전 object oid를 반환한다.
1) curOID가 NULL이라면 마지막 페이지의 마지막 object ID를 반환한다.
2) curOID가 NULL이 아니라면, curOID에 대응되는 object를 탐색한 뒤 이전 object를 찾는다.
+ 경우에 따라, 이전 object가 같은 page에 존재하지 않을 수 있다. 이 경우 앞 page의 마지막 object oid를 반환한다.
+ curOID에 대응되는 object가 첫 page의 첫 object라면 EOS를 반환한다.

## Low Level

1. eduOM_CreateObject
File을 구성하는 page 중 nearPid와 같거나 인접한 page에 새 object를 삽입한다.
1-1. Object 삽입을 위해 필요한 free space 양을 계산한다.
1-2. Object 삽입을 위해 page를 선정한다.
+ 만약 nearObj가 NULL이 아니면, nearObj가 저장된 page를 선정하거나, 그 옆에 새로운 page를 할당받는다.
+ 만약 nearObj가 NULL이라면, available space list에 적절한 free space를 가진 page를 할당받는다.
1-3. 선정한 page에 object를 삽입하고 header를 갱신한다.
+ slot array update, page header 갱신, available space list 업데이트가 이루어진다.
1-4. 삽입한 object의 ID를 반환한다.

# Mapping Between Implementation And the Design

1. EduOM_CreateObject
```cpp
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
```

2. eduOM_CreateObject
```cpp
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
```

3. EduOM_CompactPage
```cpp
// 하나의 slotted page 안에 있는 object가 연속할 수 있게 offset을 재조정
    // apage 원본을 keep 해놓는다.
    tpage = *apage;

    // 1. slotNo가 NIL(-1)이 아니라면
    if (slotNo != NIL) {
        //slotNo에 대응되는 object를 제외한 모든 object들을
        //앞에서부터 연속되게 저장한다.
        //printf("HI!\n");
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

        //slotNo에 대응되는 object는 마지막 object로 저장한다.
        obj = &(tpage.data[tpage.slot[-slotNo].offset]);
        len = ALIGNED_LENGTH(obj->header.length) + sizeof(ObjectHdr);

        memcpy(&(apage->data[apageDataOffset]), (char *)obj, len);
        apage->slot[slotNo].offset = apageDataOffset;
        apageDataOffset += len;
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
```

4. EduOM_DestroyObject()
```cpp
// 1. available space list에서 object를 포함하고 있는 page를 삭제한다.
    // 1-1. File의 catPage를 얻어온다.
    BfM_GetTrain((TrainID *)catObjForFile, (char **)&catPage, PAGE_BUF);
    GET_PTR_TO_CATENTRY_FOR_DATA(catObjForFile, catPage, catEntry);

    // 1-2. object가 들어있는 page를 얻어온다.
    MAKE_PAGEID(pid, oid->volNo, oid->pageNo);
    BfM_GetTrain((TrainID *)&pid, (char **)&apage, PAGE_BUF);

    // 1-3. available space list에서 page를 삭제한다.
    om_RemoveFromAvailSpaceList(catObjForFile, &pid, apage);

    // 2. 삭제할 object에 대응하는 slot을 empty unused slot으로 지정한다.
    // offset of the slot = EMPTYSLOT
    
    // 2-1. 원래 offset을 기록
    offset = apage->slot[-oid->slotNo].offset;
    obj = (Object *)&(apage->data[offset]);

    // 2-2. offset을 EMPTYSLOT으로 초기화
    apage->slot[-oid->slotNo].offset = EMPTYSLOT;

    // 3. Page Header 업데이트
    // Case 1. 지울 object가 slot array의 last slot이라면, slot array의 사이즈를 변경한다.
    if (apage->header.nSlots == oid->slotNo + 1) {
        apage->header.nSlots--;
    }

    alignedLen = ALIGNED_LENGTH(obj->header.length) + sizeof(ObjectHdr);

    // Object의 offset에 따라 free나 unused 변수 값을 업데이트
    if (offset + alignedLen == apage->header.free) apage->header.free -= alignedLen;
    else apage->header.unused += alignedLen;

    // 4. page가 file의 첫 페이지가 아니고, deleted object가 그 page의 only object라면
    if (apage->header.pid.pageNo != catEntry->firstPage && apage->header.nSlots == 0) {
        // 4-1. page를 page list에서 삭제
        om_FileMapDeletePage(catObjForFile, &pid);

        // 4-2. page를 deallocation
        // a. dlPool에서 새로운 dealloc list element를 할당받음
        Util_getElementFromPool(dlPool, &dlElem);

        // b. deallocation할 page의 정보를 dlElem에 저장한다.
        dlElem->type = DL_PAGE;
        dlElem->elem.pid = pid;

        // c. dealloc list의 first element를 dlElem으로 만든다.
        dlElem->next = dlHead->next;
        dlHead->next = dlElem;
    }
    // 5. page가 file의 첫 페이지거나, deleted object가 그 page의 여러 object 중 하나라면
    else {
        // page를 appropriate available space list에 삽입한다.
        om_PutInAvailSpaceList(catObjForFile, &pid, apage);
    }

    // 6. 마무리
    BfM_SetDirty((TrainID *)&pid, PAGE_BUF);
    BfM_FreeTrain((TrainID *)&pid, PAGE_BUF);
    BfM_FreeTrain((TrainID *)catObjForFile, PAGE_BUF);
```

5. EduOM_ReadObject
```cpp
// 1. oid를 활용해 object에 접근한다.
    // 1-1. oid를 활용해 object가 들어있는 page를 알아낸다.
    MAKE_PAGEID(pid, oid->volNo, oid->pageNo);
    BfM_GetTrain((TrainID *)&pid, (char **)&apage, PAGE_BUF);

    // 1-2. obj 포인터를 얻는다.
    obj = (Object *)&(apage->data[apage->slot[-oid->slotNo].offset]);

    // 2. start, length를 사용해 object의 데이터를 읽는다.
    // If length == REMAINDER, reade data to the end
    if (length == REMAINDER) length = obj->header.length;
    memcpy(buf, &(obj->data[start]), length);

    // 3. 마무리
    BfM_FreeTrain((TrainID *)&pid, PAGE_BUF);
```

6. EduOM_NextObject
```cpp
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
```

7. EduOM_PrevObject
```cpp
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
```