/*
 * SolPathDataSource.h
 *
 * Copyright (C) 2010 by VISUS (University of Stuttgart)
 * Alle Rechte vorbehalten.
 */
#ifndef MEGAMOL_PROTEIN_SOLPATHDATASOURCE_H_INCLUDED
#define MEGAMOL_PROTEIN_SOLPATHDATASOURCE_H_INCLUDED
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */

#include "mmcore/CalleeSlot.h"
#include "mmcore/Module.h"
#include "mmcore/param/ParamSlot.h"
#include "protein/SolPathDataCall.h"
#include "vislib/Array.h"
#include "vislib/math/Cuboid.h"
#include "vislib/sys/File.h"


namespace megamol {
namespace protein {

/**
 * Data source for solvent path data
 */
class SolPathDataSource : public megamol::core::Module {
public:
    /**
     * Answer the name of this module.
     *
     * @return The name of this module.
     */
    static const char* ClassName(void) {
        return "SolPathDataSource";
    }

    /**
     * Answer a human readable description of this module.
     *
     * @return A human readable description of this module.
     */
    static const char* Description(void) {
        return "Data source for solvent path data.";
    }

    /**
     * Answers whether this module is available on the current system.
     *
     * @return 'true' if the module is available, 'false' otherwise.
     */
    static bool IsAvailable(void) {
        return true;
    }

    /** ctor */
    SolPathDataSource(void);

    /** dtor */
    virtual ~SolPathDataSource(void);

protected:
    /**
     * Implementation of 'Create'.
     *
     * @return 'true' on success, 'false' otherwise.
     */
    virtual bool create(void);

    /**
     * Implementation of 'Release'.
     */
    virtual void release(void);

private:
    /**
     * structure of file blocks
     */
    typedef struct fileBlockInfo_t {

        /** id of the block */
        unsigned int id;

        /** starting offset of the block */
        vislib::sys::File::FileOffset start;

        /** size of the block */
        vislib::sys::File::FileSize size;

        /**
         * Test for equality.
         *
         * @param rhs The right hand side operand.
         *
         * @return 'true' if equal, 'false' otherwise.
         */
        inline bool operator==(const struct fileBlockInfo_t& rhs) const {
            return (this->id == rhs.id) && (this->start == rhs.start) && (this->size == rhs.size);
        }

    } fileBlockInfo;

    /**
     * Answer whether any parameter slot is dirty.
     *
     * @return 'true' if any parameter slot is dirty.
     */
    inline bool anyParamslotDirty(void) const {
        return this->filenameslot.IsDirty() || this->smoothSlot.IsDirty() || this->smoothValueSlot.IsDirty() ||
               this->smoothExpSlot.IsDirty() || this->speedOfSmoothedSlot.IsDirty() ||
               this->clusterOfSmoothedSlot.IsDirty();
    }

    /**
     * Sends the data to the caller
     */
    bool getData(megamol::core::Call& call);

    /**
     * Sends data size information to the caller
     */
    bool getExtent(megamol::core::Call& call);

    /**
     * Clears all data
     */
    void clear(void);

    /**
     * Loads the data file
     */
    void loadData(void);

    /** The slot publishing the data */
    megamol::core::CalleeSlot getdataslot;

    /** The path of the solpath file to load */
    megamol::core::param::ParamSlot filenameslot;

    /** Flag whether or not to smooth the data */
    megamol::core::param::ParamSlot smoothSlot;

    /** Value for the smooth filter */
    megamol::core::param::ParamSlot smoothValueSlot;

    /** The smoothing function exponent */
    megamol::core::param::ParamSlot smoothExpSlot;

    /** Flag whether or not to use the smoothed data for the speed calculation */
    megamol::core::param::ParamSlot speedOfSmoothedSlot;

    /** Flag to cluster the smoothed or unsmoothed data */
    megamol::core::param::ParamSlot clusterOfSmoothedSlot;

    /** The bbox */
    vislib::math::Cuboid<float> bbox;

    /** The minimum frame number */
    float minTime;

    /** The maximum frame number */
    float maxTime;

    /** The minimum speed */
    float minSpeed;

    /** The maximum speed */
    float maxSpeed;

    /** The vertex data */
    vislib::Array<SolPathDataCall::Vertex> vertices;

    /** The pathline data */
    vislib::Array<SolPathDataCall::Pathline> pathlines;
};

} /* end namespace protein */
} /* end namespace megamol */

#endif /*  MEGAMOL_PROTEIN_SOLPATHDATASOURCE_H_INCLUDED */
