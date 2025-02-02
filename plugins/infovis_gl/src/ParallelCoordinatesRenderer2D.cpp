#include "ParallelCoordinatesRenderer2D.h"

#include <algorithm>
#include <array>
#include <iostream>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "datatools/table/TableDataCall.h"
#include "mmcore/CoreInstance.h"
#include "mmcore/param/BoolParam.h"
#include "mmcore/param/ButtonParam.h"
#include "mmcore/param/ColorParam.h"
#include "mmcore/param/EnumParam.h"
#include "mmcore/param/FlexEnumParam.h"
#include "mmcore/param/FloatParam.h"
#include "mmcore/param/IntParam.h"
#include "mmcore/param/StringParam.h"
#include "mmcore/utility/ColourParser.h"
#include "mmcore_gl/FlagCallsGL.h"
#include "mmcore_gl/utility/ShaderFactory.h"
#include "mmcore_gl/view/CallGetTransferFunctionGL.h"
#include "vislib/graphics/InputModifiers.h"
#include "vislib_gl/graphics/gl/IncludeAllGL.h"
#include "vislib_gl/graphics/gl/ShaderSource.h"

//#define FUCK_THE_PIPELINE
//#define USE_TESSELLATION
//#define REMOVE_TEXT

using namespace megamol;
using namespace megamol::infovis_gl;
using namespace megamol::datatools;

ParallelCoordinatesRenderer2D::ParallelCoordinatesRenderer2D(void)
        : Renderer2D()
        , getDataSlot("getdata", "Float table input")
        , getTFSlot("getTF", "connects to the transfer function")
        , readFlagsSlot("readFlags", "reads the flag storage")
        , writeFlagsSlot("writeFlags", "writes the flag storage")
        , currentHash(0xFFFFFFFF)
        , currentFlagsVersion(0)
        , densityFBO()
        , drawModeSlot("drawMode", "Draw mode")
        , drawSelectedItemsSlot("drawSelectedItems", "Draw selected items")
        , selectedItemsColorSlot("selectedItemsColor", "Color for selected items")
        , drawOtherItemsSlot("drawOtherItems", "Draw other (e.g., non-selected) items")
        , otherItemsColorSlot("otherItemsColor", "Color for other items (e.g., non-selected)")
        , otherItemsAttribSlot("otherItemsAttrib", "attribute to use for TF lookup and item coloring")
        , drawAxesSlot("drawAxes", "Draw dimension axes")
        , axesColorSlot("axesColor", "Color for dimension axes")
        , filterIndicatorColorSlot("filterIndicatorCol", "Color for filter indicators")
        , selectionModeSlot("selectionMode", "Selection mode")
        , drawSelectionIndicatorSlot("drawSelectionIndicator", "Draw selection indicator")
        , selectionIndicatorColorSlot("selectionIndicatorColor", "Color for selection indicator")
        , pickRadiusSlot("pickRadius", "Picking radius in object-space")
        , scaleToFitSlot("scaleToFit", "fit the diagram in the viewport")
        , glDepthTestSlot("glEnableDepthTest", "Toggle GLDEPTHTEST")
        , glLineSmoothSlot("glEnableLineSmooth", "Toggle GLLINESMOOTH")
        , glLineWidthSlot("glLineWidth", "Value for glLineWidth")
        , sqrtDensitySlot("sqrtDensity", "map root of density to transfer function (instead of linear mapping)")
        //, resetFlagsSlot("resetFlags", "Reset item flags to initial state")
        , resetFiltersSlot("resetFilters", "Reset dimension filters to initial state")
        , filterStateSlot("filterState", "stores filter state for serialization")
        , triangleModeSlot("triangleMode", "Enables triangles instead of GL_LINES")
        , lineThicknessSlot("lineThickness", "Float value to incease line thickness")
        , axesLineThicknessSlot("axesLineThickness", "Float value to incease line thickness of Axes and Indicators")
        , numTicks(5)
        , columnCount(0)
        , itemCount(0)
        , dataBuffer(0)
        , minimumsBuffer(0)
        , maximumsBuffer(0)
        , axisIndirectionBuffer(0)
        , filtersBuffer(0)
        , minmaxBuffer(0)
        , interactionState(InteractionState::NONE)
        , pickedAxis(-1)
        , pickedIndicatorAxis(-1)
        , pickedIndicatorIndex(-1)
        , strokeStartX(0)
        , strokeStartY(0)
        , strokeEndX(0)
        , strokeEndY(0)
        , needSelectionUpdate(false)
        , needFlagsUpdate(false)
        , lastTimeStep(0)
        , font(core::utility::SDFFont::PRESET_EVOLVENTA_SANS, core::utility::SDFFont::RENDERMODE_FILL) {

    this->getDataSlot.SetCompatibleCall<table::TableDataCallDescription>();
    this->MakeSlotAvailable(&this->getDataSlot);

    this->getTFSlot.SetCompatibleCall<core_gl::view::CallGetTransferFunctionGLDescription>();
    this->MakeSlotAvailable(&this->getTFSlot);

    this->readFlagsSlot.SetCompatibleCall<core_gl::FlagCallRead_GLDescription>();
    this->MakeSlotAvailable(&this->readFlagsSlot);
    this->writeFlagsSlot.SetCompatibleCall<core_gl::FlagCallWrite_GLDescription>();
    this->MakeSlotAvailable(&this->writeFlagsSlot);

    auto drawModes = new core::param::EnumParam(DRAW_DISCRETE);
    drawModes->SetTypePair(DRAW_DISCRETE, "Discrete");
    drawModes->SetTypePair(DRAW_CONTINUOUS, "Continuous");
    drawModes->SetTypePair(DRAW_HISTOGRAM, "Histogram");
    drawModeSlot.SetParameter(drawModes);
    this->MakeSlotAvailable(&drawModeSlot);

    drawSelectedItemsSlot << new core::param::BoolParam(true);
    this->MakeSlotAvailable(&drawSelectedItemsSlot);

    selectedItemsColorSlot << new core::param::ColorParam("red");
    this->MakeSlotAvailable(&selectedItemsColorSlot);

    drawOtherItemsSlot << new core::param::BoolParam(true);
    this->MakeSlotAvailable(&drawOtherItemsSlot);

    otherItemsColorSlot << new core::param::ColorParam("gray");
    this->MakeSlotAvailable(&otherItemsColorSlot);
    otherItemsAttribSlot << new core::param::FlexEnumParam("undef");
    this->MakeSlotAvailable(&this->otherItemsAttribSlot);

    drawAxesSlot << new core::param::BoolParam(true);
    this->MakeSlotAvailable(&drawAxesSlot);

    axesColorSlot << new core::param::ColorParam("white");
    this->MakeSlotAvailable(&axesColorSlot);

    filterIndicatorColorSlot << new core::param::ColorParam("orange");
    this->MakeSlotAvailable(&filterIndicatorColorSlot);

    drawSelectionIndicatorSlot << new core::param::BoolParam(true);
    this->MakeSlotAvailable(&drawSelectionIndicatorSlot);

    selectionIndicatorColorSlot << new core::param::ColorParam("MegaMolBlue");
    this->MakeSlotAvailable(&selectionIndicatorColorSlot);

    auto pickModes = new core::param::EnumParam(SELECT_STROKE);
    pickModes->SetTypePair(SELECT_PICK, "Pick");
    pickModes->SetTypePair(SELECT_STROKE, "Stroke");
    selectionModeSlot.SetParameter(pickModes);
    this->MakeSlotAvailable(&selectionModeSlot);

    pickRadiusSlot << new core::param::FloatParam(0.1f, 0.01f, 10.0f);
    this->MakeSlotAvailable(&pickRadiusSlot);

    scaleToFitSlot << new core::param::BoolParam(false);
    scaleToFitSlot.SetUpdateCallback(this, &ParallelCoordinatesRenderer2D::scalingChangedCallback);
    this->MakeSlotAvailable(&scaleToFitSlot);

    glDepthTestSlot << new core::param::BoolParam(false);
    this->MakeSlotAvailable(&glDepthTestSlot);

    glLineSmoothSlot << new core::param::BoolParam(false);
    this->MakeSlotAvailable(&glLineSmoothSlot);

    glLineWidthSlot << new core::param::FloatParam(1.0f, 0.1f);
    this->MakeSlotAvailable(&glLineWidthSlot);

    sqrtDensitySlot << new core::param::BoolParam(true);
    this->MakeSlotAvailable(&sqrtDensitySlot);

    // resetFlagsSlot << new core::param::ButtonParam();
    // resetFlagsSlot.SetUpdateCallback(this, &ParallelCoordinatesRenderer2D::resetFlagsSlotCallback);
    // this->MakeSlotAvailable(&resetFlagsSlot);

    resetFiltersSlot << new core::param::ButtonParam();
    resetFiltersSlot.SetUpdateCallback(this, &ParallelCoordinatesRenderer2D::resetFiltersSlotCallback);
    this->MakeSlotAvailable(&resetFiltersSlot);

    filterStateSlot << new ::core::param::StringParam("");
    // filterStateSlot.Param<core::param::StringParam>()->SetGUIVisible(false);
    this->MakeSlotAvailable(&filterStateSlot);

    this->triangleModeSlot << new core::param::BoolParam(false);
    this->MakeSlotAvailable(&triangleModeSlot);

    this->lineThicknessSlot << new core::param::FloatParam(1.5);
    this->MakeSlotAvailable(&lineThicknessSlot);

    this->axesLineThicknessSlot << new core::param::FloatParam(1.0);
    this->MakeSlotAvailable(&axesLineThicknessSlot);

    fragmentMinMax.resize(2);
}

