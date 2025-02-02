/*
 * AbstractGetData3DCall.cpp
 *
 * Copyright (C) 2009 by Universitaet Stuttgart (VISUS).
 * Alle Rechte vorbehalten.
 */

#include "mmcore/AbstractGetData3DCall.h"
#include "stdafx.h"

using namespace megamol::core;


/*
 * AbstractGetData3DCall::AbstractGetData3DCall
 */
AbstractGetData3DCall::AbstractGetData3DCall(void)
        : AbstractGetDataCall()
        , forceFrame(false)
        , frameCnt(0)
        , frameID(0)
        , bboxs() {
    // intentionally empty
}


/*
 * AbstractGetData3DCall::~AbstractGetData3DCall
 */
AbstractGetData3DCall::~AbstractGetData3DCall(void) {
    this->Unlock();
}


/*
 * AbstractGetData3DCall::operator=
 */
AbstractGetData3DCall& AbstractGetData3DCall::operator=(const AbstractGetData3DCall& rhs) {
    AbstractGetDataCall::operator=(rhs);
    this->forceFrame = rhs.forceFrame;
    this->frameCnt = rhs.frameCnt;
    this->frameID = rhs.frameID;
    this->bboxs = rhs.bboxs;
    return *this;
}
