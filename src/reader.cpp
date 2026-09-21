// Reader-mode extraction.  Everything here works on the raw PSRAM buffer:
// an Arduino String holding a whole page would blow the internal heap.
#include "app.h"
#include <ctype.h>

namespace reader {

struct Para { String text; bool heading; };

static bool ieq(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
    return true;
}

// Tags whose entire subtree is page furniture.
static bool isDropTag(const char *n, size_t len) {
    static const char *drop[] = {"script", "style", "noscript", "svg", "iframe", "form",
                                 "nav", "aside", "header", "footer", "template", "button",
                                 "select", "textarea", "canvas", "video", "audio", "head",
                                 "figure", "object", "embed", "map", "dialog"};
    for (auto d : drop) if (strlen(d) == len && ieq(n, d, len)) return true;
    return false;
}

static bool isBlockTag(const char *n, size_t len) {
    static const char *blk[] = {"p", "div", "br", "hr", "h1", "h2", "h3", "h4", "h5", "h6",
                                "li", "ul", "ol", "blockquote", "pre", "section", "article",
                                "main", "tr", "td", "th", "dd", "dt", "dl", "figcaption",
                                "table", "body"};
    for (auto b : blk) if (strlen(b) == len && ieq(n, b, len)) return true;
    return false;
}

static bool isHeadingTag(const char *n, size_t len) {
    return len == 2 && tolower((unsigned char)n[0]) == 'h' && n[1] >= '1' && n[1] <= '6';
}

// Walk forward past a tag, respecting quoted attribute values.
static size_t endOfTag(const char *h, size_t len, size_t i) {
    char quote = 0;
    for (size_t k = i; k < len; k++) {
        char c = h[k];
        if (quote) { if (c == quote) quote = 0; }
        else if (c == '"' || c == '\'') quote = c;
        else if (c == '>') return k + 1;
    }
    return len;
}

static size_t findClose(const char *h, size_t len, const char *name, size_t nameLen, size_t from) {
    for (size_t k = from; k + nameLen + 3 <= len; k++) {
        if (h[k] != '<' || h[k + 1] != '/') continue;
        if (!ieq(h + k + 2, name, nameLen)) continue;
        char after = (k + 2 + nameLen < len) ? h[k + 2 + nameLen] : '>';
        if (after == '>' || isspace((unsigned char)after)) return endOfTag(h, len, k);
    }
    return len;
}

// Locate <tag>..</tag> honouring nesting; returns false if absent.
static bool findBlock(const char *h, size_t len, const char *name,
                      size_t &bStart, size_t &bEnd) {
    size_t nameLen = strlen(name);
    for (size_t i = 0; i + nameLen + 1 < len; i++) {
        if (h[i] != '<' || !ieq(h + i + 1, name, nameLen)) continue;
        char after = h[i + 1 + nameLen];
        if (after != '>' && !isspace((unsigned char)after)) continue;
        size_t open = endOfTag(h, len, i);
        int depth = 1;
        size_t k = open;
        while (k < len && depth > 0) {
            if (h[k] == '<') {
                if (h[k + 1] == '/' && ieq(h + k + 2, name, nameLen)) depth--;
                else if (ieq(h + k + 1, name, nameLen)) depth++;
                k = endOfTag(h, len, k);
            } else k++;
        }
        bStart = open;
        bEnd = k;
        return true;
    }
    return false;
}

static String extractTitle(const char *h, size_t len) {
    size_t s, e;
    if (!findBlock(h, len, "title", s, e)) return "";
    String t;
    for (size_t i = s; i < e && i < s + 400; i++) {
        if (h[i] == '<') break;
        t += h[i];
    }
    return htmlToText(t);
}

static void walk(const char *h, size_t from, size_t to, bool latin1,
                 std::vector<Para> &out) {
    String cur;
    bool curHeading = false;
    bool pendingBullet = false;

    auto flush = [&]() {
        if (cur.length()) {
            String t = latin1 ? latin1ToUtf8(cur) : cur;
            t = htmlToText(t);
            if (t.length()) {
                Para p;
                p.text = pendingBullet ? String("- ") + t : t;
                p.heading = curHeading;
                out.push_back(p);
            }
        }
        cur = "";
        curHeading = false;
        pendingBullet = false;
    };

    size_t i = from;
    while (i < to) {
        char c = h[i];
        if (c != '<') {
            // collapse all runs of whitespace to a single space
            if (isspace((unsigned char)c)) {
                if (cur.length() && cur[cur.length() - 1] != ' ') cur += ' ';
            } else {
                if (cur.length() < 6000) cur += c;
            }
            i++;
            continue;
        }
        if (i + 3 < to && h[i + 1] == '!' && h[i + 2] == '-' && h[i + 3] == '-') {
            size_t k = i + 4;
            while (k + 2 < to && !(h[k] == '-' && h[k + 1] == '-' && h[k + 2] == '>')) k++;
            i = (k + 3 < to) ? k + 3 : to;
            continue;
        }
        if (h[i + 1] == '!' || h[i + 1] == '?') { i = endOfTag(h, to, i); continue; }

        bool closing = (h[i + 1] == '/');
        const char *name = h + i + 1 + (closing ? 1 : 0);
        size_t nameLen = 0;
        while (name + nameLen < h + to && (isalnum((unsigned char)name[nameLen]))) nameLen++;
        if (nameLen == 0) { i = endOfTag(h, to, i); continue; }

        if (!closing && isDropTag(name, nameLen)) {
            size_t after = endOfTag(h, to, i);
            // a self-closing drop tag has nothing to skip
            if (after >= 2 && h[after - 2] == '/') { i = after; continue; }
            flush();
            i = findClose(h, to, name, nameLen, after);
            continue;
        }
        if (isBlockTag(name, nameLen)) {
            flush();
            if (!closing && isHeadingTag(name, nameLen)) curHeading = true;
            if (!closing && nameLen == 2 && ieq(name, "li", 2)) pendingBullet = true;
        }
        i = endOfTag(h, to, i);
    }
    flush();
}

bool fetch(const String &url, String &title, String &body, String &err) {
    title = "";
    body = "";
    char *buf = nullptr;
    size_t len = 0;
    String ctype;
    if (!http::get(url, &buf, &len, &ctype, err)) return false;

    struct Guard { char *p; ~Guard() { http::freeBody(p); } } guard{buf};

    String ct = ctype;
    ct.toLowerCase();
    bool latin1 = ct.indexOf("iso-8859-1") >= 0 || ct.indexOf("windows-1252") >= 0;

    if (ct.indexOf("text/plain") >= 0) {
        String t;
        for (size_t i = 0; i < len && t.length() < 60000; i++) t += buf[i];
        body = utf8ToAscii(latin1 ? latin1ToUtf8(t) : t);
        title = url;
        return body.length() > 0;
    }
    if (ct.length() && ct.indexOf("html") < 0 && ct.indexOf("xml") < 0) {
        err = String("Not a readable page (") + ctype + ")";
        return false;
    }

    // charset can also be declared in a meta tag
    if (!latin1) {
        size_t scan = len < 2048 ? len : 2048;
        for (size_t i = 0; i + 8 < scan; i++)
            if (ieq(buf + i, "charset", 7)) {
                if (ieq(buf + i, "charset=iso-8859-1", 18) ||
                    ieq(buf + i, "charset=\"iso-8859-1", 19) ||
                    ieq(buf + i, "charset=windows-1252", 20) ||
                    ieq(buf + i, "charset=\"windows-1252", 21)) latin1 = true;
                break;
            }
    }

    title = extractTitle(buf, len);

    // Prefer an explicit content container when the page offers one.
    size_t from = 0, to = len;
    size_t s, e;
    if (findBlock(buf, len, "article", s, e) && e - s > 400)      { from = s; to = e; }
    else if (findBlock(buf, len, "main", s, e) && e - s > 400)    { from = s; to = e; }
    else if (findBlock(buf, len, "body", s, e))                   { from = s; to = e; }

    std::vector<Para> paras;
    walk(buf, from, to, latin1, paras);

    auto assemble = [&](int minLen) {
        String outBody;
        String prev;
        int kept = 0;
        for (auto &p : paras) {
            if (kept > 400 || outBody.length() > MAX_ARTICLE_CHARS) break;
            int need = p.heading ? 2 : minLen;
            if ((int)p.text.length() < need) continue;
            if (p.text == prev) continue;
            prev = p.text;
            if (outBody.length()) outBody += "\n\n";
            outBody += p.heading ? String("# ") + p.text : p.text;
            kept++;
        }
        return outBody;
    };

    body = assemble(30);
    if (body.length() < 250) body = assemble(12);      // short or oddly-structured page
    if (body.length() < 60) {
        err = "No readable text found (the page may need JavaScript)";
        return false;
    }
    if (body.length() > MAX_ARTICLE_CHARS)
        body = body.substring(0, MAX_ARTICLE_CHARS) + "\n\n[Article truncated to fit memory.]";
    if (title.length() == 0) title = domainOf(url);
    return true;
}

}  // namespace reader
