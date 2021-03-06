/*
 * BRLTTY - A background process providing access to the console screen (when in
 *          text mode) for a blind person using a refreshable braille display.
 *
 * Copyright (C) 1995-2017 by The BRLTTY Developers.
 *
 * BRLTTY comes with ABSOLUTELY NO WARRANTY.
 *
 * This is free software, placed under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any
 * later version. Please see the file LICENSE-GPL for details.
 *
 * Web Page: http://brltty.com/
 *
 * This software is maintained by Dave Mielke <dave@mielke.cc>.
 */

#include "prologue.h"

#include <string.h>

#include "message.h"
#include "defaults.h"
#include "async_wait.h"
#include "async_task.h"
#include "charset.h"
#include "brl_utils.h"
#include "brl_cmds.h"
#include "spk.h"
#include "ktb_types.h"
#include "update.h"
#include "cmd_queue.h"
#include "api_control.h"
#include "core.h"

int messageHoldTimeout = DEFAULT_MESSAGE_HOLD_TIMEOUT;

typedef struct {
  unsigned touch:1;

  int timeout;
  unsigned endWait:1;
} MessageData;

ASYNC_CONDITION_TESTER(testEndMessageWait) {
  MessageData *mgd = data;
  return mgd->endWait;
}

static int
handleMessageCommands (int command, void *data) {
  MessageData *mgd = data;

  switch (command & BRL_MSK_CMD) {
    default: {
      int arg = command & BRL_MSK_ARG;

      switch (command & BRL_MSK_BLK) {
        case BRL_CMD_BLK(TOUCH_AT):
          mgd->touch = arg != BRL_MSK_ARG;
          mgd->timeout = 1000;
          break;

        default:
          mgd->timeout = -1;
          break;
      }

      break;
    }
  }

  if (!mgd->touch) mgd->endWait = 1;
  return 1;
}

typedef struct {
  const char *mode;
  MessageOptions options;
  unsigned presented:1;
  unsigned deallocate:1;
  char text[0];
} MessageParameters;

ASYNC_TASK_CALLBACK(presentMessage) {
  MessageParameters *mgp = data;

#ifdef ENABLE_SPEECH_SUPPORT
  if (!(mgp->options & MSG_SILENT)) {
    if (isAutospeakActive()) {
      sayString(&spk, mgp->text, SAY_OPT_MUTE_FIRST);
    }
  }
#endif /* ENABLE_SPEECH_SUPPORT */

  if (canBraille()) {
    MessageData mgd = {
      .touch = 0
    };

    size_t size = textCount * brl.textRows;
    wchar_t buffer[size];

    size_t length = getTextLength(mgp->text);
    wchar_t characters[length + 1];
    const wchar_t *character = characters;

    int wasLinked = api.isLinked();
    if (wasLinked) api.unlink();

    convertTextToWchars(characters, mgp->text, ARRAY_COUNT(characters));
    suspendUpdates();
    pushCommandEnvironment("message", NULL, NULL);
    pushCommandHandler("message", KTB_CTX_WAITING,
                       handleMessageCommands, NULL, &mgd);

    while (length) {
      size_t count;

      /* strip leading spaces */
      while (iswspace(*character)) {
        character += 1;
        length -= 1;
      }

      if (length <= size) {
        count = length; /* the whole message fits in the braille window */
      } else {
        /* split the message across multiple braille windows on space characters */
        for (count=size-2; count>0 && iswspace(characters[count]); count-=1);
        count = count? count+1: size-1;
      }

      wmemcpy(buffer, character, count);
      character += count;

      if (length -= count) {
        while (count < size) buffer[count++] = WC_C('-');
        buffer[count - 1] = WC_C('>');
      }

      brl.cursor = BRL_NO_CURSOR;
      if (!writeBrailleCharacters(mgp->mode, buffer, count)) {
        mgp->presented = 0;
        break;
      }

      mgd.timeout = messageHoldTimeout - brl.writeDelay;
      drainBrailleOutput(&brl, 0);

      if (length || !(mgp->options & MSG_NODELAY)) {
        mgd.timeout = MAX(mgd.timeout, 0);

        while (1) {
          mgd.endWait = 0;

          if (asyncAwaitCondition(mgd.timeout, testEndMessageWait, &mgd)) {
            if (mgd.timeout < 0) break;
          } else {
            if (!(mgd.touch || (mgp->options & MSG_WAITKEY))) break;
            mgd.timeout = messageHoldTimeout;
          }
        }
      }
    }

    popCommandEnvironment();
    resumeUpdates(1);
    if (wasLinked) api.link();
  }

  if (mgp->deallocate) free(mgp);
}

int 
message (const char *mode, const char *text, MessageOptions options) {
  int presented = 0;
  MessageParameters *mgp;
  size_t size = sizeof(*mgp) + strlen(text);

  if ((mgp = malloc(size))) {
    memset(mgp, 0, size);
    mgp->mode = mode? mode: "";
    mgp->options = options;
    mgp->presented = 1;
    strcpy(mgp->text, text);

    if (mgp->options & MSG_SYNC) {
      mgp->deallocate = 0;
      presentMessage(mgp);
      if (mgp->presented) presented = 1;
    } else {
      mgp->deallocate = 1;
      if (asyncAddTask(NULL, presentMessage, mgp)) return 1;
    }

    free(mgp);
  }

  return presented;
}