ParallelCoordinatesRenderer2D::~ParallelCoordinatesRenderer2D(void) {
    this->Release();
}

bool ParallelCoordinatesRenderer2D::enableProgramAndBind(std::unique_ptr<glowl::GLSLProgram>& program) {
    program->use();
    // bindbuffer?
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, dataBuffer);
    auto flags = this->readFlagsSlot.CallAs<core_gl::FlagCallRead_GL>();
    flags->getData()->validateFlagCount(this->itemCount);
    flags->getData()->flags->bind(1);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, minimumsBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, maximumsBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, axisIndirectionBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, filtersBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, minmaxBuffer);

    glUniform2f(program->getUniformLocation("scaling"), 1.0f, 1.0f); // scaling, whatever
    glUniformMatrix4fv(
        program->getUniformLocation("modelView"), 1, GL_FALSE, glm::value_ptr(camera_cpy.getViewMatrix()));
    glUniformMatrix4fv(
        program->getUniformLocation("projection"), 1, GL_FALSE, glm::value_ptr(camera_cpy.getProjectionMatrix()));
    glUniform1ui(program->getUniformLocation("dimensionCount"), this->columnCount);
    glUniform1ui(program->getUniformLocation("itemCount"), this->itemCount);

    glUniform2f(program->getUniformLocation("margin"), this->marginX, this->marginY);
    glUniform1f(program->getUniformLocation("axisDistance"), this->axisDistance);
    glUniform1f(program->getUniformLocation("axisHeight"), this->axisHeight);

    return true;
}

