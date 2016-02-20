/*
 * Copyright (C) 2016 Muhammad Tayyab Akram
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <SFConfig.h>
#include <SFTypes.h>

#include <stddef.h>
#include <stdlib.h>

#include "SFAssert.h"
#include "SFAlbum.h"
#include "SFCommon.h"
#include "SFData.h"
#include "SFGDEF.h"
#include "SFLocator.h"
#include "SFOpenType.h"

#include "SFGlyphManipulation.h"
#include "SFGlyphPositioning.h"
#include "SFGlyphSubstitution.h"
#include "SFShapingEngine.h"

static SFBoolean _SFApplyChainContextF3(SFTextProcessorRef processor, SFData chainContext);
static void _SFApplyContextRecord(SFTextProcessorRef processor, SFData contextRecord, SFUInteger startIndex, SFUInteger endIndex);

SF_PRIVATE SFBoolean _SFApplyExtensionSubtable(SFTextProcessorRef processor, SFData extensionSubtable)
{
    SFUInt16 format = SFExtension_Format(extensionSubtable);

    switch (format) {
    case 1:
        {
            SFLookupType lookupType = SFExtensionF1_LookupType(extensionSubtable);
            SFUInt32 offset = SFExtensionF1_ExtensionOffset(extensionSubtable);
            SFData innerSubtable = SFData_Subdata(extensionSubtable, offset);

            switch (processor->_featureKind) {
            case SFFeatureKindSubstitution:
                return _SFApplySubstitutionSubtable(processor, lookupType, innerSubtable);

            case SFFeatureKindPositioning:
                return _SFApplyPositioningSubtable(processor, lookupType, innerSubtable);
            }
        }
        break;
    }

    return SFFalse;
}

SF_PRIVATE SFBoolean _SFApplyChainContextSubtable(SFTextProcessorRef processor, SFData chainContext)
{
    SFUInt16 format = SFChainContext_Format(chainContext);

    switch (format) {
    case 3:
        return _SFApplyChainContextF3(processor, chainContext);
    }

    return SFFalse;
}

static SFBoolean _SFApplyChainContextF3(SFTextProcessorRef processor, SFData chainContext)
{
    SFData backtrackRecord = SFChainContextF3_BacktrackRecord(chainContext);
    SFUInt16 backtrackCount = SFBacktrackRecord_GlyphCount(backtrackRecord);
    SFData inputRecord = SFBacktrackRecord_InputRecord(backtrackRecord, backtrackCount);
    SFUInt16 inputCount = SFInputRecord_GlyphCount(inputRecord);
    SFData lookaheadRecord = SFInputRecord_LookaheadRecord(inputRecord, inputCount);
    SFUInt16 lookaheadCount = SFInputRecord_GlyphCount(lookaheadRecord);
    SFData contextRecord = SFLookaheadRecord_ContextRecord(lookaheadRecord, lookaheadCount);

    /* Make sure that input record has at least one input glyph. */
    if (inputCount > 0) {
        SFAlbumRef album = processor->_album;
        SFLocatorRef locator = &processor->_locator;
        SFGlyphID inputGlyph;
        SFOffset offset;
        SFData coverage;
        SFUInteger coverageIndex;

        offset = SFInputRecord_Value(inputRecord, 0);
        coverage = SFData_Subdata(chainContext, offset);

        inputGlyph = SFAlbumGetGlyph(album, locator->index);
        coverageIndex = SFOpenTypeSearchCoverageIndex(coverage, inputGlyph);

        /* Proceed if first glyph exists in coverage. */
        if (coverageIndex != SFInvalidIndex) {
            SFUInteger backtrackIndex;
            SFUInteger inputIndex;
            SFUInteger lookaheadIndex;
            SFUInteger recordIndex;

            inputIndex = locator->index;

            /* Match the input glyphs. */
            for (recordIndex = 1; recordIndex < inputCount; recordIndex++) {
                inputIndex = SFLocatorGetAfter(locator, inputIndex);

                if (inputIndex != SFInvalidIndex) {
                    offset = SFInputRecord_Value(inputRecord, recordIndex);
                    coverage = SFData_Subdata(chainContext, offset);

                    inputGlyph = SFAlbumGetGlyph(album, inputIndex);
                    coverageIndex = SFOpenTypeSearchCoverageIndex(coverage, inputGlyph);

                    if (coverageIndex == SFInvalidIndex) {
                        goto NotMatched;
                    }
                } else {
                    goto NotMatched;
                }
            }

            backtrackIndex = locator->index;

            /* Match the backtrack glyphs. */
            for (recordIndex = 0; recordIndex < backtrackCount; recordIndex++) {
                backtrackIndex = SFLocatorGetBefore(locator, backtrackIndex);

                if (backtrackIndex != SFInvalidIndex) {
                    offset = SFBacktrackRecord_Value(backtrackRecord, recordIndex);
                    coverage = SFData_Subdata(chainContext, offset);

                    inputGlyph = SFAlbumGetGlyph(album, backtrackIndex);
                    coverageIndex = SFOpenTypeSearchCoverageIndex(coverage, inputGlyph);

                    if (coverageIndex == SFInvalidIndex) {
                        goto NotMatched;
                    }
                } else {
                    goto NotMatched;
                }
            }

            lookaheadIndex = inputIndex;

            /* Match the lookahead glyphs. */
            for (recordIndex = 0; recordIndex < lookaheadCount; recordIndex++) {
                lookaheadIndex = SFLocatorGetAfter(locator, lookaheadIndex);

                if (lookaheadIndex != SFInvalidIndex) {
                    offset = SFLookaheadRecord_Value(lookaheadRecord, recordIndex);
                    coverage = SFData_Subdata(chainContext, offset);

                    inputGlyph = SFAlbumGetGlyph(album, lookaheadIndex);
                    coverageIndex = SFOpenTypeSearchCoverageIndex(coverage, inputGlyph);

                    if (coverageIndex == SFInvalidIndex) {
                        goto NotMatched;
                    }
                } else {
                    goto NotMatched;
                }
            }

            _SFApplyContextRecord(processor, contextRecord, locator->index, (inputIndex - locator->index) + 1);
            return SFTrue;
        }
    }

NotMatched:
    return SFFalse;
}

static void _SFApplyContextRecord(SFTextProcessorRef processor, SFData contextRecord, SFUInteger index, SFUInteger count) {
    SFLocator originalLocator = processor->_locator;
    SFUInt16 lookupCount;
    SFUInteger lookupIndex;

    lookupCount = SFContextRecord_LookupCount(contextRecord);

    for (lookupIndex = 0; lookupIndex < lookupCount; lookupIndex++) {
        SFData lookupRecord = SFContextRecord_LookupRecord(contextRecord, lookupIndex);
        SFUInt16 sequenceIndex = SFLookupRecord_SequenceIndex(lookupRecord);
        SFUInt16 lookupListIndex = SFLookupRecord_LookupListIndex(lookupRecord);

        /* Make the locator cover only context range. */
        SFLocatorReset(&processor->_locator, index, count);
        /* Skip the glyphs till sequence index and apply the lookup. */
        if (SFLocatorSkip(&processor->_locator, sequenceIndex)) {
            _SFApplyLookup(processor, lookupListIndex);
        }
    }

    /* Take the state of context locator so that input glyphs are skipped properly. */
    SFLocatorTakeState(&originalLocator, &processor->_locator);
    /* Switch back to the original locator. */
    processor->_locator = originalLocator;
}
