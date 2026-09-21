#include "app.h"
#include "json_psram.h"
#include <WiFi.h>

namespace hn {

static const int FILTER_DEPTH = 8;     // how deep the reply tree is kept
// ArduinoJson enforces its nesting limit on the *input* even for subtrees the
// filter discards, so this has to cover the deepest reply chain HN will ever
// hand us, not just FILTER_DEPTH. Each reply level costs two (object + array).
// The parser recurses per level; main.cpp sizes the loop task's stack for it.
static const int NESTING = 128;

// Small RAII wrapper so every early return releases the PSRAM body.
struct Body {
    char *p = nullptr;
    size_t n = 0;
    ~Body() { http::freeBody(p); }
};

static String feedUrl(Feed f) {
    String base = "https://hn.algolia.com/api/v1/";
    switch (f) {
        case FEED_NEW:
            return base + "search_by_date?tags=story&hitsPerPage=" + MAX_STORIES;
        case FEED_BEST: {
            uint32_t since = (uint32_t)time(nullptr);
            since = (since > 90000) ? since - 86400 : 0;
            return base + "search?tags=story&numericFilters=created_at_i>" + since +
                   "&hitsPerPage=" + MAX_STORIES;
        }
        case FEED_ASK:  return base + "search?tags=ask_hn&hitsPerPage=" + MAX_STORIES;
        case FEED_SHOW: return base + "search?tags=show_hn&hitsPerPage=" + MAX_STORIES;
        case FEED_JOBS: return base + "search_by_date?tags=job&hitsPerPage=" + MAX_STORIES;
        case FEED_TOP:
        default:        return base + "search?tags=front_page&hitsPerPage=" + MAX_STORIES;
    }
}

bool fetchFeed(Feed f, std::vector<Story> &out, String &err) {
    Body b;
    if (!http::get(feedUrl(f), &b.p, &b.n, nullptr, err)) return false;

    JsonDocument filter(&gPsram);
    JsonObject hit = filter["hits"].to<JsonArray>().add<JsonObject>();
    hit["objectID"] = true;
    hit["title"] = true;
    hit["url"] = true;
    hit["author"] = true;
    hit["points"] = true;
    hit["num_comments"] = true;
    hit["created_at_i"] = true;
    hit["story_text"] = true;

    JsonDocument doc(&gPsram);
    DeserializationError e = deserializeJson(doc, b.p, b.n,
                                             DeserializationOption::Filter(filter),
                                             DeserializationOption::NestingLimit(NESTING));
    if (e) { err = String("JSON: ") + e.c_str(); return false; }

    out.clear();
    for (JsonObjectConst h : doc["hits"].as<JsonArrayConst>()) {
        Story s;
        s.id       = strtoul(h["objectID"] | "0", nullptr, 10);
        s.title    = utf8ToAscii(String((const char *)(h["title"] | "")));
        s.url      = String((const char *)(h["url"] | ""));
        s.author   = String((const char *)(h["author"] | ""));
        s.points   = h["points"] | 0u;
        s.comments = h["num_comments"] | 0u;
        s.time     = h["created_at_i"] | 0u;
        const char *st = h["story_text"] | "";
        if (st && *st) s.text = htmlToText(String(st));
        if (s.id && s.title.length()) out.push_back(s);
        if (out.size() >= MAX_STORIES) break;
    }
    if (out.empty()) { err = "No stories returned"; return false; }
    return true;
}

// Comment bodies live in Arduino Strings, i.e. internal RAM, so the thread is
// capped by total characters as well as by count.
static size_t gCommentChars = 0;

static void flatten(JsonArrayConst kids, int depth, std::vector<Comment> &out) {
    for (JsonObjectConst c : kids) {
        if (out.size() >= MAX_COMMENTS || gCommentChars > MAX_COMMENT_CHARS) return;
        const char *author = c["author"] | "";
        const char *text   = c["text"] | "";
        bool dead = (!author || !*author) && (!text || !*text);
        if (!dead && text && *text) {
            Comment cm;
            cm.author = String(author);
            cm.text   = htmlToText(String(text));
            cm.depth  = (uint8_t)min(depth, 10);
            cm.time   = c["created_at_i"] | 0u;
            if (cm.text.length() > MAX_ONE_COMMENT)
                cm.text = cm.text.substring(0, MAX_ONE_COMMENT) + " [...]";
            if (cm.text.length()) { gCommentChars += cm.text.length(); out.push_back(cm); }
        }
        JsonArrayConst sub = c["children"].as<JsonArrayConst>();
        if (!sub.isNull()) flatten(sub, depth + 1, out);
    }
}

bool fetchComments(uint32_t storyId, std::vector<Comment> &out, Story &story, String &err) {
    Body b;
    String url = String("https://hn.algolia.com/api/v1/items/") + storyId;
    if (!http::get(url, &b.p, &b.n, nullptr, err)) return false;

    // Build a filter nested FILTER_DEPTH levels deep so only the fields we
    // render survive parsing — a hot thread is otherwise megabytes of JSON.
    JsonDocument filter(&gPsram);
    JsonObject lvl = filter.to<JsonObject>();
    lvl["id"] = true;
    lvl["title"] = true;
    lvl["url"] = true;
    lvl["points"] = true;
    lvl["author"] = true;
    lvl["created_at_i"] = true;
    lvl["text"] = true;
    for (int d = 0; d < FILTER_DEPTH; d++) {
        JsonObject kid = lvl["children"].to<JsonArray>().add<JsonObject>();
        kid["author"] = true;
        kid["text"] = true;
        kid["created_at_i"] = true;
        lvl = kid;
    }

    JsonDocument doc(&gPsram);
    DeserializationError e = deserializeJson(doc, b.p, b.n,
                                             DeserializationOption::Filter(filter),
                                             DeserializationOption::NestingLimit(NESTING));
    if (e) { err = String("JSON: ") + e.c_str(); return false; }

    if (story.title.length() == 0)
        story.title = utf8ToAscii(String((const char *)(doc["title"] | "")));
    if (story.author.length() == 0)
        story.author = String((const char *)(doc["author"] | ""));
    if (story.url.length() == 0)
        story.url = String((const char *)(doc["url"] | ""));
    if (story.points == 0) story.points = doc["points"] | 0u;
    if (story.time == 0)   story.time   = doc["created_at_i"] | 0u;
    const char *body = doc["text"] | "";
    if (body && *body && story.text.length() == 0) story.text = htmlToText(String(body));

    out.clear();
    gCommentChars = 0;
    JsonArrayConst kids = doc["children"].as<JsonArrayConst>();
    if (!kids.isNull()) flatten(kids, 0, out);
    return true;
}

}  // namespace hn