bool ParallelCoordinatesRenderer2D::create(void) {
    glGenBuffers(1, &dataBuffer);
    glGenBuffers(1, &minimumsBuffer);
    glGenBuffers(1, &maximumsBuffer);
    glGenBuffers(1, &axisIndirectionBuffer);
    glGenBuffers(1, &filtersBuffer);
    glGenBuffers(1, &minmaxBuffer);
    glGenBuffers(1, &counterBuffer);

#ifndef REMOVE_TEXT
    if (!font.Initialise(this->GetCoreInstance()))
        return false;
    font.SetBatchDrawMode(true);
#endif

    auto const shader_options = msf::ShaderFactoryOptionsOpenGL(this->GetCoreInstance()->GetShaderPaths());

    try {
        drawAxesProgram = core::utility::make_glowl_shader("pc_axes_draw_axes", shader_options,
            "infovis_gl/pc_axes_draw_axes.vert.glsl", "infovis_gl/pc_axes_draw_axes.frag.glsl");

        drawScalesProgram = core::utility::make_glowl_shader("pc_axes_draw_scales", shader_options,
            "infovis_gl/pc_axes_draw_scales.vert.glsl", "infovis_gl/pc_axes_draw_scales.frag.glsl");

        drawFilterIndicatorsProgram = core::utility::make_glowl_shader("pc_axes_draw_filterindicators", shader_options,
            "infovis_gl/pc_axes_draw_filterindicators.vert.glsl", "infovis_gl/pc_axes_draw_filterindicators.frag.glsl");

        drawStrokeIndicatorProgram = core::utility::make_glowl_shader("pc_item_stroke_indicator", shader_options,
            "infovis_gl/pc_item_stroke_indicator.vert.glsl", "infovis_gl/pc_item_stroke_indicator.frag.glsl");

        drawPickIndicatorProgram = core::utility::make_glowl_shader("pc_item_pick_indicator", shader_options,
            "infovis_gl/pc_item_pick_indicator.vert.glsl", "infovis_gl/pc_item_pick_indicator.frag.glsl");

        drawItemsDiscreteProgram = core::utility::make_glowl_shader("pc_item_draw_discrete", shader_options,
            "infovis_gl/pc_item_draw_discrete.vert.glsl", "infovis_gl/pc_item_draw_discrete.frag.glsl");

        drawItemsTriangleProgram = core::utility::make_glowl_shader("pc_item_draw_discreteT", shader_options,
            "infovis_gl/pc_item_draw_discreteT.vert.glsl", "infovis_gl/pc_item_draw_discreteT.frag.glsl");

        traceItemsDiscreteProgram = core::utility::make_glowl_shader("pc_item_draw_muhaha", shader_options,
            "infovis_gl/pc_item_draw_muhaha.vert.glsl", "infovis_gl/pc_item_draw_muhaha.frag.glsl");

        drawItemsDiscreteTessProgram = core::utility::make_glowl_shader("pc_item_draw_discTess", shader_options,
            "infovis_gl/pc_item_draw_discTess.vert.glsl", "infovis_gl/pc_item_draw_discTess.tesc.glsl",
            "infovis_gl/pc_item_draw_discTess.tese.glsl", "infovis_gl/pc_item_draw_discTess.frag.glsl");

        drawItemContinuousProgram = core::utility::make_glowl_shader("pc_fragment_count", shader_options,
            "infovis_gl/pc_fragment_count.vert.glsl", "infovis_gl/pc_fragment_count.frag.glsl");

        minMaxProgram = core::utility::make_glowl_shader(
            "pc_fragment_count", shader_options, "infovis_gl/pc_fragment_count.comp.glsl");

        drawItemsHistogramProgram = core::utility::make_glowl_shader("pc_item_draw_histogram", shader_options,
            "infovis_gl/pc_item_draw_histogram.vert.glsl", "infovis_gl/pc_item_draw_histogram.frag.glsl");

        filterProgram =
            core::utility::make_glowl_shader("pc_item_filter", shader_options, "infovis_gl/pc_item_filter.comp.glsl");

        pickProgram =
            core::utility::make_glowl_shader("pc_item_pick", shader_options, "infovis_gl/pc_item_pick.comp.glsl");

        strokeProgram =
            core::utility::make_glowl_shader("pc_item_stroke", shader_options, "infovis_gl/pc_item_stroke.comp.glsl");

    } catch (std::exception& e) {
        megamol::core::utility::log::Log::DefaultLog.WriteMsg(megamol::core::utility::log::Log::LEVEL_ERROR,
            ("ParallelCoordinatesRenderer2D: " + std::string(e.what())).c_str());
        return false;
    }

    glGetIntegerv(GL_MAX_TESS_GEN_LEVEL, &this->maxAxes); // TODO we should reject data with more axes!
    this->isoLinesPerInvocation = maxAxes; // warning: for tesslevel n there are JUST n lines!!! not n+1 !!

    glGetProgramiv(filterProgram->getHandle(), GL_COMPUTE_WORK_GROUP_SIZE, filterWorkgroupSize);
    glGetProgramiv(minMaxProgram->getHandle(), GL_COMPUTE_WORK_GROUP_SIZE, counterWorkgroupSize);
    glGetProgramiv(pickProgram->getHandle(), GL_COMPUTE_WORK_GROUP_SIZE, pickWorkgroupSize);
    glGetProgramiv(strokeProgram->getHandle(), GL_COMPUTE_WORK_GROUP_SIZE, strokeWorkgroupSize);

    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &maxWorkgroupCount[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &maxWorkgroupCount[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &maxWorkgroupCount[2]);

    // this->filterStateSlot.ForceSetDirty();

    return true;
}

void ParallelCoordinatesRenderer2D::release(void) {
    glDeleteBuffers(1, &dataBuffer);
    glDeleteBuffers(1, &minimumsBuffer);
    glDeleteBuffers(1, &maximumsBuffer);
    glDeleteBuffers(1, &axisIndirectionBuffer);
    glDeleteBuffers(1, &filtersBuffer);
    glDeleteBuffers(1, &minmaxBuffer);
    glDeleteBuffers(1, &counterBuffer);
}

int ParallelCoordinatesRenderer2D::mouseXtoAxis(float x) {
    float f = (x - this->marginX) / this->axisDistance;
    float frac = f - static_cast<long>(f);
    int integral = static_cast<int>(std::round(f));
    if (integral >= static_cast<int>(this->columnCount) || integral < 0)
        return -1;
    if (frac > 0.7 || frac < 0.3) {
        // megamol::core::utility::log::Log::DefaultLog.WriteInfo("picking axis %i at mouse position of axis %i",
        // axisIndirection[integral], integral);
        return axisIndirection[integral];
    } else {
        return -1;
    }
}

void ParallelCoordinatesRenderer2D::pickIndicator(float x, float y, int& axis, int& index) {
    axis = mouseXtoAxis(x);
    index = -1;
    if (axis != -1) {
        // calculate position of click and filters in [0, 1] range of axis height
        float pickPos = (y - this->marginY) / this->axisHeight;
        float upperPos = (this->filters[axis].upper - minimums[axis]) / (maximums[axis] - minimums[axis]);
        float lowerPos = (this->filters[axis].lower - minimums[axis]) / (maximums[axis] - minimums[axis]);

        // Add small epsilon for better UI feeling because indicator is drawn only to one side.
        // This also handles intuitive selection if upper and lower filter are set to the same value.
        upperPos += 0.01;
        lowerPos -= 0.01;

        float distUpper = fabs(upperPos - pickPos);
        float distLower = fabs(lowerPos - pickPos);

        float thresh = 0.1f;
        if (distUpper < thresh && distUpper < distLower) {
            index = 1;
        } else if (distLower < thresh) {
            index = 0;
        }
    }
    if (index == -1) {
        axis = -1;
    }
}

bool ParallelCoordinatesRenderer2D::scalingChangedCallback(core::param::ParamSlot& caller) {
    this->computeScaling();
    return true;
}

// bool ParallelCoordinatesRenderer2D::resetFlagsSlotCallback(core::param::ParamSlot& caller) { return true; }

bool ParallelCoordinatesRenderer2D::resetFiltersSlotCallback(core::param::ParamSlot& caller) {
    for (GLuint i = 0; i < this->columnCount; i++) {
        this->filters[i].lower = minimums[i];
        this->filters[i].upper = maximums[i];
    }
    this->needFlagsUpdate = true;
    return true;
}

bool ParallelCoordinatesRenderer2D::assertData(core_gl::view::CallRender2DGL& call) {
    auto floats = getDataSlot.CallAs<megamol::datatools::table::TableDataCall>();
    if (floats == nullptr)
        return false;
    auto tc = getTFSlot.CallAs<megamol::core_gl::view::CallGetTransferFunctionGL>();
    if (tc == nullptr) {
        megamol::core::utility::log::Log::DefaultLog.WriteMsg(megamol::core::utility::log::Log::LEVEL_ERROR,
            "ParallelCoordinatesRenderer2D requires a transfer function!");
        return false;
    }
    auto flagsc = readFlagsSlot.CallAs<core_gl::FlagCallRead_GL>();
    if (flagsc == nullptr) {
        megamol::core::utility::log::Log::DefaultLog.WriteMsg(
            megamol::core::utility::log::Log::LEVEL_ERROR, "ParallelCoordinatesRenderer2D requires a flag storage!");
        return false;
    }

    floats->SetFrameID(static_cast<unsigned int>(call.Time()));
    (*floats)(1);
    (*floats)(0);
    call.SetTimeFramesCount(floats->GetFrameCount());
    auto hash = floats->DataHash();
    (*flagsc)(core_gl::FlagCallRead_GL::CallGetData);
    if (flagsc->hasUpdate()) {
        this->currentFlagsVersion = flagsc->version();
    }

    if (hash != this->currentHash || this->lastTimeStep != static_cast<unsigned int>(call.Time()) ||
        this->otherItemsAttribSlot.IsDirty()) {
        // set minmax for TF only when frame or hash changes
        try {
            auto colcol = this->columnIndex.at(this->otherItemsAttribSlot.Param<core::param::FlexEnumParam>()->Value());
            tc->SetRange(
                {floats->GetColumnsInfos()[colcol].MinimumValue(), floats->GetColumnsInfos()[colcol].MaximumValue()});
            this->otherItemsAttribSlot.ResetDirty();
        } catch (std::out_of_range& ex) {
            megamol::core::utility::log::Log::DefaultLog.WriteError(
                "ParallelCoordinatesRenderer2D: tried to color lines by non-existing column '%s'",
                this->otherItemsAttribSlot.Param<core::param::FlexEnumParam>()->Value().c_str());
        }
    }

    if (hash != this->currentHash || this->lastTimeStep != static_cast<unsigned int>(call.Time())) {

        this->computeScaling();

        this->columnCount = static_cast<GLuint>(floats->GetColumnsCount());
        this->itemCount = static_cast<GLuint>(floats->GetRowsCount());
        this->axisIndirection.resize(columnCount);
        this->filters.resize(columnCount);
        this->minimums.resize(columnCount);
        this->maximums.resize(columnCount);
        this->names.resize(columnCount);
        this->otherItemsAttribSlot.Param<core::param::FlexEnumParam>()->ClearValues();
        this->columnIndex.clear();
        for (GLuint x = 0; x < columnCount; x++) {
            axisIndirection[x] = x;
            filters[x].dimension = 0;
            filters[x].flags = 0;
            minimums[x] = floats->GetColumnsInfos()[x].MinimumValue();
            maximums[x] = floats->GetColumnsInfos()[x].MaximumValue();
            if (maximums[x] - minimums[x] < 0.001f) {
                maximums[x] = minimums[x] + 1.f;
            }
            names[x] = floats->GetColumnsInfos()[x].Name();
            filters[x].lower = minimums[x];
            filters[x].upper = maximums[x];
            this->otherItemsAttribSlot.Param<core::param::FlexEnumParam>()->AddValue(
                floats->GetColumnsInfos()[x].Name());
            this->columnIndex[floats->GetColumnsInfos()[x].Name()] = x;
        }

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, dataBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, this->columnCount * this->itemCount * sizeof(float), floats->GetData(),
            GL_STATIC_DRAW); // TODO: huh.
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, minimumsBuffer);
        glBufferData(
            GL_SHADER_STORAGE_BUFFER, this->columnCount * sizeof(GLfloat), this->minimums.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, maximumsBuffer);
        glBufferData(
            GL_SHADER_STORAGE_BUFFER, this->columnCount * sizeof(GLfloat), this->maximums.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, axisIndirectionBuffer);
        glBufferData(
            GL_SHADER_STORAGE_BUFFER, this->columnCount * sizeof(GLuint), axisIndirection.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, filtersBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, this->columnCount * sizeof(DimensionFilter), this->filters.data(),
            GL_STATIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, minmaxBuffer);
        glBufferData(
            GL_SHADER_STORAGE_BUFFER, 2 * sizeof(GLfloat), fragmentMinMax.data(), GL_DYNAMIC_READ); // TODO: huh.
        this->currentHash = hash;
        this->lastTimeStep = static_cast<unsigned int>(call.Time());
    }

    (*tc)(0);

    makeDebugLabel(GL_BUFFER, DEBUG_NAME(dataBuffer));
    makeDebugLabel(GL_BUFFER, DEBUG_NAME(minimumsBuffer));
    makeDebugLabel(GL_BUFFER, DEBUG_NAME(maximumsBuffer));
    makeDebugLabel(GL_BUFFER, DEBUG_NAME(axisIndirectionBuffer));
    makeDebugLabel(GL_BUFFER, DEBUG_NAME(filtersBuffer));
    makeDebugLabel(GL_BUFFER, DEBUG_NAME(minmaxBuffer));

    return true;
}

