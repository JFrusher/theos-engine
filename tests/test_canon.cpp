#include <catch2/catch_test_macros.hpp>
#include "theos/canon.hpp"

using namespace Theos;
using AST::Canon;

TEST_CASE("Canon sizes", "[canon]") {
    REQUIRE(Scripture::book_count(Canon::Protestant) == 66);
    REQUIRE(Scripture::book_count(Canon::Catholic) == 73);
    REQUIRE(Scripture::book_count(Canon::Orthodox) == 77);
}

TEST_CASE("Resolves references per canon", "[canon]") {
    auto ps = Scripture::resolve(Canon::Protestant, "Psalms 102:25-27");
    REQUIRE(ps);
    REQUIRE(Scripture::book_name(Canon::Protestant, ps->book) == "Psalms");
    REQUIRE(ps->book == 19);
    REQUIRE(ps->verse == 25);
    REQUIRE(ps->end_verse == 27);

    auto cat = Scripture::resolve(Canon::Catholic, "Psalm 102:25");
    REQUIRE(cat);
    REQUIRE(Scripture::book_name(Canon::Catholic, cat->book) == "Psalms");
    REQUIRE(cat->book == 23);   // after Tobit, Judith, 1-2 Maccabees

    REQUIRE(Scripture::resolve(Canon::Protestant, "1 corinthians 15:3-8"));
    REQUIRE(Scripture::resolve(Canon::Protestant, "1Corinthians 15"));
    REQUIRE(Scripture::resolve(Canon::Orthodox, "Psalms 151:1"));
    REQUIRE(Scripture::resolve(Canon::Catholic, "Wisdom 13:1-9"));
}

TEST_CASE("Rejects unknown books and out-of-range chapters", "[canon]") {
    REQUIRE(Scripture::resolve(Canon::Protestant, "Wisdom 13:1").error() == "unknown book 'Wisdom' in the protestant canon");
    REQUIRE_FALSE(Scripture::resolve(Canon::Protestant, "Psalms 151:1"));
    REQUIRE_FALSE(Scripture::resolve(Canon::Protestant, "Jude 2:1"));
    REQUIRE_FALSE(Scripture::resolve(Canon::Protestant, "John 3:16-10"));
    REQUIRE_FALSE(Scripture::resolve(Canon::Protestant, "John"));
    REQUIRE_FALSE(Scripture::resolve(Canon::Protestant, "John x:1"));
}

TEST_CASE("Quran surah and ayah bounds", "[canon]") {
    auto kursi = Scripture::resolve(Canon::Quran, "2:255");
    REQUIRE(kursi);
    REQUIRE(kursi->chapter == 2);
    REQUIRE(Scripture::format(*kursi) == "2:255");
    REQUIRE_FALSE(Scripture::resolve(Canon::Quran, "1:8"));
    REQUIRE_FALSE(Scripture::resolve(Canon::Quran, "115:1"));
    REQUIRE(Scripture::resolve(Canon::Quran, "112:1-4"));
}

TEST_CASE("Formats canonical citations", "[canon]") {
    REQUIRE(Scripture::format(*Scripture::resolve(Canon::Protestant, "song of songs 2:1")) == "Song of Solomon 2:1");
    REQUIRE(Scripture::format(*Scripture::resolve(Canon::Protestant, "John 1:1-2:3")) == "John 1:1-2:3");
    REQUIRE(Scripture::format(*Scripture::resolve(Canon::Protestant, "Jude 1")) == "Jude 1");
}
