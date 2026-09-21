#include "app.h"

const char *kFeedName[FEED_COUNT] = {"Top", "New", "Best", "Ask HN", "Show HN", "Jobs"};
const char *kFeedDesc[FEED_COUNT] = {
    "The front page right now",
    "Newest submissions, most recent first",
    "Highest scoring of the last 24 hours",
    "Questions put to the community",
    "Things people have made",
    "Who's hiring and job posts",
};

// --------------------------------------------------------------------------
// UTF-8 -> ASCII.  The bundled Roboto fonts only carry 0x20..0x7E, so anything
// outside that range has to be folded down to something printable.
// --------------------------------------------------------------------------
static const char *latin1Map(uint32_t c) {
    switch (c) {
        case 0x00C0: case 0x00C1: case 0x00C2: case 0x00C3: case 0x00C4: case 0x00C5: return "A";
        case 0x00C6: return "AE";
        case 0x00C7: return "C";
        case 0x00C8: case 0x00C9: case 0x00CA: case 0x00CB: return "E";
        case 0x00CC: case 0x00CD: case 0x00CE: case 0x00CF: return "I";
        case 0x00D0: return "D";
        case 0x00D1: return "N";
        case 0x00D2: case 0x00D3: case 0x00D4: case 0x00D5: case 0x00D6: case 0x00D8: return "O";
        case 0x00D9: case 0x00DA: case 0x00DB: case 0x00DC: return "U";
        case 0x00DD: return "Y";
        case 0x00DE: return "Th";
        case 0x00DF: return "ss";
        case 0x00E0: case 0x00E1: case 0x00E2: case 0x00E3: case 0x00E4: case 0x00E5: return "a";
        case 0x00E6: return "ae";
        case 0x00E7: return "c";
        case 0x00E8: case 0x00E9: case 0x00EA: case 0x00EB: return "e";
        case 0x00EC: case 0x00ED: case 0x00EE: case 0x00EF: return "i";
        case 0x00F0: return "d";
        case 0x00F1: return "n";
        case 0x00F2: case 0x00F3: case 0x00F4: case 0x00F5: case 0x00F6: case 0x00F8: return "o";
        case 0x00F9: case 0x00FA: case 0x00FB: case 0x00FC: return "u";
        case 0x00FD: case 0x00FF: return "y";
        case 0x00FE: return "th";
        default: return nullptr;
    }
}

static const char *punctMap(uint32_t c) {
    switch (c) {
        case 0x00A0: case 0x2002: case 0x2003: case 0x2009: case 0x202F: return " ";
        case 0x2018: case 0x2019: case 0x201A: case 0x201B: case 0x2032: return "'";
        case 0x201C: case 0x201D: case 0x201E: case 0x201F: case 0x2033: return "\"";
        case 0x00AB: return "<<";
        case 0x00BB: return ">>";
        case 0x2010: case 0x2011: case 0x2012: case 0x2013: case 0x2014:
        case 0x2015: case 0x2212: return "-";
        case 0x2026: return "...";
        case 0x2022: case 0x00B7: case 0x2027: return "-";
        case 0x2190: return "<-";
        case 0x2192: return "->";
        case 0x00D7: return "x";
        case 0x00B0: return "deg";
        case 0x00A9: return "(c)";
        case 0x00AE: return "(R)";
        case 0x2122: return "(TM)";
        case 0x00BD: return "1/2";
        case 0x00BC: return "1/4";
        case 0x00BE: return "3/4";
        case 0x20AC: return "EUR";
        case 0x00A3: return "GBP";
        case 0x00A5: return "JPY";
        case 0x2264: return "<=";
        case 0x2265: return ">=";
        case 0x2260: return "!=";
        default: return nullptr;
    }
}

String utf8ToAscii(const String &in) {
    String out;
    out.reserve(in.length() + 8);
    const uint8_t *p = (const uint8_t *)in.c_str();
    size_t n = in.length(), i = 0;
    while (i < n) {
        uint8_t b = p[i];
        uint32_t cp;
        int len;
        if (b < 0x80)            { cp = b;            len = 1; }
        else if ((b & 0xE0) == 0xC0) { cp = b & 0x1F; len = 2; }
        else if ((b & 0xF0) == 0xE0) { cp = b & 0x0F; len = 3; }
        else if ((b & 0xF8) == 0xF0) { cp = b & 0x07; len = 4; }
        else { i++; continue; }                       // stray continuation byte
        if (i + len > n) break;
        for (int k = 1; k < len; k++) {
            if ((p[i + k] & 0xC0) != 0x80) { cp = 0xFFFD; break; }
            cp = (cp << 6) | (p[i + k] & 0x3F);
        }
        i += len;

        if (cp == '\t') { out += "    "; continue; }
        if (cp == '\r') continue;
        if (cp == '\n') { out += '\n'; continue; }
        if (cp >= 0x20 && cp < 0x7F) { out += (char)cp; continue; }

        const char *m = punctMap(cp);
        if (!m) m = latin1Map(cp);
        if (m) { out += m; continue; }
        // Latin Extended-A: strip the diacritic by folding onto the base letter
        if (cp >= 0x0100 && cp <= 0x017F) {
            static const char *ext =
                "AaAaAaCcCcCcCcDdDdEeEeEeEeEeGgGgGgGgHhHhIiIiIiIiIiJjKkkLlLlLlLlLl"
                "NnNnNnnNnOoOoOoRrRrRrSsSsSsSsTtTtTtUuUuUuUuUuUuWwYyYZzZzZzs";
            size_t idx = cp - 0x0100;
            if (idx < strlen(ext)) { out += ext[idx]; continue; }
        }
        if (cp > 0x7F) out += '?';
    }
    return out;
}