void ParallelCoordinatesRenderer2D::computeScaling(void) {
    auto fc = getDataSlot.CallAs<megamol::datatools::table::TableDataCall>();
    if (fc == nullptr)
        return;

    this->marginX = 20.f;
    this->marginY = 20.f;
    this->axisDistance = 40.0f;
    this->fontSize = this->axisDistance / 10.0f;
    auto left = 0.0f;
    auto right = 2.0f * marginX + this->axisDistance * (fc->GetColumnsCount() - 1);
    auto width = right - left;

    if (this->scaleToFitSlot.Param<core::param::BoolParam>()->Value()) {
        // scale to fit
        float requiredHeight = width / camera_cpy.get<core::view::Camera::OrthographicParameters>().aspect;
        this->axisHeight = requiredHeight - 3.0f * marginY;
    } else {
        this->axisHeight = 80.0f;
    }
    auto bottom = 0.0f;
    auto top = 3.0f * marginY + this->axisHeight;

    bounds.SetBoundingBox(left, bottom, 0, right, top, 0);
}

bool ParallelCoordinatesRenderer2D::GetExtents(core_gl::view::CallRender2DGL& call) {

    if (!this->assertData(call)) {
        return false;
    }

    call.AccessBoundingBoxes() = this->bounds;

    return true;
}

bool ParallelCoordinatesRenderer2D::OnMouseButton(
    core::view::MouseButton button, core::view::MouseButtonAction action, core::view::Modifiers mods) {

    if (this->ctrlDown) {
        // these clicks go to the view
        return false;
    }

    // TODO could values from mouse move be updated from other threads? otherwise this copy is useless
    float mousePressedX = this->mouseX;
    float mousePressedY = this->mouseY;

    if (button != core::view::MouseButton::BUTTON_LEFT) {
        // ignore everything which is not left mouse button
        return false;
    }

    // any up/down event stops interaction, but only down changes selection
    this->interactionState = InteractionState::NONE;
    if (action == core::view::MouseButtonAction::PRESS) {
        this->pickedAxis = -1;
        this->pickedIndicatorAxis = -1;
        this->pickedIndicatorIndex = -1;

        if (this->altDown) {
            this->pickedAxis = mouseXtoAxis(mousePressedX);
            if (this->pickedAxis != -1) {
                this->interactionState = InteractionState::INTERACTION_DRAG;
            }
            return true;
        }

        if (this->shiftDown) {

            auto axis = mouseXtoAxis(mousePressedX);
            if (axis != -1) {
                float base = this->marginY * 0.5f - fontSize * 0.5f;
                if ((mousePressedY > base && mousePressedY < base + fontSize) ||
                    (mousePressedY > base + this->marginY + this->axisHeight &&
                        mousePressedY < base + this->marginY + this->axisHeight + fontSize)) {
                    std::swap(this->filters[axis].lower, this->filters[axis].upper);
                    this->needFlagsUpdate = true;
                    return true;
                }
            }

            pickIndicator(mousePressedX, mousePressedY, this->pickedIndicatorAxis, this->pickedIndicatorIndex);
            if (this->pickedIndicatorAxis != -1) {
                this->interactionState = InteractionState::INTERACTION_FILTER;
            }
            return true;
        }

        this->interactionState = InteractionState::INTERACTION_SELECT;
        this->strokeStartX = mousePressedX;
        this->strokeStartY = mousePressedY;
        this->strokeEndX = mousePressedX;
        this->strokeEndY = mousePressedY;
        this->needSelectionUpdate = true;
        return true;
    }

    return true;
}

