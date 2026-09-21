// Host-side stand-in for the firmware's app.h, exposing only the pure logic.
#pragma once
#include "Arduino.h"
#include <vector>

#define MAX_ARTICLE_CHARS 60000

String utf8ToAscii(const String &in);
String latin1ToUtf8(const String &in);
String htmlToText(const String &in);
String timeAgo(uint32_t unixSecs);
String domainOf(const String &url);

enum Feed { FEED_TOP = 0, FEED_NEW, FEED_BEST, FEED_ASK, FEED_SHOW, FEED_JOBS, FEED_COUNT };
extern const char *kFeedName[FEED_COUNT];
extern const char *kFeedDesc[FEED_COUNT];

namespace http {
    bool get(const String &url, char **body, size_t *len, String *contentType, String &err,
             size_t maxBytes = 3u * 1024 * 1024);
    void freeBody(char *body);
}
namespace reader { bool fetch(const String &url, String &title, String &body, String &err); }
