/*
 * OSPRayMetallicPaintMaterial.cpp
 * Copyright (C) 2009-2017 by MegaMol Team
 * Alle Rechte vorbehalten.
 */

#include "OSPRayMetallicPaintMaterial.h"
#include "mmcore/param/FloatParam.h"
#include "mmcore/param/Vector3fParam.h"
#include "stdafx.h"

using namespace megamol::ospray;


OSPRayMetallicPaintMaterial::OSPRayMetallicPaintMaterial(void)
        : AbstractOSPRayMaterial()
        ,
        // METALLIC
        metallicShadeColor("ShadeColor", "")
        , metallicGlitterColor("GlitterColor", "")
        , metallicGlitterSpread("GlitterSpread", "")
        , metallicEta("Eta", "") {

    this->metallicShadeColor << new core::param::Vector3fParam(vislib::math::Vector<float, 3>(0.5f, 0.42f, 0.35f));
    this->metallicGlitterColor << new core::param::Vector3fParam(vislib::math::Vector<float, 3>(0.5f, 0.44f, 0.42f));
    this->metallicEta << new core::param::FloatParam(1.45f);
    this->metallicGlitterSpread << new core::param::FloatParam(0.0f);

    this->MakeSlotAvailable(&this->metallicEta);
    this->MakeSlotAvailable(&this->metallicGlitterColor);
    this->MakeSlotAvailable(&this->metallicGlitterSpread);
    this->MakeSlotAvailable(&this->metallicShadeColor);
}

OSPRayMetallicPaintMaterial::~OSPRayMetallicPaintMaterial(void) {
    this->Release();
}

void OSPRayMetallicPaintMaterial::readParams() {
    materialContainer.materialType = materialTypeEnum::METALLICPAINT;

    metallicpaintMaterial mpm;

    auto scolor = this->metallicShadeColor.Param<core::param::Vector3fParam>();
    mpm.metallicShadeColor = scolor->getArray();
    auto gcolor = this->metallicGlitterColor.Param<core::param::Vector3fParam>();
    mpm.metallicGlitterColor = gcolor->getArray();
    mpm.metallicGlitterSpread = this->metallicGlitterSpread.Param<core::param::FloatParam>()->Value();
    mpm.metallicEta = this->metallicEta.Param<core::param::FloatParam>()->Value();

    materialContainer.material = mpm;
}

bool OSPRayMetallicPaintMaterial::InterfaceIsDirty() {
    if (this->metallicEta.IsDirty() || this->metallicGlitterColor.IsDirty() || this->metallicGlitterSpread.IsDirty() ||
        this->metallicShadeColor.IsDirty()) {
        this->metallicEta.ResetDirty();
        this->metallicGlitterColor.ResetDirty();
        this->metallicGlitterSpread.ResetDirty();
        this->metallicShadeColor.ResetDirty();
        return true;
    } else {
        return false;
    }
}