bool ParallelCoordinatesRenderer2D::OnMouseMove(double x, double y) {
    // Make the following a convenience function in the future
    auto cam_pose = camera_cpy.get<core::view::Camera::Pose>();
    auto cam_intrinsics = camera_cpy.get<core::view::Camera::OrthographicParameters>();
    float world_x, world_y;
    world_x = ((x * 2.0f / static_cast<float>(viewRes.x)) - 1.0f);
    world_y = 1.0f - (y * 2.0f / static_cast<float>(viewRes.y));
    world_x = world_x * 0.5f * cam_intrinsics.frustrum_height * cam_intrinsics.aspect + cam_pose.position.x;
    world_y = world_y * 0.5f * cam_intrinsics.frustrum_height + cam_pose.position.y;

    this->mouseX = world_x;
    this->mouseY = world_y;

    if (this->ctrlDown) {
        // these clicks go to the view
        return false;
    }

    if (this->interactionState == InteractionState::INTERACTION_DRAG) {
        int currAxis = mouseXtoAxis(this->mouseX);
        if (currAxis != this->pickedAxis && currAxis >= 0 && currAxis < static_cast<int>(this->columnCount)) {
            auto pickedAxisIt = std::find(this->axisIndirection.begin(), this->axisIndirection.end(), this->pickedAxis);
            int pickedIdx = std::distance(this->axisIndirection.begin(), pickedAxisIt);
            this->axisIndirection.erase(pickedAxisIt);

            auto currAxisIt = std::find(this->axisIndirection.begin(), this->axisIndirection.end(), currAxis);
            int currIdx = std::distance(this->axisIndirection.begin(), currAxisIt);
            if (pickedIdx <= currIdx) {
                currAxisIt++;
            }
            this->axisIndirection.insert(currAxisIt, this->pickedAxis);

            this->needFlagsUpdate = true;
        }

        return true;
    }

    if (this->interactionState == InteractionState::INTERACTION_FILTER) {
        float val = ((this->mouseY - this->marginY) / this->axisHeight) *
                        (maximums[this->pickedIndicatorAxis] - minimums[this->pickedIndicatorAxis]) +
                    minimums[this->pickedIndicatorAxis];
        if (this->pickedIndicatorIndex == 0) {
            val = std::clamp(val, minimums[this->pickedIndicatorAxis], maximums[this->pickedIndicatorAxis]);
            this->filters[this->pickedIndicatorAxis].lower = val;
        } else {
            val = std::clamp(val, minimums[this->pickedIndicatorAxis], maximums[this->pickedIndicatorAxis]);
            this->filters[this->pickedIndicatorAxis].upper = val;
        }
        this->needFlagsUpdate = true;

        return true;
    }

    if (this->interactionState == InteractionState::INTERACTION_SELECT) {
        if (this->mouseX != this->strokeEndX || this->mouseY != this->strokeEndY) {
            this->strokeEndX = this->mouseX;
            this->strokeEndY = this->mouseY;
            this->needSelectionUpdate = true;
        }

        return true;
    }

    return false;
}

bool ParallelCoordinatesRenderer2D::OnKey(
    core::view::Key key, core::view::KeyAction action, core::view::Modifiers mods) {
    ctrlDown = mods.test(core::view::Modifier::CTRL);
    altDown = mods.test(core::view::Modifier::ALT);
    shiftDown = mods.test(core::view::Modifier::SHIFT);
    return false;
}

void ParallelCoordinatesRenderer2D::drawAxes(glm::mat4 ortho) {
    debugPush(1, "drawAxes");
    if (this->columnCount > 0) {
        this->enableProgramAndBind(this->drawAxesProgram);
        glUniform4fv(this->drawAxesProgram->getUniformLocation("color"), 1,
            this->axesColorSlot.Param<core::param::ColorParam>()->Value().data());
        glUniform1i(this->drawAxesProgram->getUniformLocation("pickedAxis"), pickedAxis);
        glUniform1i(this->drawAxesProgram->getUniformLocation("width"), viewRes.x);
        glUniform1i(this->drawAxesProgram->getUniformLocation("height"), viewRes.y);
        glUniform1f(this->drawAxesProgram->getUniformLocation("axesThickness"),
            axesLineThicknessSlot.Param<core::param::FloatParam>()->Value());
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, this->columnCount);
        glUseProgram(0);

        this->enableProgramAndBind(this->drawScalesProgram);
        glUniform4fv(this->drawScalesProgram->getUniformLocation("color"), 1,
            this->axesColorSlot.Param<core::param::ColorParam>()->Value().data());
        glUniform1ui(this->drawScalesProgram->getUniformLocation("numTicks"), this->numTicks);
        glUniform1f(this->drawScalesProgram->getUniformLocation("axisHalfTick"), 2.0f);
        glUniform1i(this->drawScalesProgram->getUniformLocation("pickedAxis"), pickedAxis);
        glUniform1i(this->drawScalesProgram->getUniformLocation("width"), viewRes.x);
        glUniform1i(this->drawScalesProgram->getUniformLocation("height"), viewRes.y);
        glUniform1f(this->drawScalesProgram->getUniformLocation("axesThickness"),
            axesLineThicknessSlot.Param<core::param::FloatParam>()->Value());
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, this->columnCount * this->numTicks);
        glUseProgram(0);

        this->enableProgramAndBind(this->drawFilterIndicatorsProgram);
        glUniform4fv(this->drawFilterIndicatorsProgram->getUniformLocation("color"), 1,
            this->filterIndicatorColorSlot.Param<core::param::ColorParam>()->Value().data());
        glUniform1f(this->drawFilterIndicatorsProgram->getUniformLocation("axisHalfTick"), 2.0f);
        glUniform2i(this->drawFilterIndicatorsProgram->getUniformLocation("pickedIndicator"), pickedIndicatorAxis,
            pickedIndicatorIndex);
        glUniform1i(this->drawFilterIndicatorsProgram->getUniformLocation("width"), viewRes.x);
        glUniform1i(this->drawFilterIndicatorsProgram->getUniformLocation("height"), viewRes.y);
        glUniform1f(this->drawFilterIndicatorsProgram->getUniformLocation("axesThickness"),
            axesLineThicknessSlot.Param<core::param::FloatParam>()->Value());
        glDrawArraysInstanced(GL_TRIANGLES, 0, 12, this->columnCount * 2);
        glUseProgram(0);
        float red[4] = {1.0f, 0.0f, 0.0f, 1.0f};
        const float* color;
