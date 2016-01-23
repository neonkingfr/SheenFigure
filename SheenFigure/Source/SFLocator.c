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

#include "SFAssert.h"
#include "SFAlbum.h"
#include "SFGDEF.h"
#include "SFOpenType.h"
#include "SFLocator.h"

static SFBoolean _SFIsIgnoredGlyph(SFLocatorRef locator, SFUInteger index);

SF_INTERNAL void SFLocatorInitialize(SFLocatorRef locator, SFAlbumRef album, SFData gdef)
{
    /* Album must NOT be null. */
    SFAssert(album != NULL);

    locator->_album = album;
    locator->_markAttachClassDef = NULL;
    locator->_markGlyphSetsDef = NULL;
    locator->_version = SFInvalidIndex;
    locator->_startIndex = 0;
    locator->_limitIndex = 0;
    locator->_stateIndex = 0;
    locator->index = SFInvalidIndex;
    locator->_ignoreMask.full = 0;
    locator->lookupFlag = 0;

    if (gdef) {
        SFOffset offset = SFGDEF_MarkAttachClassDefOffset(gdef);
        locator->_markAttachClassDef = SFData_Subdata(gdef, offset);

        if (SFGDEF_Version(gdef) == 0x00010002) {
            offset = SFGDEF_MarkGlyphSetsDefOffset(gdef);
            locator->_markGlyphSetsDef = SFData_Subdata(gdef, offset);
        }
    }
}

SF_INTERNAL void SFLocatorReserveGlyphs(SFLocatorRef locator, SFUInteger glyphCount)
{
    /* The album version MUST be same. */
    SFAssert(locator->_version == locator->_album->_version);

    SFAlbumReserveGlyphs(locator->_album, locator->_stateIndex, glyphCount);

    locator->_version = locator->_album->_version;
    locator->_limitIndex += glyphCount;
}

SF_INTERNAL void SFLocatorSetFeatureMask(SFLocatorRef locator, SFUInt16 featureMask)
{
    locator->_ignoreMask.section.featureMask = _SFAlbumGetAntiFeatureMask(featureMask);
}

SF_INTERNAL void SFLocatorSetLookupFlag(SFLocatorRef locator, SFLookupFlag lookupFlag)
{
    SFGlyphTraits glyphTraits = SFGlyphTraitNone;

    if (lookupFlag & SFLookupFlagIgnoreBaseGlyphs) {
        glyphTraits |= SFGlyphTraitBase;
    }

    if (lookupFlag & SFLookupFlagIgnoreLigatures) {
        glyphTraits |= SFGlyphTraitLigature;
    }

    if (lookupFlag & SFLookupFlagIgnoreMarks) {
        glyphTraits |= SFGlyphTraitMark;
    }

    glyphTraits |= SFGlyphTraitRemoved;

    locator->lookupFlag = lookupFlag;
    locator->_ignoreMask.section.glyphTraits = glyphTraits;
}

SF_INTERNAL void SFLocatorReset(SFLocatorRef locator, SFUInteger index, SFUInteger count)
{
    /* The index must be valid and there should be no integer overflow. */
    SFAssert(index <= locator->_album->glyphCount && index <= (index + count));

    locator->_version = locator->_album->_version;
    locator->_startIndex = index;
    locator->_limitIndex = index + count;
    locator->_stateIndex = index;
    locator->index = SFInvalidIndex;
}

static SFBoolean _SFIsIgnoredGlyph(SFLocatorRef locator, SFUInteger index) {
    SFAlbumRef album = locator->_album;
    SFLookupFlag lookupFlag = locator->lookupFlag;
    SFGlyphMask glyphMask = _SFAlbumGetGlyphMask(album, index);

    if (locator->_ignoreMask.full & glyphMask.full) {
        return SFTrue;
    }

    if (lookupFlag & SFLookupFlagMarkAttachmentType) {
        SFGlyphID glyph = SFAlbumGetGlyph(album, index);
        SFUInt16 glyphClass;

        if (locator->_markAttachClassDef
            && (glyphMask.section.glyphTraits & SFGlyphTraitMark)
            && !(SFOpenTypeSearchGlyphClass(locator->_markAttachClassDef, glyph, &glyphClass)
                 && glyphClass == (lookupFlag >> 8))) {
                return SFTrue;
            }
    }

    return SFFalse;
}

SF_INTERNAL SFBoolean SFLocatorMoveNext(SFLocatorRef locator)
{
    /* The state of locator must be valid. */
    SFAssert(locator->_stateIndex <= locator->_limitIndex);
    /* The album version MUST be same. */
    SFAssert(locator->_version == locator->_album->_version);

    while (locator->_stateIndex < locator->_limitIndex) {
        SFUInteger index = locator->_stateIndex++;

        if (!_SFIsIgnoredGlyph(locator, index)) {
            locator->index = index;
            return SFTrue;
        }
    }

    return SFFalse;
}

SF_INTERNAL SFBoolean SFLocatorSkip(SFLocatorRef locator, SFUInteger count)
{
    SFUInteger skipper;

    for (skipper = count; skipper; skipper--) {
        if (SFLocatorMoveNext(locator)) {
            continue;
        }

        return SFFalse;
    }

    return SFTrue;
}

SF_INTERNAL void SFLocatorJumpTo(SFLocatorRef locator, SFUInteger index)
{
    /*
     * The index must be valid.
     *
     * NOTE:
     *      It is legal to jump to limit index so that MoveNext method returns SFFalse thereafter.
     */
    SFAssert(index <= locator->_limitIndex);
    /* The album version MUST be same. */
    SFAssert(locator->_version == locator->_album->_version);

    locator->_stateIndex = index;
}

SF_INTERNAL SFUInteger SFLocatorGetAfter(SFLocatorRef locator, SFUInteger index)
{
    /* The index must be valid. */
    SFAssert(index < locator->_limitIndex);
    /* The album version MUST be same. */
    SFAssert(locator->_version == locator->_album->_version);

    for (index += 1; index < locator->_limitIndex; index++) {
        if (!_SFIsIgnoredGlyph(locator, index)) {
            return index;
        }
    }

    return SFInvalidIndex;
}

SF_INTERNAL SFUInteger SFLocatorGetBefore(SFLocatorRef locator, SFUInteger index)
{
    /* The index must be valid. */
    SFAssert(index < locator->_limitIndex);
    /* The album version MUST be same. */
    SFAssert(locator->_version == locator->_album->_version);

    while (index-- > locator->_startIndex) {
        if (!_SFIsIgnoredGlyph(locator, index)) {
            return index;
        }
    }

    return SFInvalidIndex;
}

SF_INTERNAL void SFLocatorTakeState(SFLocatorRef locator, SFLocatorRef sibling) {
    /* Both of the locators MUST belong to the same album. */
    SFAssert(locator->_album == sibling->_album);
    /* The state of sibling must be valid. */
    SFAssert(sibling->_stateIndex <= locator->_limitIndex);

    locator->_stateIndex = sibling->_stateIndex;
}