// --------------------------------------------------------------------------
// HTML entities
// --------------------------------------------------------------------------
static void appendCodepoint(String &out, uint32_t cp) {
    if (cp < 0x80) { out += (char)cp; return; }
    // re-encode as UTF-8; utf8ToAscii folds it afterwards
    if (cp < 0x800) {
        out += (char)(0xC0 | (cp >> 6));
        out += (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += (char)(0xE0 | (cp >> 12));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    } else {
        out += (char)(0xF0 | (cp >> 18));
        out += (char)(0x80 | ((cp >> 12) & 0x3F));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    }
}

static String decodeEntities(const String &in) {
    String out;
    out.reserve(in.length());
    size_t i = 0, n = in.length();
    while (i < n) {
        char c = in[i];
        if (c != '&') { out += c; i++; continue; }
        int semi = in.indexOf(';', i + 1);
        if (semi < 0 || semi - (int)i > 10) { out += c; i++; continue; }
        String e = in.substring(i + 1, semi);
        if      (e == "amp")  out += '&';
        else if (e == "lt")   out += '<';
        else if (e == "gt")   out += '>';
        else if (e == "quot") out += '"';
        else if (e == "apos") out += '\'';
        else if (e == "nbsp") out += ' ';
        else if (e.startsWith("#x") || e.startsWith("#X"))
            appendCodepoint(out, (uint32_t)strtoul(e.c_str() + 2, nullptr, 16));
        else if (e.startsWith("#"))
            appendCodepoint(out, (uint32_t)strtoul(e.c_str() + 1, nullptr, 10));
        else { out += c; i++; continue; }
        i = semi + 1;
    }
    return out;
}

// HN comment bodies are small HTML fragments: <p>, <i>, <a href>, <pre><code>.
String htmlToText(const String &in) {
    String out;
    out.reserve(in.length());
    size_t i = 0, n = in.length();
    while (i < n) {
        char c = in[i];
        if (c == '<') {
            int close = in.indexOf('>', i + 1);
            if (close < 0) break;
            String tag = in.substring(i + 1, close);
            tag.toLowerCase();
            tag.trim();
            if (tag.startsWith("p"))        out += "\n\n";
            else if (tag.startsWith("br"))  out += "\n";
            else if (tag.startsWith("/p"))  { /* nothing */ }
            else if (tag.startsWith("pre") || tag.startsWith("/pre")) out += "\n";
            i = close + 1;
            continue;
        }
        out += c;
        i++;
    }
    out = decodeEntities(out);
    out = utf8ToAscii(out);
    // collapse runs of blank lines / trailing space
    String tidy;
    tidy.reserve(out.length());
    int nl = 0;
    for (size_t k = 0; k < out.length(); k++) {
        char ch = out[k];
        if (ch == '\n') { nl++; if (nl > 2) continue; }
        else nl = 0;
        tidy += ch;
    }
    tidy.trim();
    return tidy;
}

String timeAgo(uint32_t t) {
    time_t now = time(nullptr);
    if (now < 1600000000 || t == 0) return "";        // clock not set yet
    long d = (long)now - (long)t;
    if (d < 0) d = 0;
    char buf[24];
    if (d < 3600)        snprintf(buf, sizeof(buf), "%ldm", d / 60);
    else if (d < 86400)  snprintf(buf, sizeof(buf), "%ldh", d / 3600);
    else if (d < 86400L * 30) snprintf(buf, sizeof(buf), "%ldd", d / 86400);
    else                 snprintf(buf, sizeof(buf), "%ldmo", d / (86400L * 30));
    return String(buf);
}

String domainOf(const String &url) {
    if (url.length() == 0) return "";
    int s = url.indexOf("://");
    s = (s < 0) ? 0 : s + 3;
    int e = url.indexOf('/', s);
    String h = (e < 0) ? url.substring(s) : url.substring(s, e);
    if (h.startsWith("www.")) h = h.substring(4);
    int q = h.indexOf(':');
    if (q > 0) h = h.substring(0, q);
    return h;
}

String clampStr(const String &s, size_t maxLen) {
    if (s.length() <= maxLen) return s;
    return s.substring(0, maxLen - 1) + "~";
}

// Pages that declare latin-1 / windows-1252 arrive as raw high bytes; promote
// them to UTF-8 so the normal folding path can deal with them.
String latin1ToUtf8(const String &in) {
    String out;
    out.reserve(in.length() + 8);
    for (size_t i = 0; i < in.length(); i++) {
        uint8_t b = (uint8_t)in[i];
        if (b < 0x80) { out += (char)b; continue; }
        // windows-1252 punctuation lives in the 0x80..0x9F hole
        static const uint16_t cp1252[32] = {
            0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
            0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
            0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
            0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178};
        uint32_t cp = (b >= 0x80 && b <= 0x9F) ? cp1252[b - 0x80] : b;
        if (cp < 0x800) {
            out += (char)(0xC0 | (cp >> 6));
            out += (char)(0x80 | (cp & 0x3F));
        } else {
            out += (char)(0xE0 | (cp >> 12));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
    }
    return out;
}
