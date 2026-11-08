#include "numerick/application_matcher.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using numerick::matchApplicationNames;

static void testExactMatch() {
    const std::vector<std::wstring> candidates = {
        L"Notepad",
        L"Blender-Launcher",
        L"Spotify"
    };

    const auto matches = matchApplicationNames(L"Notepad", candidates);

    assert(matches.size() == 1);
    assert(matches[0] == 0);
}

static void testCaseInsensitiveExactMatch() {
    const std::vector<std::wstring> candidates = {
        L"Notepad"
    };

    const auto matches = matchApplicationNames(L"NOTEPAD", candidates);

    assert(matches.size() == 1);
    assert(matches[0] == 0);
}

static void testFriendlyNameWithHyphen() {
    const std::vector<std::wstring> candidates = {
        L"Blender-Launcher"
    };

    const auto matches = matchApplicationNames(L"Blender", candidates);

    assert(matches.size() == 1);
    assert(matches[0] == 0);
}

static void testFriendlyNameWithUnderscore() {
    const std::vector<std::wstring> candidates = {
        L"Visual_Studio"
    };

    const auto matches = matchApplicationNames(L"Visual", candidates);

    assert(matches.size() == 1);
    assert(matches[0] == 0);
}

static void testFriendlyNameWithSpace() {
    const std::vector<std::wstring> candidates = {
        L"Microsoft Office"
    };

    const auto matches = matchApplicationNames(L"Microsoft", candidates);

    assert(matches.size() == 1);
    assert(matches[0] == 0);
}

static void testExactMatchHasPriority() {
    const std::vector<std::wstring> candidates = {
        L"Blender-Launcher",
        L"Blender Beta",
        L"Blender"
    };

    const auto matches = matchApplicationNames(L"Blender", candidates);

    assert(matches.size() == 1);
    assert(matches[0] == 2);
}

static void testAmbiguousFriendlyMatches() {
    const std::vector<std::wstring> candidates = {
        L"Blender-Launcher",
        L"Blender-Beta",
        L"Notepad"
    };

    const auto matches = matchApplicationNames(L"Blender", candidates);

    assert(matches.size() == 2);
    assert(matches[0] == 0);
    assert(matches[1] == 1);
}

static void testPartialWordsAreRejected() {
    const std::vector<std::wstring> candidates = {
        L"Notepad",
        L"BlenderLauncher",
        L"SpotifyApp"
    };

    assert(matchApplicationNames(L"Not", candidates).empty());
    assert(matchApplicationNames(L"Blender", candidates).empty());
    assert(matchApplicationNames(L"Spotify", candidates).empty());
}

static void testEmptyRequestIsRejected() {
    const std::vector<std::wstring> candidates = {
        L"Notepad",
        L"Blender-Launcher"
    };

    assert(matchApplicationNames(L"", candidates).empty());
}

int main() {
    testExactMatch();
    testCaseInsensitiveExactMatch();
    testFriendlyNameWithHyphen();
    testFriendlyNameWithUnderscore();
    testFriendlyNameWithSpace();
    testExactMatchHasPriority();
    testAmbiguousFriendlyMatches();
    testPartialWordsAreRejected();
    testEmptyRequestIsRejected();

    std::cout << "All application matcher tests passed.\n";
    return 0;
}