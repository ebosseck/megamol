/*
 * OSPRayGlassMaterial.cpp
 * Copyright (C) 2009-2017 by MegaMol Team
 * Alle Rechte vorbehalten.
 */

#include "OSPRayGlassMaterial.h"
#include "mmcore/param/FloatParam.h"
#include "mmcore/param/Vector3fParam.h"
#include "stdafx.h"

using namespace megamol::ospray;


OSPRayGlassMaterial::OSPRayGlassMaterial(void)
        : AbstractOSPRayMaterial()
        ,
        // GLASS
        glassEtaInside("EtaInside", "")
        , glassEtaOutside("EtaOutside", "")
        , glassAttenuationColorInside("AttenuationColorInside", "")
        , glassAttenuationColorOutside("AttenuationColorOutside", "")
        , glassAttenuationDistance("AttenuationDistance", "") {

    this->glassAttenuationColorInside << new core::param::Vector3fParam(
        vislib::math::Vector<float, 3>(1.0f, 1.0f, 1.0f));
    this->glassAttenuationColorOutside << new core::param::Vector3fParam(
        vislib::math::Vector<float, 3>(1.0f, 1.0f, 1.0f));
    this->glassAttenuationDistance << new core::param::FloatParam(1.0f);
    this->glassEtaOutside << new core::param::FloatParam(1.0f);
    this->glassEtaInside << new core::param::FloatParam(1.5f);
    this->MakeSlotAvailable(&this->glassAttenuationColorInside);
    this->MakeSlotAvailable(&this->glassAttenuationColorOutside);
    this->MakeSlotAvailable(&this->glassAttenuationDistance);
    this->MakeSlotAvailable(&this->glassEtaInside);
    this->MakeSlotAvailable(&this->glassEtaOutside);
}

OSPRayGlassMaterial::~OSPRayGlassMaterial(void) {
    this->Release();
}

void OSPRayGlassMaterial::readParams() {
    materialContainer.materialType = materialTypeEnum::GLASS;

    glassMaterial gm;
    auto colori = this->glassAttenuationColorInside.Param<core::param::Vector3fParam>();
    gm.glassAttenuationColorInside = colori->getArray();
    auto coloro = this->glassAttenuationColorOutside.Param<core::param::Vector3fParam>();
    gm.glassAttenuationColorOutside = coloro->getArray();
    gm.glassEtaInside = this->glassEtaInside.Param<core::param::FloatParam>()->Value();
    gm.glassEtaOutside = this->glassEtaOutside.Param<core::param::FloatParam>()->Value();
    gm.glassAttenuationDistance = this->glassAttenuationDistance.Param<core::param::FloatParam>()->Value();

    materialContainer.material = gm;
}

bool OSPRayGlassMaterial::InterfaceIsDirty() {
    if (this->glassAttenuationColorInside.IsDirty() || this->glassAttenuationColorOutside.IsDirty() ||
        this->glassAttenuationDistance.IsDirty() || this->glassEtaInside.IsDirty() || this->glassEtaOutside.IsDirty()) {
        this->glassAttenuationColorInside.ResetDirty();
        this->glassAttenuationColorOutside.ResetDirty();
        this->glassAttenuationDistance.ResetDirty();
        this->glassEtaInside.ResetDirty();
        this->glassEtaOutside.ResetDirty();
        return true;
    } else {
        return false;
    }
}