#ifndef REMOVE_TEXT
        glActiveTexture(GL_TEXTURE0);
        font.ClearBatchDrawCache();
        for (unsigned int c = 0; c < this->columnCount; c++) {
            unsigned int realCol = this->axisIndirection[c];
            if (this->pickedAxis == realCol) {
                color = red;
            } else {
                color = this->axesColorSlot.Param<core::param::ColorParam>()->Value().data();
            }
            float x = this->marginX + this->axisDistance * c;
#if 0
            this->font.DrawString(ortho, color, x, this->marginY * 0.5f                   , fontSize, false, std::to_string(minimums[realCol]).c_str(), vislib::graphics::AbstractFont::ALIGN_CENTER_MIDDLE);
            this->font.DrawString(ortho, color, x, this->marginY * 1.5f + this->axisHeight, fontSize, false, std::to_string(maximums[realCol]).c_str(), vislib::graphics::AbstractFont::ALIGN_CENTER_MIDDLE);
#else
            float bottom = filters[realCol].lower;
            // bottom *= (maximums[realCol] - minimums[realCol]);
            // bottom += minimums[realCol];
            float top = filters[realCol].upper;
            // top *= (maximums[realCol] - minimums[realCol]);
            // top += minimums[realCol];
            this->font.DrawString(ortho, color, x, this->marginY * 0.5f, fontSize, false,
                std::to_string(bottom).c_str(), core::utility::SDFFont::ALIGN_CENTER_MIDDLE);
            this->font.DrawString(ortho, color, x, this->marginY * 1.5f + this->axisHeight, fontSize, false,
                std::to_string(top).c_str(), core::utility::SDFFont::ALIGN_CENTER_MIDDLE);
#endif
            this->font.DrawString(ortho, color, x,
                this->marginY * (2.0f + static_cast<float>(c % 2) * 0.5f) + this->axisHeight, fontSize * 2.0f, false,
                names[realCol].c_str(), core::utility::SDFFont::ALIGN_CENTER_MIDDLE);
        }
        this->font.BatchDrawString(ortho);
#endif
    }
    debugPop();
}

void ParallelCoordinatesRenderer2D::drawDiscrete(
    const float otherColor[4], const float selectedColor[4], float tfColorFactor, glm::ivec2 const& viewRes) {

    constexpr core::FlagStorageTypes::flag_item_type testMask = core::FlagStorageTypes::to_integral(
        core::FlagStorageTypes::flag_bits::ENABLED | core::FlagStorageTypes::flag_bits::FILTERED |
        core::FlagStorageTypes::flag_bits::SELECTED);

    if (this->drawOtherItemsSlot.Param<core::param::BoolParam>()->Value()) {
        constexpr core::FlagStorageTypes::flag_item_type passMask =
            core::FlagStorageTypes::to_integral(core::FlagStorageTypes::flag_bits::ENABLED);
        this->drawItemsDiscrete(testMask, passMask, otherColor, tfColorFactor, viewRes);
    }
    if (this->drawSelectedItemsSlot.Param<core::param::BoolParam>()->Value()) {
        constexpr core::FlagStorageTypes::flag_item_type passMask = core::FlagStorageTypes::to_integral(
            core::FlagStorageTypes::flag_bits::ENABLED | core::FlagStorageTypes::flag_bits::SELECTED);
        this->drawItemsDiscrete(testMask, passMask, selectedColor, tfColorFactor, viewRes);
    }
}

void ParallelCoordinatesRenderer2D::drawItemsDiscrete(
    uint32_t testMask, uint32_t passMask, const float color[4], float tfColorFactor, glm::ivec2 const& viewRes) {
    auto tf = this->getTFSlot.CallAs<megamol::core_gl::view::CallGetTransferFunctionGL>();
    if (tf == nullptr)
        return;

    debugPush(2, "drawItemsDiscrete");

#ifdef FUCK_THE_PIPELINE
    std::unique_ptr<glowl::GLSLProgram>& prog = this->traceItemsDiscreteProgram;
#else
#ifdef USE_TESSELLATION
    std::unique_ptr<glowl::GLSLProgram>& prog = this->drawItemsDiscreteTessProgram;
#else
    std::unique_ptr<glowl::GLSLProgram>& prog = this->triangleModeSlot.Param<core::param::BoolParam>()->Value()
                                                    ? this->drawItemsTriangleProgram
                                                    : this->drawItemsDiscreteProgram;
#endif
#endif

    this->enableProgramAndBind(prog);
    tf->BindConvenience(prog, GL_TEXTURE5, 5);
    glUniform4fv(prog->getUniformLocation("color"), 1, color);
    glUniform1f(prog->getUniformLocation("tfColorFactor"), tfColorFactor);
    glUniform1i(prog->getUniformLocation("widthR"), viewRes.x);
    glUniform1i(prog->getUniformLocation("heightR"), viewRes.y);
    try {
        auto colcol = this->columnIndex.at(this->otherItemsAttribSlot.Param<core::param::FlexEnumParam>()->Value());
        glUniform1i(prog->getUniformLocation("colorColumn"), colcol);
    } catch (std::out_of_range& ex) {
        // megamol::core::utility::log::Log::DefaultLog.WriteError(
        //    "ParallelCoordinatesRenderer2D: tried to color lines by non-existing column '%s'",
        //    this->otherItemsAttribSlot.Param<core::param::FlexEnumParam>()->Value().c_str());
        glUniform1i(prog->getUniformLocation("colorColumn"), -1);
    }
    glUniform1ui(prog->getUniformLocation("fragmentTestMask"), testMask);
    glUniform1ui(prog->getUniformLocation("fragmentPassMask"), passMask);

    glUniform1f(prog->getUniformLocation("thicknessP"), lineThicknessSlot.Param<core::param::FloatParam>()->Value());

    glEnable(GL_CLIP_DISTANCE0);
#ifdef FUCK_THE_PIPELINE
    glDrawArrays(GL_TRIANGLES, 0, 6 * ((this->itemCount / 128) + 1));
#else
#ifdef USE_TESSELLATION
    glUniform1i(prog->getUniformLocation("isoLinesPerInvocation"), isoLinesPerInvocation);
    glPatchParameteri(GL_PATCH_VERTICES, 1);
    glDrawArrays(GL_PATCHES, 0, (this->itemCount / isoLinesPerInvocation) + 1);
#else
    // glDrawArraysInstanced(GL_LINE_STRIP, 0, this->columnCount, this->itemCount);
    // glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, this->columnCount * 2, this->itemCount);
    if (this->triangleModeSlot.Param<core::param::BoolParam>()->Value()) {
        glDrawArrays(GL_TRIANGLES, 0, (this->columnCount - 1) * 6 * this->itemCount);
    } else {
        glDrawArrays(GL_LINES, 0, (this->columnCount - 1) * 2 * this->itemCount);
    }

#endif
#endif
    glUseProgram(0);
    glDisable(GL_CLIP_DISTANCE0);
    debugPop();
}

void ParallelCoordinatesRenderer2D::drawPickIndicator(float x, float y, float pickRadius, const float color[4]) {
    auto& program = this->drawPickIndicatorProgram;

    this->enableProgramAndBind(program);

    glUniform2f(program->getUniformLocation("mouse"), x, y);
    glUniform1f(program->getUniformLocation("pickRadius"), pickRadius);

    glUniform4fv(program->getUniformLocation("indicatorColor"), 1, color);
    glDisable(GL_DEPTH_TEST);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glUseProgram(0);
}

