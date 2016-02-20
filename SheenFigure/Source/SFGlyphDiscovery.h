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

#ifndef _SF_GLYPH_DISCOVERY_INTERNAL_H
#define _SF_GLYPH_DISCOVERY_INTERNAL_H

/*
 * NOTE:
 *      This file is part of SFTextProcessor class whose purpose is to discover character glyphs.
 */

#include <SFConfig.h>
#include <SFTypes.h>

#include "SFAlbum.h"
#include "SFTextProcessor.h"

SF_PRIVATE SFGlyphTraits _SFGetGlyphTraits(SFTextProcessorRef processor, SFGlyphID glyph);
SF_PRIVATE void _SFDiscoverGlyphs(SFTextProcessorRef processor);

#endif
