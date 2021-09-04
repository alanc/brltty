#!/bin/bash
###############################################################################
# BRLTTY - A background process providing access to the console screen (when in
#          text mode) for a blind person using a refreshable braille display.
#
# Copyright (C) 1995-2021 by The BRLTTY Developers.
#
# BRLTTY comes with ABSOLUTELY NO WARRANTY.
#
# This is free software, placed under the terms of the
# GNU Lesser General Public License, as published by the Free Software
# Foundation; either version 2.1 of the License, or (at your option) any
# later version. Please see the file LICENSE-LGPL for details.
#
# Web Page: http://brltty.app/
#
# This software is maintained by Dave Mielke <dave@mielke.cc>.
###############################################################################

. "$(dirname "${BASH_SOURCE[0]}")/brltty-prologue.sh"

setSourceRoot() {
   findContainingDirectory BRLTTY_SOURCE_ROOT "${programDirectory}" brltty.pc.in || {
      semanticError "source tree not found"
   }
}

setBuildRoot() {
   local variable=BRLTTY_BUILD_ROOT
   set -- brltty.pc

   findContainingDirectory "${variable}" "${initialDirectory}" "${@}" || {
      findContainingDirectory "${variable}" "${programDirectory}" "${@}" || {
         semanticError "build tree not found"
      }
   }
}

readonly documentsSubdirectory="Documents"
readonly programsSubdirectory="Programs"