void ParallelCoordinatesRenderer2D::drawStrokeIndicator(float x0, float y0, float x1, float y1, const float color[4]) {
    auto& prog = this->drawStrokeIndicatorProgram;

    this->enableProgramAndBind(prog);

    glUniform2f(prog->getUniformLocation("mousePressed"), x0, y0);
    glUniform2f(prog->getUniformLocation("mouseReleased"), x1, y1);

    glUniform1i(prog->getUniformLocation("width"), viewRes.x);
    glUniform1i(prog->getUniformLocation("height"), viewRes.y);
    glUniform1f(prog->getUniformLocation("thickness"), axesLineThicknessSlot.Param<core::param::FloatParam>()->Value());

    glUniform4fv(prog->getUniformLocation("indicatorColor"), 1, color);
    glDisable(GL_DEPTH_TEST);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glUseProgram(0);
}

void ParallelCoordinatesRenderer2D::doPicking(float x, float y, float pickRadius) {
    debugPush(3, "doPicking");

    this->enableProgramAndBind(pickProgram);

    glUniform2f(pickProgram->getUniformLocation("mouse"), x, y);
    glUniform1f(pickProgram->getUniformLocation("pickRadius"), pickRadius);

    GLuint groupCounts[3];
    computeDispatchSizes(itemCount, pickWorkgroupSize, maxWorkgroupCount, groupCounts);

    glDispatchCompute(groupCounts[0], groupCounts[1], groupCounts[2]);
    ::glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glUseProgram(0);
    debugPop();
}

void ParallelCoordinatesRenderer2D::doStroking(float x0, float y0, float x1, float y1) {
    debugPush(3, "doStroking");

    this->enableProgramAndBind(strokeProgram);

    glUniform2f(strokeProgram->getUniformLocation("mousePressed"), x0, y0);
    glUniform2f(strokeProgram->getUniformLocation("mouseReleased"), x1, y1);

    GLuint groupCounts[3];
    computeDispatchSizes(itemCount, strokeWorkgroupSize, maxWorkgroupCount, groupCounts);

    glDispatchCompute(groupCounts[0], groupCounts[1], groupCounts[2]);
    ::glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glUseProgram(0);
    debugPop();
}

void ParallelCoordinatesRenderer2D::doFragmentCount(void) {
    debugPush(4, "doFragmentCount");
    int invocations[] = {
        static_cast<int>(std::ceil(fbo->getWidth() / 16)), static_cast<int>(std::ceil(fbo->getHeight() / 16))};
    GLuint invocationCount = invocations[0] * invocations[1];

    size_t bytes = sizeof(uint32_t) * 2 * invocationCount;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, counterBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, bytes, nullptr, GL_STATIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, counterBuffer);

    makeDebugLabel(GL_BUFFER, DEBUG_NAME(counterBuffer));

    glActiveTexture(GL_TEXTURE1);
    densityFBO.BindColourTexture();

    GLuint groupCounts[3] = {
        static_cast<GLuint>((std::max)(1.0f, std::ceil(float(invocations[0]) / counterWorkgroupSize[0]))),
        static_cast<GLuint>((std::max)(1.0f, std::ceil(float(invocations[1]) / counterWorkgroupSize[1]))), 1};

    this->enableProgramAndBind(minMaxProgram);

    // uniforms invocationcount etc.
    glUniform1ui(minMaxProgram->getUniformLocation("invocationCount"), invocationCount);
    glUniform4fv(minMaxProgram->getUniformLocation("clearColor"), 1, backgroundColor);
    glUniform2ui(minMaxProgram->getUniformLocation("resolution"), fbo->getWidth(), fbo->getHeight());
    glUniform2ui(minMaxProgram->getUniformLocation("fragmentCountStepSize"), invocations[0], invocations[1]);


    glDispatchCompute(groupCounts[0], groupCounts[1], groupCounts[2]);

    glUseProgram(0);

    // todo read back minmax and check for plausibility!
    debugPop();
}

