/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "PromptNormalizer.h"

namespace RepentePd {

namespace {

bool isWordChar(juce::juce_wchar c)
{
    return juce::CharacterFunctions::isLetterOrDigit(c) || c == '_';
}

// Case-insensitive whole-word search. The boundary check is what keeps "pd"
// from matching inside "update" or "speed".
bool containsWord(juce::String const& haystack, juce::String const& word)
{
    auto const lower  = haystack.toLowerCase();
    auto const needle = word.toLowerCase();
    if (needle.isEmpty()) return false;

    for (int from = 0;;) {
        int const at = lower.indexOf(from, needle);
        if (at < 0) return false;

        int const end      = at + needle.length();
        bool const leftOk  = at == 0 || !isWordChar(lower[at - 1]);
        bool const rightOk = end >= lower.length() || !isWordChar(lower[end]);
        if (leftOk && rightOk) return true;

        from = at + 1;
    }
}

// `lower` is expected to be already lowercased and trimmed.
bool startsWithWord(juce::String const& lower, juce::String const& word)
{
    if (!lower.startsWith(word)) return false;
    int const end = word.length();
    return end >= lower.length() || !isWordChar(lower[end]);
}

juce::String upperFirst(juce::String const& s)
{
    if (s.isEmpty()) return s;
    return s.substring(0, 1).toUpperCase() + s.substring(1);
}

// Lowercases the first letter for text that becomes mid-sentence — but leaves
// acronyms alone, so "FM synth" and "SC Saw" survive intact.
juce::String lowerFirst(juce::String const& s)
{
    if (s.isEmpty()) return s;
    if (s.length() >= 2 && juce::CharacterFunctions::isUpperCase(s[0])
                        && juce::CharacterFunctions::isUpperCase(s[1]))
        return s;
    return s.substring(0, 1).toLowerCase() + s.substring(1);
}

bool isTerminator(juce::juce_wchar c) { return c == '.' || c == '?' || c == '!'; }

juce::String ensurePeriod(juce::String const& s)
{
    auto const t = s.trimEnd();
    if (t.isEmpty()) return t;
    return isTerminator(t.getLastCharacter()) ? t : t + ".";
}

juce::String createForm(juce::String const& trimmed)
{
    // Longest phrases first — "i want you to" has to win over "i want".
    static char const* verbPhrases[] = {
        "can you write", "can you create", "can you make", "can you build",
        "can you generate", "can you implement", "i want you to",
        "i want", "i need", "give me", "please",
        "write", "create", "make", "build", "generate", "implement", "do",
    };

    auto rest = trimmed;
    for (bool stripped = true; stripped;) {
        stripped = false;
        auto const lower = rest.toLowerCase();
        for (auto const* v : verbPhrases) {
            if (startsWithWord(lower, v)) {
                rest     = rest.substring(juce::String(v).length()).trim();
                stripped = true;
                break;
            }
        }
    }
    // "write me a sampler" — drop the indirect object the verb left behind.
    for (auto const* pronoun : { "me", "us" }) {
        if (startsWithWord(rest.toLowerCase(), pronoun)) {
            rest = rest.substring(2).trim();
            break;
        }
    }
    if (rest.isEmpty()) rest = trimmed;

    auto const lower  = rest.toLowerCase();
    bool const clause = startsWithWord(lower, "that") || startsWithWord(lower, "which")
                     || startsWithWord(lower, "to");

    auto const body = lowerFirst(rest);
    return ensurePeriod(clause ? "Create a Pure Data patch " + body
                               : "Write a Pure Data patch for " + body);
}

juce::String modifyForm(juce::String const& trimmed)
{
    auto const lower  = trimmed.toLowerCase();
    bool const isAdd  = startsWithWord(lower, "add");
    bool const isIns  = startsWithWord(lower, "insert");

    // "add reverb" → "Add reverb to the Pure Data patch." Skipped when the
    // sentence already has its own "to ..." target, which that form would garble.
    if ((isAdd || isIns) && !lower.contains(" to ")) {
        auto const rest = trimmed.substring(isAdd ? 3 : 6).trim();
        if (rest.isNotEmpty())
            return ensurePeriod("Add " + lowerFirst(rest) + " to the Pure Data patch");
    }
    return ensurePeriod("In this Pure Data patch, " + lowerFirst(trimmed));
}

juce::String analyzeForm(juce::String const& trimmed)
{
    // Cheapest possible touch: qualify the noun the user already wrote.
    if (containsWord(trimmed, "this patch"))
        return upperFirst(trimmed.replace("this patch", "this Pure Data patch", true));
    if (containsWord(trimmed, "the patch"))
        return upperFirst(trimmed.replace("the patch", "the Pure Data patch", true));

    return ensurePeriod("In Pure Data, " + lowerFirst(trimmed));
}

juce::String convertForm(juce::String const& trimmed)
{
    auto body = trimmed;
    juce::String tail;
    if (isTerminator(body.getLastCharacter())) {
        tail = juce::String::charToString(body.getLastCharacter());
        body = body.dropLastCharacters(1).trimEnd();
    }
    return upperFirst(body) + " to Pure Data" + (tail.isEmpty() ? "." : tail);
}

} // namespace

PromptNormalizer::Intent PromptNormalizer::classify(juce::String const& prompt)
{
    auto const trimmed = prompt.trim();
    if (trimmed.isEmpty()) return Intent::AlreadyQualified; // nothing to rewrite

    static char const* qualifiers[] = { "pure data", "puredata", "plugdata", "pd" };
    for (auto const* q : qualifiers)
        if (containsWord(trimmed, q)) return Intent::AlreadyQualified;

    auto const lower = trimmed.toLowerCase();

    static char const* convertVerbs[] = { "convert", "translate", "port" };
    for (auto const* v : convertVerbs)
        if (containsWord(lower, v)) return Intent::Convert;

    // A trailing "?" catches questions that don't open with a wh-word — without
    // it they fall through to Create and get mangled into "Write a patch for...".
    if (trimmed.endsWithChar('?')) return Intent::Analyze;

    static char const* analyzeStarts[] = { "what", "how", "why", "explain",
                                           "describe", "analyze", "analyse",
                                           "tell me about" };
    for (auto const* a : analyzeStarts)
        if (startsWithWord(lower, a)) return Intent::Analyze;

    static char const* modifyStarts[] = { "add", "insert", "append", "connect",
                                          "remove", "delete", "change", "replace",
                                          "modify", "set" };
    for (auto const* m : modifyStarts)
        if (startsWithWord(lower, m)) return Intent::Modify;

    return Intent::Create;
}

juce::String PromptNormalizer::normalize(juce::String const& prompt)
{
    auto const trimmed = prompt.trim();
    if (trimmed.isEmpty()) return prompt;

    switch (classify(trimmed)) {
        case Intent::AlreadyQualified: return prompt; // byte-identical passthrough
        case Intent::Convert:          return convertForm(trimmed);
        case Intent::Analyze:          return analyzeForm(trimmed);
        case Intent::Modify:           return modifyForm(trimmed);
        case Intent::Create:
        default:                       return createForm(trimmed);
    }
}

} // namespace RepentePd