void ParallelCoordinatesRenderer2D::drawItemsContinuous(void) {
    auto tf = this->getTFSlot.CallAs<megamol::core_gl::view::CallGetTransferFunctionGL>();
    if (tf == nullptr)
        return;
    debugPush(6, "drawItemsContinuous");
    doFragmentCount();
    this->enableProgramAndBind(drawItemContinuousProgram);
    // glUniform2f(drawItemContinuousProgram.ParameterLocation("bottomLeft"), 0.0f, 0.0f);
    // glUniform2f(drawItemContinuousProgram.ParameterLocation("topRight"), windowWidth, windowHeight);
    densityFBO.BindColourTexture();
    // megamol::core::utility::log::Log::DefaultLog.WriteInfo("setting tf range to [%f, %f]", tf->Range()[0],
    // tf->Range()[1]);
    tf->BindConvenience(drawItemContinuousProgram, GL_TEXTURE5, 5);
    glUniform1i(this->drawItemContinuousProgram->getUniformLocation("fragmentCount"), 1);
    glUniform4fv(this->drawItemContinuousProgram->getUniformLocation("clearColor"), 1, backgroundColor);
    glUniform1i(this->drawItemContinuousProgram->getUniformLocation("sqrtDensity"),
        this->sqrtDensitySlot.Param<core::param::BoolParam>()->Value() ? 1 : 0);
    glEnable(GL_CLIP_DISTANCE0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glUseProgram(0);
    glDisable(GL_CLIP_DISTANCE0);
    debugPop();
}

void ParallelCoordinatesRenderer2D::drawItemsHistogram(void) {
    debugPush(7, "drawItemsHistogram");
    doFragmentCount();
    this->enableProgramAndBind(drawItemsHistogramProgram);
    glActiveTexture(GL_TEXTURE1);
    densityFBO.BindColourTexture();
    glUniform4fv(this->drawItemContinuousProgram->getUniformLocation("clearColor"), 1, backgroundColor);
    glEnable(GL_CLIP_DISTANCE0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glUseProgram(0);
    glDisable(GL_CLIP_DISTANCE0);
    debugPop();
}

void ParallelCoordinatesRenderer2D::drawParcos(glm::ivec2 const& viewRes) {

    // TODO only when filters changed!
    GLuint groups = this->itemCount / (filterWorkgroupSize[0] * filterWorkgroupSize[1] * filterWorkgroupSize[2]);
    GLuint groupCounts[3] = {(groups % maxWorkgroupCount[0]) + 1, (groups / maxWorkgroupCount[0]) + 1, 1};
    this->enableProgramAndBind(this->filterProgram);
    glDispatchCompute(groupCounts[0], groupCounts[1], groupCounts[2]);
    ::glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    const float red[] = {1.0f, 0.0f, 0.0f, 1.0};
    const float moreRed[] = {10.0f, 0.0f, 0.0f, 1.0};

    auto drawmode = this->drawModeSlot.Param<core::param::EnumParam>()->Value();

    switch (drawmode) {
    case DRAW_DISCRETE:
        this->drawDiscrete(this->otherItemsColorSlot.Param<core::param::ColorParam>()->Value().data(),
            this->selectedItemsColorSlot.Param<core::param::ColorParam>()->Value().data(), 1.0f, viewRes);
        break;
    case DRAW_CONTINUOUS:
    case DRAW_HISTOGRAM:
        bool ok = true;
        if (!this->densityFBO.IsValid() || this->densityFBO.GetWidth() != fbo->getWidth() ||
            this->densityFBO.GetHeight() != fbo->getHeight()) {
            densityFBO.Release();
            ok = densityFBO.Create(fbo->getWidth(), fbo->getHeight(), GL_R32F, GL_RED, GL_FLOAT);
            makeDebugLabel(GL_TEXTURE, densityFBO.GetColourTextureID(), "densityFBO");
        }
        if (ok) {
            densityFBO.Enable();
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            //::glDisable(GL_ALPHA_TEST);
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            glBlendEquation(GL_FUNC_ADD);
            this->drawDiscrete(red, moreRed, 0.0f, viewRes);
            densityFBO.Disable();
            glDisable(GL_BLEND);

            if (drawmode == DRAW_CONTINUOUS) {
                this->drawItemsContinuous();
            } else if (drawmode == DRAW_HISTOGRAM) {
                this->drawItemsHistogram();
            }

        } else {
            megamol::core::utility::log::Log::DefaultLog.WriteError("could not create FBO");
        }
        break;
    }
}

void ParallelCoordinatesRenderer2D::store_filters() {
    nlohmann::json jf, jf_array;
    for (auto& f : this->filters) {
        DimensionFilter::to_json(jf, f);
        jf_array.push_back(jf);
    }
    auto js = jf_array.dump();
    this->filterStateSlot.Param<core::param::StringParam>()->SetValue(js.c_str());
}

void ParallelCoordinatesRenderer2D::load_filters() {
    try {
        auto j = nlohmann::json::parse(this->filterStateSlot.Param<core::param::StringParam>()->Value());
        int i = 0;
        for (auto& f : j) {
            if (i < this->filters.size()) {
                DimensionFilter::from_json(f, this->filters[i]);
            } else {
                break;
            }
            i++;
        }
    } catch (nlohmann::json::exception e) {
        megamol::core::utility::log::Log::DefaultLog.WriteError(
            "ParallelCoordinatesRenderer2D: could not parse serialized filters (exception %i)!", e.id);
        return;
    }
}

bool ParallelCoordinatesRenderer2D::Render(core_gl::view::CallRender2DGL& call) {

    // get camera
    core::view::Camera cam = call.GetCamera();
    auto view = cam.getViewMatrix();
    auto proj = cam.getProjectionMatrix();
    auto cam_intrinsics =
        cam.get<core::view::Camera::OrthographicParameters>(); // don't you dare using a perspective cam here...
    fbo = call.GetFramebuffer();
    camera_cpy = cam;
    viewRes = call.GetViewResolution();

    // maintainer comment: assuming this wants to know the aspect of the full window, i.e. not onlyof the current image
    // tile


    // maintainer comment: assuming this now here wants to know about the current tile's resolution


    auto bg = call.BackgroundColor();

    backgroundColor[0] = bg[0];
    backgroundColor[1] = bg[1];
    backgroundColor[2] = bg[2];
    backgroundColor[3] = bg[3];

    glm::mat4 ortho = proj * view;

    if (!this->assertData(call)) {
        return false;
    }

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    if (this->filterStateSlot.IsDirty()) {
        load_filters();
        this->filterStateSlot.ResetDirty();
        this->needFlagsUpdate = true;
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, axisIndirectionBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, this->columnCount * sizeof(GLuint), axisIndirection.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, filtersBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, this->columnCount * sizeof(DimensionFilter), this->filters.data());

    // Do stroking/picking
    if (this->needSelectionUpdate) {
        this->needSelectionUpdate = false;

        switch (selectionModeSlot.Param<core::param::EnumParam>()->Value()) {
        case SELECT_STROKE:
            this->doStroking(this->strokeStartX, this->strokeStartY, this->strokeEndX, this->strokeEndY);
            break;
        case SELECT_PICK:
            this->doPicking(this->strokeEndX, this->strokeEndY,
                this->pickRadiusSlot.Param<megamol::core::param::FloatParam>()->Value());
            break;
        }

        this->needFlagsUpdate = true;
    }
    drawParcos(call.GetViewResolution());

    // Draw stroking/picking indicator
    if (this->drawSelectionIndicatorSlot.Param<core::param::BoolParam>()->Value()) {
        switch (selectionModeSlot.Param<core::param::EnumParam>()->Value()) {
        case SELECT_STROKE:
            if (this->interactionState == InteractionState::INTERACTION_SELECT) {
                this->drawStrokeIndicator(this->strokeStartX, this->strokeStartY, this->strokeEndX, this->strokeEndY,
                    this->selectionIndicatorColorSlot.Param<core::param::ColorParam>()->Value().data());
            }
            break;
        case SELECT_PICK:
            this->drawPickIndicator(this->mouseX, this->mouseY,
                this->pickRadiusSlot.Param<megamol::core::param::FloatParam>()->Value(),
                this->selectionIndicatorColorSlot.Param<core::param::ColorParam>()->Value().data());
            break;
        }
    }

    if (needFlagsUpdate) {
        needFlagsUpdate = false;
        this->store_filters();

        this->currentFlagsVersion++;
        // HAZARD: downloading everything over and over is slowish
        auto readFlags = readFlagsSlot.CallAs<core_gl::FlagCallRead_GL>();
        auto writeFlags = writeFlagsSlot.CallAs<core_gl::FlagCallWrite_GL>();
        if (readFlags != nullptr && writeFlags != nullptr) {
            writeFlags->setData(readFlags->getData(), this->currentFlagsVersion);
            (*writeFlags)(core_gl::FlagCallWrite_GL::CallGetData);
#if 0
            auto flags = readFlags->getData()->flags;
            std::vector<core::FlagStorage::FlagItemType> f(flags->getByteSize()/sizeof(core::FlagStorage::FlagItemType));
            flags->bind();
            glGetBufferSubData(
                GL_SHADER_STORAGE_BUFFER, 0, flags->getByteSize(), f.data());

            core::FlagStorage::FlagVectorType::size_type numFiltered = 0, numEnabled = 0, numSelected = 0,
                                                         numSoftSelected = 0;
            for (unsigned int& i : f) {
                if ((i & core::FlagStorage::FILTERED) > 0) numFiltered++;
                if ((i & core::FlagStorage::ENABLED) > 0) numEnabled++;
                if ((i & core::FlagStorage::SELECTED) > 0) numSelected++;
                if ((i & core::FlagStorage::SOFTSELECTED) > 0) numSoftSelected++;
            }
            megamol::core::utility::log::Log::DefaultLog.WriteInfo(
                "ParallelCoordinateRenderer2D: %lu items: %lu enabled, %lu filtered, %lu selected, %lu "
                "soft-selected.",
                f.size(), numEnabled, numFiltered, numSelected, numSoftSelected);
#endif
        }
    }


    if (this->drawAxesSlot.Param<core::param::BoolParam>()->Value()) {
        drawAxes(ortho);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glDepthMask(GL_TRUE);

    return true;
}
